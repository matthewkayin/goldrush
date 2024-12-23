#include "network.h"

#include "logger.h"
#include "asserts.h"
#include "lcg.h"
#include "noise.h"
#include <cstring>
#include <enet/enet.h>
#include <queue>
#include <ctime>

struct network_state_t {
    ENetHost* host;
    ENetSocket scanner;
    NetworkStatus status;

    char client_username[NAME_BUFFER_SIZE];

    uint8_t player_id;
    player_t players[MAX_PLAYERS];
    ENetPeer* peers[MAX_PLAYERS];
    bool server_lobby_closed;

    std::queue<network_event_t> event_queue;
    std::vector<lobby_t> lobbies;
};
static network_state_t state;

// MESSAGE

enum MessageType {
    MESSAGE_GREET_SERVER,
    MESSAGE_INVALID_VERSION,
    MESSAGE_GAME_ALREADY_STARTED,
    MESSAGE_WELCOME,
    MESSAGE_NEW_PLAYER,
    MESSAGE_GREET,
    MESSAGE_READY,
    MESSAGE_NOT_READY,
    MESSAGE_MATCH_LOAD,
    MESSAGE_INPUT
};

struct message_greet_server_t {
    const uint8_t type = MESSAGE_GREET_SERVER;
    char username[NAME_BUFFER_SIZE];
    char app_version[sizeof(APP_VERSION)];
};

struct message_welcome_t {
    const uint8_t type = MESSAGE_WELCOME;
    uint8_t player_id;
    uint8_t padding[2];
    char server_username[NAME_BUFFER_SIZE];
};

struct message_new_player_t {
    const uint8_t type = MESSAGE_NEW_PLAYER;
    uint8_t padding;
    uint16_t port;
    char ip[NAME_BUFFER_SIZE];
};

struct message_greet_t {
    const uint8_t type = MESSAGE_GREET;
    uint8_t player_id;
    uint8_t padding[2];
    player_t player;
};

bool network_init() {
    if (enet_initialize() != 0) {
        log_error("Unable to initialize enet.");
        return false;
    }

    state.status = NETWORK_STATUS_OFFLINE;

    return true;
}

void network_quit() {
    enet_deinitialize();
}

void network_disconnect() {
    if (state.status == NETWORK_STATUS_OFFLINE) {
        log_warn("network_disconnect() called while offline.");
        return;
    }

    network_scanner_destroy();

    for (uint8_t peer_id = 0; peer_id < state.host->peerCount; peer_id++) {
        if (state.host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED) {
            if (state.status == NETWORK_STATUS_SERVER || state.status == NETWORK_STATUS_CONNECTED) {
                enet_peer_disconnect(&state.host->peers[peer_id], 0);
            } else {
                enet_peer_reset(&state.host->peers[peer_id]);
            }
        }
    }
    enet_host_flush(state.host);
    if (state.status == NETWORK_STATUS_SERVER || state.status == NETWORK_STATUS_CONNECTING || state.status == NETWORK_STATUS_DISCONNECTING) {
        enet_host_destroy(state.host);
        state.host = NULL;
        memset(state.peers, 0, sizeof(state.peers));
        state.status = NETWORK_STATUS_OFFLINE;
    } else if (state.status == NETWORK_STATUS_CONNECTED) {
        log_info("Client attempted gentle disconnect...");
        state.status = NETWORK_STATUS_DISCONNECTING;
    }
}

bool network_scanner_create() {
    state.scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (state.scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create scanner socket.");
        return false;
    }
    enet_socket_set_option(state.scanner, ENET_SOCKOPT_BROADCAST, 1);

    return true;
}

void network_scanner_search() {
    state.lobbies.clear();

    ENetAddress scan_address;
    scan_address.host = ENET_HOST_BROADCAST;
    scan_address.port = SCANNER_PORT;

    char buffer = 3;
    ENetBuffer send_buffer;
    send_buffer.data = &buffer;
    send_buffer.dataLength = 1;
    if (enet_socket_send(state.scanner, &scan_address, &send_buffer, 1) != 1) {
        log_error("Failed to scan for LAN servers.");
    }
}

void network_scanner_destroy() {
    if (state.scanner != ENET_SOCKET_NULL) {
        enet_socket_shutdown(state.scanner, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(state.scanner);
        state.scanner = ENET_SOCKET_NULL;
    }
}

bool network_host_create() {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = BASE_PORT;

    state.host = NULL;
    while (state.host == NULL && address.port < BASE_PORT + MAX_PLAYERS) {
        state.host = enet_host_create(&address, MAX_PLAYERS - 1, 1, 0, 0);
        if (state.host == NULL) {
            address.port++;
        }
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state.players[player_id] = (player_t) {
            .status = PLAYER_STATUS_NONE,
            .name = ""
        };
        state.peers[player_id] = nullptr;
    }

    return state.host != NULL;
}

bool network_server_create(const char* username) {
    if (!network_host_create()) {
        log_error("Could not create enet host.");
        return false;
    }

    network_scanner_destroy();

    state.scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (state.scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create scanner socket.");
        return false;
    }
    enet_socket_set_option(state.scanner, ENET_SOCKOPT_REUSEADDR, 1);
    ENetAddress scanner_address;
    scanner_address.host = ENET_HOST_ANY;
    scanner_address.port = SCANNER_PORT;
    if (enet_socket_bind(state.scanner, &scanner_address) != 0) {
        log_error("Failed to bind scanner socket.");
        return false;
    }

    state.status = NETWORK_STATUS_SERVER;

    state.player_id = 0;
    state.players[0].status = PLAYER_STATUS_HOST;
    strncpy(state.players[0].name, username, MAX_USERNAME_LENGTH + 1);

    log_info("Created server.");
    return true;
}

bool network_client_create(const char* username, const char* server_ip, uint16_t server_port) {
    log_info("Connecting to %s:%u", server_ip, server_port);
    if (!network_host_create()) {
        log_error("Could not create enet host.");
        return false;
    }

    ENetAddress server_address;
    server_address.port = server_port;
    enet_address_set_host_ip(&server_address, server_ip);

    ENetPeer* server_peer = enet_host_connect(state.host, &server_address, 1, 0);
    if (server_peer == NULL) {
        enet_host_destroy(state.host);
        log_error("No peers available for initiating an enet connection.");
        return false;
    }

    memset(state.players, 0, sizeof(state.players));
    memset(state.peers, 0, sizeof(state.peers));

    strncpy(state.client_username, username, MAX_USERNAME_LENGTH + 1);
    state.status = NETWORK_STATUS_CONNECTING;
    
    return true;
}

// Getters

bool network_is_server() {
    return state.status == NETWORK_STATUS_SERVER;
}

NetworkStatus network_get_status() {
    return state.status;
}

const player_t& network_get_player(uint8_t player_id) {
    return state.players[player_id];
}

uint8_t network_get_player_id() {
    return state.player_id;
}

bool network_are_all_players_ready() {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status == PLAYER_STATUS_NOT_READY) {
            return false;
        }
    }

    return true;
}

void network_toggle_ready() {
    state.players[state.player_id].status = state.players[state.player_id].status == PLAYER_STATUS_NOT_READY ? PLAYER_STATUS_READY : PLAYER_STATUS_NOT_READY;
    uint8_t message = state.players[state.player_id].status == PLAYER_STATUS_READY ? MESSAGE_READY : MESSAGE_NOT_READY;
    ENetPacket* packet = enet_packet_create(&message, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_begin_loading_match() {
    // Set all players to NOT_READY so that they can re-ready themselves once they enter the match
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status != PLAYER_STATUS_NONE) {
            state.players[player_id].status = PLAYER_STATUS_NOT_READY;
        }
    }

    state.status = NETWORK_STATUS_CONNECTED;

    // Set LCG seed
    #ifdef GOLD_RAND_SEED
        int32_t lcg_seed = GOLD_RAND_SEED;
    #else
        int32_t lcg_seed = (int32_t)time(NULL);
    #endif
    lcg_srand(lcg_seed);
    log_trace("Host: set random seed to %i", lcg_seed);

    // Generate noise for map generation
    uint64_t noise_seed = (uint64_t)lcg_seed;
    uint32_t map_width = 128;
    uint32_t map_height = 128;
    noise_generate(noise_seed, map_width, map_height);
    log_trace("Set noise random seed to %u", noise_seed);

    // Build message
    // Message size is 1 byte for type, 4 bytes for LCG seed, 8 bytes for map width / height, and the rest of the bytes are the generated noise values
    size_t message_size = 1 + 4 + 8 + (map_width * map_height * sizeof(int8_t));
    uint8_t* message = (uint8_t*)malloc(message_size);
    message[0] = MESSAGE_MATCH_LOAD;
    memcpy(message + 1, &lcg_seed, sizeof(int32_t));
    memcpy(message + 5, &map_width, sizeof(uint32_t));
    memcpy(message + 9, &map_width, sizeof(uint32_t));
    memcpy(message + 13, &noise.map[0], map_width * map_height * sizeof(int8_t));

    // Send the packet
    ENetPacket* packet = enet_packet_create(message, message_size, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);

    free(message);
}

void network_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    out_buffer[0] = MESSAGE_INPUT;
    ENetPacket* packet = enet_packet_create(out_buffer, out_buffer_length, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

const size_t network_get_lobby_count() {
    return state.lobbies.size();
}

const lobby_t& network_get_lobby(size_t index) {
    return state.lobbies[index];
}

// POLL EVENTS

void network_service() {
    if (state.scanner != ENET_SOCKET_NULL) {
        ENetSocketSet set;
        ENET_SOCKETSET_EMPTY(set);
        ENET_SOCKETSET_ADD(set, state.scanner);
        while (enet_socketset_select(state.scanner, &set, NULL, 0) > 0) {
            ENetAddress receive_address;
            ENetBuffer receive_buffer;
            char buffer[64];
            receive_buffer.data = &buffer;
            receive_buffer.dataLength = sizeof(buffer);
            if (enet_socket_receive(state.scanner, &receive_address, &receive_buffer, 1) <= 0) {
                continue;
            }

            if (state.status == NETWORK_STATUS_SERVER) {
                // Tell the client about this game
                lobby_info_t lobby_info;
                sprintf(lobby_info.name, "%s's Game", state.players[0].name);
                lobby_info.player_count = 0;
                lobby_info.port = state.host->address.port;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (state.players[player_id].status != PLAYER_STATUS_NONE) {
                        lobby_info.player_count++;
                    }
                }

                ENetBuffer response_buffer;
                response_buffer.data = &lobby_info;
                response_buffer.dataLength = sizeof(lobby_info_t);
                enet_socket_send(state.scanner, &receive_address, &response_buffer, 1);
            } else {
                lobby_info_t lobby_info;
                memcpy(&lobby_info, buffer, sizeof(lobby_info_t));
                lobby_t lobby;
                memcpy(&lobby.name, lobby_info.name, 32);
                lobby.player_count = lobby_info.player_count;
                lobby.port = lobby_info.port;
                enet_address_get_host_ip(&receive_address, lobby.ip, NAME_BUFFER_SIZE);
                state.lobbies.push_back(lobby);
            }
        }
    }

    ENetEvent event;
    while (state.status != NETWORK_STATUS_OFFLINE && enet_host_service(state.host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                if (state.status == NETWORK_STATUS_CONNECTING) {
                    // Client greets server
                    log_info("Connected to server. Sending greeting...");
                    message_greet_server_t greet;
                    strncpy(greet.username, state.client_username, MAX_USERNAME_LENGTH + 1);
                    strncpy(greet.app_version, APP_VERSION, sizeof(APP_VERSION));
                    ENetPacket* packet = enet_packet_create(&greet, sizeof(message_greet_server_t), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(&state.host->peers[0], 0, packet);
                    enet_host_flush(state.host);
                } else if (state.status != NETWORK_STATUS_SERVER) {
                    message_greet_t greet;
                    greet.player_id = state.player_id;
                    memcpy(&greet.player, &state.players[state.player_id], sizeof(player_t));
                    ENetPacket* packet = enet_packet_create(&greet, sizeof(message_greet_t), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);
                    enet_host_flush(state.host);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                if (state.status == NETWORK_STATUS_DISCONNECTING || state.status == NETWORK_STATUS_CONNECTING) {
                    if (state.status == NETWORK_STATUS_CONNECTING) {
                        state.event_queue.push((network_event_t) {
                            .type = NETWORK_EVENT_CONNECTION_FAILED
                        });
                    }

                    enet_host_destroy(state.host);
                    state.host = NULL;
                    state.status = NETWORK_STATUS_OFFLINE;
                } else {
                    uint8_t* player_id_ptr = (uint8_t*)event.peer->data;
                    if (player_id_ptr == NULL) {
                        log_warn("Unidentified player disconnected.");
                        break;
                    }
                    log_info("Player %u disconnected.", *player_id_ptr);

                    state.players[*player_id_ptr].status = PLAYER_STATUS_NONE;

                    state.event_queue.push((network_event_t) {
                        .type = NETWORK_EVENT_PLAYER_DISCONNECTED,
                        .player_disconnected = {
                            .player_id = *player_id_ptr
                        }
                    });
                }
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                bool message_is_empty_input = event.packet->dataLength == 2 && event.packet->data[0] == MESSAGE_INPUT && event.packet->data[1] == 0;
                if (!message_is_empty_input) {
                    log_info("Received message %b", event.packet->data, event.packet->dataLength);
                }
                network_handle_message(event.packet->data, event.packet->dataLength, event.peer->incomingPeerID);
                enet_packet_destroy(event.packet);
                break;
            }
            default:
                break;
        }
    }
}

void network_handle_message(uint8_t* data, size_t length, uint16_t incoming_peer_id) {
    uint8_t message_type = data[0];

    switch (message_type) {
        case MESSAGE_GREET_SERVER: {
            message_greet_server_t greet;
            memcpy(&greet, data, sizeof(message_greet_server_t));

            // I think that ENET will handle the lobby-full case

            // Check that we're not already in a game
            if (state.players[state.player_id].status != PLAYER_STATUS_HOST) {
                log_info("Client tried to connect while we are no longer in lobby. Rejecting...");
                uint8_t message = MESSAGE_GAME_ALREADY_STARTED;
                ENetPacket* packet = enet_packet_create(&message, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(&state.host->peers[incoming_peer_id], 0, packet);
                enet_host_flush(state.host);
                return;
            }

            // Check the client version
            if (strcmp(greet.app_version, APP_VERSION) != 0) {
                log_info("Client app version mismatch. Rejecting client...");
                uint8_t message = MESSAGE_INVALID_VERSION;
                ENetPacket* packet = enet_packet_create(&message, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(&state.host->peers[incoming_peer_id], 0, packet);
                enet_host_flush(state.host);
                return;
            } 

            uint8_t incoming_player_id;
            for (incoming_player_id = 0; incoming_player_id < MAX_PLAYERS; incoming_player_id++) {
                if (state.players[incoming_player_id].status == PLAYER_STATUS_NONE) {
                    strncpy(state.players[incoming_player_id].name, greet.username, MAX_USERNAME_LENGTH + 1);
                    state.players[incoming_player_id].status = PLAYER_STATUS_NOT_READY;
                    uint8_t* player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
                    *player_id_ptr = incoming_player_id;
                    state.host->peers[incoming_peer_id].data = player_id_ptr;
                    state.peers[incoming_player_id] = &state.host->peers[incoming_peer_id];
                    break;
                }
            }

            GOLD_ASSERT(incoming_player_id != MAX_PLAYERS);
            log_info("Client has greeted us and is now player %u", incoming_player_id);

            // Send the welcome packet
            message_welcome_t welcome;
            welcome.player_id = incoming_player_id;
            strncpy(welcome.server_username, state.players[0].name, MAX_USERNAME_LENGTH + 1);
            ENetPacket* welcome_packet = enet_packet_create(&welcome, sizeof(message_welcome_t), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(&state.host->peers[incoming_peer_id], 0, welcome_packet);
            log_trace("Sent welcome packet to player %u", incoming_player_id);

            // Tell the other players about this new one
            message_new_player_t new_player;
            enet_address_get_host_ip(&state.host->peers[incoming_peer_id].address, new_player.ip, NAME_BUFFER_SIZE);
            new_player.port = state.host->peers[incoming_peer_id].address.port;
            for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
                if (player_id == incoming_player_id || state.players[player_id].status == PLAYER_STATUS_NONE) {
                    continue;
                }
                ENetPacket* new_player_packet = enet_packet_create(&new_player, sizeof(message_new_player_t), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(state.peers[player_id], 0, new_player_packet);
                log_trace("Sent new player packet to player %u", player_id);
            }

            enet_host_flush(state.host);
            break;
        }
        case MESSAGE_INVALID_VERSION: {
            log_info("Client version does not match the server.");
            state.event_queue.push((network_event_t) {
                .type = NETWORK_EVENT_INVALID_VERSION
            });
            break;
        }
        case MESSAGE_GAME_ALREADY_STARTED: {
            log_info("Server rejected this client because the game already started.");
            state.event_queue.push((network_event_t) {
                .type = NETWORK_EVENT_GAME_ALREADY_STARTED
            });
            break;
        }
        case MESSAGE_WELCOME: {
            log_info("Joined lobby.");
            state.status = NETWORK_STATUS_CONNECTED;

            // Pull data from event into struct
            message_welcome_t welcome;
            memcpy(&welcome, data, sizeof(message_welcome_t));

            // Setup current player info
            state.player_id = welcome.player_id;
            strncpy(state.players[state.player_id].name, state.client_username, MAX_USERNAME_LENGTH + 1);
            state.players[state.player_id].status = PLAYER_STATUS_NOT_READY;

            // Setup host player info
            state.players[0].status = PLAYER_STATUS_HOST;
            strncpy(state.players[0].name, welcome.server_username, MAX_USERNAME_LENGTH + 1);
            state.peers[0] = &state.host->peers[incoming_peer_id];
            uint8_t* player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
            *player_id_ptr = 0;
            state.peers[0]->data = player_id_ptr;

            state.event_queue.push((network_event_t) {
                .type = NETWORK_EVENT_JOINED_LOBBY
            });
            break;
        }
        case MESSAGE_NEW_PLAYER: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            message_new_player_t new_player;
            memcpy(&new_player, data, sizeof(message_new_player_t));

            ENetAddress new_player_address;
            new_player_address.port = new_player.port;
            enet_address_set_host_ip(&new_player_address, new_player.ip);

            ENetPeer* new_player_peer = enet_host_connect(state.host, &new_player_address, 1, 0);
            if (new_player_peer == NULL) {
                log_error("Unable to connect to new player.");
                return;
            }
            break;
        }
        case MESSAGE_GREET: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            message_greet_t greet;
            memcpy(&greet, data, sizeof(message_greet_t));
            memcpy(&state.players[greet.player_id], &greet.player, sizeof(player_t));
            state.peers[greet.player_id] = &state.host->peers[incoming_peer_id];
            uint8_t* player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
            *player_id_ptr = greet.player_id;
            state.host->peers[incoming_peer_id].data = player_id_ptr;
            break;
        }
        case MESSAGE_READY: 
        case MESSAGE_NOT_READY: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_SERVER)) {
                return;
            }

            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;
            state.players[*player_id].status = message_type == MESSAGE_READY ? PLAYER_STATUS_READY : PLAYER_STATUS_NOT_READY;
            break;
        }
        case MESSAGE_MATCH_LOAD: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            int32_t lcg_seed;
            memcpy(&lcg_seed, data + 1, sizeof(int32_t));
            lcg_srand(lcg_seed);
            log_trace("Set random seed to %i", lcg_seed);

            memcpy(&noise.width, data + 5, sizeof(uint32_t));
            memcpy(&noise.height, data + 9, sizeof(uint32_t));
            noise.map = std::vector<int8_t>(noise.width * noise.height);
            memcpy(&noise.map[0], data + 13, noise.width * noise.height * sizeof(int8_t));

            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (state.players[player_id].status != PLAYER_STATUS_NONE) {
                    state.players[player_id].status = PLAYER_STATUS_NOT_READY;
                }
            }

            state.event_queue.push((network_event_t) {
                .type = NETWORK_EVENT_MATCH_LOAD
            });
            break;
        }
        case MESSAGE_INPUT: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                log_trace("network is not connecting, ignoring input.");
                return;
            }

            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;

            network_event_t network_event;
            network_event.type = NETWORK_EVENT_INPUT;
            network_event.input.in_buffer_length = length;
            network_event.input.player_id = *player_id;
            memcpy(network_event.input.in_buffer, data, length);

            state.event_queue.push(network_event);
            break;
        }
    }
}

bool network_poll_events(network_event_t* event) {
    if (state.event_queue.empty()) {
        return false;
    }

    *event = state.event_queue.front();
    state.event_queue.pop();

    return true;
}
