#include "network.h"

#include "logger.h"
#include "asserts.h"
#include <cstring>
#include <enet/enet.h>
#include <queue>

struct network_state_t {
    ENetHost* host;
    NetworkStatus status;

    char client_username[NAME_BUFFER_SIZE];

    uint8_t player_id;
    player_t players[MAX_PLAYERS];
    ENetPeer* peers[MAX_PLAYERS];

    std::queue<network_event_t> event_queue;
};
static network_state_t state;

// MESSAGE

enum MessageType {
    MESSAGE_GREET_SERVER,
    MESSAGE_INVALID_VERSION,
    MESSAGE_WELCOME,
    MESSAGE_NEW_PLAYER,
    MESSAGE_GREET,
    MESSAGE_READY,
    MESSAGE_NOT_READY
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

    state.status = NETWORK_STATUS_SERVER;

    state.player_id = 0;
    state.players[0].status = PLAYER_STATUS_HOST;
    strncpy(state.players[0].name, username, MAX_USERNAME_LENGTH + 1);

    log_info("Created server.");
    return true;
}

bool network_client_create(const char* username, const char* server_ip, uint16_t server_port) {
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

void network_toggle_ready() {
    state.players[state.player_id].status = state.players[state.player_id].status == PLAYER_STATUS_NOT_READY ? PLAYER_STATUS_READY : PLAYER_STATUS_NOT_READY;
    uint8_t message = state.players[state.player_id].status == PLAYER_STATUS_READY ? MESSAGE_READY : MESSAGE_NOT_READY;
    ENetPacket* packet = enet_packet_create(&message, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

// POLL EVENTS

void network_service() {
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

                    if (*player_id_ptr == 0) {
                        // This is used in lobby so that players know just to exit the game
                        state.event_queue.push((network_event_t) {
                            .type = NETWORK_EVENT_PLAYER_DISCONNECTED,
                            .player_disconnected = {
                                .player_id = *player_id_ptr
                            }
                        });
                    }
                }
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                log_info("Received message %b", event.packet->data, event.packet->dataLength);
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
            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;
            state.players[*player_id].status = message_type == MESSAGE_READY ? PLAYER_STATUS_READY : PLAYER_STATUS_NOT_READY;
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
