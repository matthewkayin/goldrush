#include "network.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "menu/match_setting.h"
#include <enet/enet.h>
#include <queue>
#include <vector>

struct NetworkState {
    NetworkStatus status;
    ENetHost* host;
    ENetSocket scanner;

    char client_username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    char lobby_name_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];

    uint8_t player_id;
    NetworkPlayer players[MAX_PLAYERS];
    ENetPeer* peers[MAX_PLAYERS];

    std::queue<NetworkEvent> event_queue;
    std::vector<NetworkLobby> lobbies;
    uint8_t match_settings[MATCH_SETTING_COUNT];
};
static NetworkState state;

enum NetworkMessageType {
    NETWORK_MESSAGE_GREET_SERVER,
    NETWORK_MESSAGE_INVALID_VERSION,
    NETWORK_MESSAGE_GAME_ALREADY_STARTED,
    NETWORK_MESSAGE_WELCOME,
    NETWORK_MESSAGE_NEW_PLAYER,
    NETWORK_MESSAGE_GREET_CLIENT,
    NETWORK_MESSAGE_SET_READY,
    NETWORK_MESSAGE_SET_NOT_READY,
    NETWORK_MESSAGE_SET_COLOR,
    NETWORK_MESSAGE_SET_MATCH_SETTING,
    NETWORK_MESSAGE_SET_TEAM,
    NETWORK_MESSAGE_CHAT,
    NETWORK_MESSAGE_LOAD_MATCH,
    NETWORK_MESSAGE_INPUT
};

struct NetworkMessageGreetServer {
    const uint8_t type = NETWORK_MESSAGE_GREET_SERVER;
    char username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char app_version[NETWORK_APP_VERSION_BUFFER_SIZE];
};

struct NetworkMessageWelcome {
    const uint8_t type = NETWORK_MESSAGE_WELCOME;
    uint8_t incoming_player_id;
    uint8_t incoming_recolor_id;
    uint8_t incoming_team;
    uint8_t server_recolor_id;
    uint8_t server_team;
    char server_username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint8_t match_settings[MATCH_SETTING_COUNT];
};

struct NetworkMessageNewPlayer {
    const uint8_t type = NETWORK_MESSAGE_NEW_PLAYER;
    uint8_t padding;
    uint16_t port;
    char ip[NETWORK_IP_BUFFER_SIZE];
};

struct NetworkMessageGreetClient {
    const uint8_t type = NETWORK_MESSAGE_GREET_CLIENT;
    uint8_t player_id;
    uint8_t padding[2];
    NetworkPlayer player;
};

struct NetworkMessageSetColor {
    const uint8_t type = NETWORK_MESSAGE_SET_COLOR;
    uint8_t recolor_id;
};

struct NetworkMessageSetTeam {
    const uint8_t type = NETWORK_MESSAGE_SET_TEAM;
    uint8_t team;
};

struct NetworkMessageChat {
    const uint8_t type = NETWORK_MESSAGE_CHAT;
    char message[NETWORK_CHAT_BUFFER_SIZE];
};

struct NetworkMessageSetMatchSetting {
    const uint8_t type = NETWORK_MESSAGE_SET_MATCH_SETTING;
    uint8_t setting;
    uint8_t value;
};

bool network_init() {
    if (enet_initialize() != 0) {
        log_error("Unable to initialize enet.");
        return false;
    }

    state.scanner = ENET_SOCKET_NULL;
    state.status = NETWORK_STATUS_OFFLINE;

    log_info("Initialized network.");

    return true;
}

void network_quit() {
    enet_deinitialize();
    log_info("Quit network.");
}

void network_disconnect() {
    if (state.status == NETWORK_STATUS_OFFLINE) {
        log_warn("network_disconnect() called while offline.");
        return;
    }

    network_scanner_destroy();

    size_t connected_peers = 0;
    for (int peer_id = 0; peer_id < state.host->peerCount; peer_id++) {
        if (state.host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED) {
            connected_peers++;
            if (state.status == NETWORK_STATUS_SERVER || state.status == NETWORK_STATUS_CONNECTED) {
                enet_peer_disconnect(&state.host->peers[peer_id], 0);
            } else {
                enet_peer_reset(&state.host->peers[peer_id]);
            }
        }
    }
    enet_host_flush(state.host);

    if (state.status == NETWORK_STATUS_SERVER || state.status == NETWORK_STATUS_CONNECTING || state.status == NETWORK_STATUS_DISCONNECTING || connected_peers == 0) {
        enet_host_destroy(state.host);
        state.host = NULL;
        memset(state.peers, 0, sizeof(state.peers));
        state.status = NETWORK_STATUS_OFFLINE;
    } else if (state.status == NETWORK_STATUS_CONNECTED) {
        log_info("Client attempting gentle disconnect...");
        state.status = NETWORK_STATUS_DISCONNECTING;
    }
}

// SCANNER

bool network_scanner_create() {
    state.scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (state.scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create scanner socket.");
        return false;
    }
    enet_socket_set_option(state.scanner, ENET_SOCKOPT_BROADCAST, 1);

    return true;
}

void network_scanner_destroy() {
    if (state.scanner != ENET_SOCKET_NULL) {
        enet_socket_shutdown(state.scanner, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(state.scanner);
        state.scanner = ENET_SOCKET_NULL;
    }
}

void network_scanner_search(const char* query) {
    state.lobbies.clear();
    strcpy(state.lobby_name_query, query);

    ENetAddress scan_address;
    scan_address.host = ENET_HOST_BROADCAST;
    scan_address.port = NETWORK_SCANNER_PORT;

    char buffer = 3;
    ENetBuffer send_buffer;
    send_buffer.data = &buffer;
    send_buffer.dataLength = 1;
    if (enet_socket_send(state.scanner, &scan_address, &send_buffer, 1) != 1) {
        log_error("Failed to scan for LAN servers.");
    }
}

bool network_host_create() {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = NETWORK_BASE_PORT;

    state.host = NULL;
    while (state.host == NULL && address.port < NETWORK_BASE_PORT + MAX_PLAYERS) {
        state.host = enet_host_create(&address, MAX_PLAYERS - 1, 1, 0, 0);
        if (state.host == NULL) {
            address.port++;
        }
    }

    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        state.players[player_id] = (NetworkPlayer) {
            .status = NETWORK_PLAYER_STATUS_NONE,
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
    scanner_address.port = NETWORK_SCANNER_PORT;
    if (enet_socket_bind(state.scanner, &scanner_address) != 0) {
        log_error("Failed to bind scanner socket.");
        return false;
    }

    state.status = NETWORK_STATUS_SERVER;

    state.player_id = 0;
    state.players[0].status = NETWORK_PLAYER_STATUS_HOST;
    state.players[0].team = 0;
    strncpy(state.players[0].name, username, MAX_USERNAME_LENGTH + 1);
    sprintf(state.lobby_name, "%s's Game", username);

    memset(state.match_settings, 0, sizeof(state.match_settings));

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

// Service

void network_service() {
    if (state.scanner != ENET_SOCKET_NULL) {
        ENetSocketSet set;
        ENET_SOCKETSET_EMPTY(set);
        ENET_SOCKETSET_ADD(set, state.scanner);
        while (enet_socketset_select(state.scanner, &set, NULL, 0) > 0) {
            ENetAddress receive_address;
            ENetBuffer receive_buffer;
            char buffer[128]; 
            receive_buffer.data = &buffer;
            receive_buffer.dataLength = sizeof(buffer);
            if (enet_socket_receive(state.scanner, &receive_address, &receive_buffer, 1) <= 0) {
                continue;
            }

            if (state.status == NETWORK_STATUS_SERVER) {
                // Tell the client about this game
                NetworkLobbyInfo lobby_info;
                strncpy(lobby_info.name, state.lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
                lobby_info.player_count = 0;
                lobby_info.port = state.host->address.port;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (state.players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
                        lobby_info.player_count++;
                    }
                }
                log_trace("sending lobby info. name %s player count %u port %u", lobby_info.name, lobby_info.player_count, lobby_info.port);

                ENetBuffer response_buffer;
                response_buffer.data = &lobby_info;
                response_buffer.dataLength = sizeof(NetworkLobbyInfo);
                enet_socket_send(state.scanner, &receive_address, &response_buffer, 1);
            } else {
                NetworkLobbyInfo lobby_info;
                memcpy(&lobby_info, buffer, sizeof(NetworkLobbyInfo));
                NetworkLobby lobby;
                memcpy(&lobby.name, lobby_info.name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
                lobby.player_count = lobby_info.player_count;
                lobby.port = lobby_info.port;
                enet_address_get_host_ip(&receive_address, lobby.ip, NETWORK_IP_BUFFER_SIZE);
                if (strlen(state.lobby_name_query) == 0 || strstr(lobby.name, state.lobby_name_query) != NULL) {
                    state.lobbies.push_back(lobby);
                }
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
                    NetworkMessageGreetServer message;
                    strncpy(message.username, state.client_username, MAX_USERNAME_LENGTH + 1);
                    strncpy(message.app_version, APP_VERSION, sizeof(APP_VERSION));

                    ENetPacket* packet = enet_packet_create(&message, sizeof(message), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(&state.host->peers[0], 0, packet);
                    enet_host_flush(state.host);
                } else if (state.status != NETWORK_STATUS_SERVER) {
                    // Client greets client
                    log_info("New client joined. Sending greeting...");
                    NetworkMessageGreetClient message;
                    message.player_id = state.player_id;
                    memcpy(&message.player, &state.players[state.player_id], sizeof(NetworkPlayer));

                    ENetPacket* packet = enet_packet_create(&message, sizeof(message), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(event.peer, 0, packet);
                    enet_host_flush(state.host);
                }

                break;
            } // End case CONNECT
            case ENET_EVENT_TYPE_DISCONNECT: {
                if (state.status == NETWORK_STATUS_DISCONNECTING || state.status == NETWORK_STATUS_CONNECTING) {
                    if (state.status == NETWORK_STATUS_CONNECTED) {
                        state.event_queue.push((NetworkEvent) {
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

                    state.players[*player_id_ptr].status = NETWORK_PLAYER_STATUS_NONE;
                    state.event_queue.push((NetworkEvent) {
                        .type = NETWORK_EVENT_PLAYER_DISCONNECTED,
                        .player_disconnected = (NetworkEventPlayerDisconnected) {
                            .player_id = *player_id_ptr
                        }
                    });
                }

                break;
            } // End case DISCONNECT
            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet->data[0] != NETWORK_MESSAGE_INPUT) {
                    log_trace("Received message %b", event.packet->data, event.packet->dataLength);
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
        case NETWORK_MESSAGE_GREET_SERVER: {
            // ENET will handle the lobby is full scenario

            NetworkMessageGreetServer incoming_message;
            memcpy(&incoming_message, data, sizeof(incoming_message));
            
            // Check that the server is not already in a game
            if (state.players[state.player_id].status != NETWORK_PLAYER_STATUS_HOST) {
                log_info("Client tried to connect while server is no longer in lobby. Rejecting...");

                uint8_t message = NETWORK_MESSAGE_GAME_ALREADY_STARTED;
                ENetPacket* packet = enet_packet_create(&message, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(&state.host->peers[incoming_peer_id], 0, packet);
                enet_host_flush(state.host);

                return;
            }

            // Check the client version
            if (strcmp(incoming_message.app_version, APP_VERSION) != 0) {
                log_info("Client app version mismatch. Rejecting client...");

                uint8_t message = NETWORK_MESSAGE_INVALID_VERSION;
                ENetPacket* packet = enet_packet_create(&message, sizeof(uint8_t), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(&state.host->peers[incoming_peer_id], 0, packet);
                enet_host_flush(state.host);

                return;
            }

            // Determine incoming player id
            uint8_t incoming_player_id;
            for (incoming_player_id = 0; incoming_player_id < MAX_PLAYERS; incoming_player_id++) {
                if (state.players[incoming_player_id].status == NETWORK_PLAYER_STATUS_NONE) {
                    break;
                }
            }
            // Determine incoming player recolor id
            uint8_t incoming_player_recolor_id;
            for (incoming_player_recolor_id = 0; incoming_player_recolor_id < MAX_PLAYERS; incoming_player_recolor_id++) {
                bool is_recolor_id_in_use = false;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (player_id == incoming_player_id || state.players[player_id].status == NETWORK_PLAYER_STATUS_NONE) {
                        continue;
                    }

                    if (state.players[player_id].recolor_id == incoming_player_recolor_id) {
                        is_recolor_id_in_use = true;
                        break;
                    }
                }

                if (!is_recolor_id_in_use) {
                    break;
                }
            }
            // Determine incoming player team
            int team1_count = 0;
            int team2_count = 0;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (state.players[player_id].status == NETWORK_PLAYER_STATUS_NONE || player_id == incoming_player_id) {
                    continue;
                }

                if (state.players[player_id].team == 0) {
                    team1_count++;
                } else {
                    team2_count++;
                }
            }

            GOLD_ASSERT(incoming_player_id != MAX_PLAYERS && incoming_player_recolor_id != MAX_PLAYERS);

            // Set incoming player info
            strncpy(state.players[incoming_player_id].name, incoming_message.username, MAX_USERNAME_LENGTH + 1);
            state.players[incoming_player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;
            state.players[incoming_player_id].recolor_id = incoming_player_recolor_id;
            state.players[incoming_player_id].team = team1_count <= team2_count ? 0 : 1;

            // Setup incoming player / peer mapping
            uint8_t* incoming_player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
            *incoming_player_id_ptr = incoming_player_id;
            state.host->peers[incoming_peer_id].data = incoming_player_id_ptr;
            state.peers[incoming_player_id] = &state.host->peers[incoming_peer_id];

            // Send the welcome packet
            NetworkMessageWelcome response;
            response.incoming_player_id = incoming_player_id;
            response.incoming_recolor_id = incoming_player_recolor_id;
            response.incoming_team = state.players[incoming_player_id].team;
            response.server_recolor_id = state.players[0].recolor_id;
            response.server_team = state.players[0].team;
            strncpy(response.server_username, state.players[0].name, MAX_USERNAME_LENGTH + 1);
            strncpy(response.lobby_name, state.lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
            memcpy(response.match_settings, state.match_settings, sizeof(state.match_settings));

            ENetPacket* welcome_packet = enet_packet_create(&response, sizeof(response), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(&state.host->peers[incoming_peer_id], 0, welcome_packet);
            log_trace("Sent welcome packet to player %u", incoming_player_id);

            // Tell the other players about this new one
            NetworkMessageNewPlayer new_player_message;
            enet_address_get_host_ip(&state.host->peers[incoming_peer_id].address, new_player_message.ip, NETWORK_IP_BUFFER_SIZE);
            new_player_message.port = state.host->peers[incoming_peer_id].address.port;
            for (uint8_t player_id = 1; player_id < MAX_PLAYERS; player_id++) {
                if (player_id == incoming_player_id || state.players[player_id].status == NETWORK_PLAYER_STATUS_NONE) {
                    continue;
                }

                ENetPacket* new_player_packet = enet_packet_create(&new_player_message, sizeof(new_player_message), ENET_PACKET_FLAG_RELIABLE);
                enet_peer_send(state.peers[player_id], 0, new_player_packet);
                log_trace("Sent new player packet to player %u", player_id);
            }

            enet_host_flush(state.host);

            state.event_queue.push((NetworkEvent) {
                .type = NETWORK_EVENT_PLAYER_CONNECTED,
                .player_connected = (NetworkEventPlayerConnected) {
                    .player_id = incoming_player_id
                }
            });
            break;
        }
        case NETWORK_MESSAGE_INVALID_VERSION: {
            log_info("Client version does not match the server.");
            state.event_queue.push((NetworkEvent) {
                .type = NETWORK_EVENT_INVALID_VERSION
            });
            break;
        }
        case NETWORK_MESSAGE_WELCOME: {
            log_info("Joined lobby.");
            state.status = NETWORK_STATUS_CONNECTED;

            NetworkMessageWelcome incoming_message;
            memcpy(&incoming_message, data, sizeof(incoming_message));

            // Setup current player info
            state.player_id = incoming_message.incoming_player_id;
            state.players[state.player_id].recolor_id = incoming_message.incoming_recolor_id;
            state.players[state.player_id].team = incoming_message.incoming_team;
            strncpy(state.players[state.player_id].name, state.client_username, MAX_USERNAME_LENGTH + 1);
            state.players[state.player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;

            // Setup host player info
            state.players[0].status = NETWORK_PLAYER_STATUS_HOST;
            strncpy(state.players[0].name, incoming_message.server_username, MAX_USERNAME_LENGTH + 1);
            state.players[0].recolor_id = incoming_message.server_recolor_id;
            state.players[0].team = incoming_message.server_team;
            state.peers[0] = &state.host->peers[incoming_peer_id];
            uint8_t* server_player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
            *server_player_id_ptr = 0;
            state.peers[0]->data = server_player_id_ptr;

            // Get lobby name and settings
            strncpy(state.lobby_name, incoming_message.lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
            memcpy(state.match_settings, incoming_message.match_settings, sizeof(state.match_settings));

            state.event_queue.push((NetworkEvent) {
                .type = NETWORK_EVENT_JOINED_LOBBY
            });
            break;
        }
        case NETWORK_MESSAGE_NEW_PLAYER: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkMessageNewPlayer incoming_message;
            memcpy(&incoming_message, data, sizeof(incoming_message));

            ENetAddress new_player_address;
            new_player_address.port = incoming_message.port;
            enet_address_set_host_ip(&new_player_address, incoming_message.ip);

            ENetPeer* new_player_peer = enet_host_connect(state.host, &new_player_address, 1, 0);
            if (new_player_peer == NULL) {
                log_error("Unable to connect to new player.");
                return;
            }

            break;
        }
        case NETWORK_MESSAGE_GREET_CLIENT: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkMessageGreetClient incoming_message;
            memcpy(&incoming_message, data, sizeof(incoming_message));

            memcpy(&state.players[incoming_message.player_id], &incoming_message.player, sizeof(NetworkPlayer));
            state.peers[incoming_message.player_id] = &state.host->peers[incoming_peer_id];
            uint8_t* new_player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
            *new_player_id_ptr = incoming_message.player_id;
            state.host->peers[incoming_peer_id].data = new_player_id_ptr;
            break;
        }
        case NETWORK_MESSAGE_SET_READY:
        case NETWORK_MESSAGE_SET_NOT_READY: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_SERVER)) {
                return;
            }

            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;
            state.players[*player_id].status = message_type == NETWORK_MESSAGE_SET_READY ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;
            break;
        }
        case NETWORK_MESSAGE_SET_COLOR: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_SERVER)) {
                return;
            }

            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;
            state.players[*player_id].recolor_id = data[1];
            break;
        }
        case NETWORK_MESSAGE_SET_TEAM: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_SERVER)) {
                return;
            }

            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;
            state.players[*player_id].team = data[1];

            break;
        }
        case NETWORK_MESSAGE_CHAT: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_SERVER)) {
                return;
            }

            NetworkEvent event;
            event.type = NETWORK_EVENT_CHAT;
            uint8_t* player_id = (uint8_t*)state.host->peers[incoming_peer_id].data;
            event.chat.player_id = *player_id;
            strncpy(event.chat.message, (char*)(data + 1), NETWORK_CHAT_BUFFER_SIZE);
            state.event_queue.push(event);

            break;
        }
        case NETWORK_MESSAGE_SET_MATCH_SETTING: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_SERVER)) {
                return;
            }

            state.match_settings[data[1]] = data[2];
            break;
        }
        case NETWORK_MESSAGE_LOAD_MATCH: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (state.players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
                    state.players[player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;
                }
            }

            NetworkEvent event;
            event.type = NETWORK_EVENT_MATCH_LOAD;
            memcpy(&event.match_load.lcg_seed, data + 1, sizeof(int32_t));
            memcpy(&event.match_load.noise.width, data + 5, sizeof(uint32_t));
            memcpy(&event.match_load.noise.height, data + 9, sizeof(uint32_t));
            event.match_load.noise.map = (int8_t*)malloc(event.match_load.noise.width * event.match_load.noise.height);
            memcpy(&event.match_load.noise.map[0], data + 13, event.match_load.noise.width * event.match_load.noise.height);

            state.event_queue.push(event);
            break;
        }
        case NETWORK_MESSAGE_INPUT: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            uint8_t* player_id_ptr = (uint8_t*)state.host->peers[incoming_peer_id].data;

            NetworkEvent event;
            event.type = NETWORK_EVENT_INPUT;
            event.input.in_buffer_length = length;
            event.input.player_id = *player_id_ptr;
            memcpy(event.input.in_buffer, data, length);

            state.event_queue.push(event);

            break;
        }
    }
}

bool network_poll_events(NetworkEvent* event) {
    if (state.event_queue.empty()) {
        return false;
    }

    *event = state.event_queue.front();
    state.event_queue.pop();

    return true;
}

// Getters

NetworkStatus network_get_status() {
    return state.status;
}

bool network_is_server() {
    return state.status == NETWORK_STATUS_SERVER;
}

const NetworkPlayer& network_get_player(uint8_t player_id) {
    return state.players[player_id];
}

uint8_t network_get_player_id() {
    return state.player_id;
}

const size_t network_get_lobby_count() {
    return state.lobbies.size();
}

const NetworkLobby& network_get_lobby(size_t index) {
    return state.lobbies[index];
}

const char* network_get_lobby_name() {
    return state.lobby_name;
}

void network_send_chat(const char* message) {
    NetworkMessageChat out_message;
    strcpy(out_message.message, message);

    ENetPacket* packet = enet_packet_create(&out_message, sizeof(out_message), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_set_player_ready(bool ready) {
    state.players[state.player_id].status = ready ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;

    uint8_t message = ready ? NETWORK_MESSAGE_SET_READY : NETWORK_MESSAGE_SET_NOT_READY;
    ENetPacket* packet = enet_packet_create(&message, sizeof(message), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_set_player_color(uint8_t color) {
    NetworkMessageSetColor message;
    message.recolor_id = color;

    state.players[state.player_id].recolor_id = color;

    ENetPacket* packet = enet_packet_create(&message, sizeof(message), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_set_match_setting(uint8_t setting, uint8_t value) {
    NetworkMessageSetMatchSetting message;
    message.setting = setting;
    message.value = value;
    state.match_settings[setting] = value;

    ENetPacket* packet = enet_packet_create(&message, sizeof(message), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

uint8_t network_get_match_setting(uint8_t setting) {
    return state.match_settings[setting];
}

void network_set_player_team(uint8_t team) {
    state.players[state.player_id].team = team;
    NetworkMessageSetTeam message;
    message.team = team;

    ENetPacket* packet = enet_packet_create(&message, sizeof(message), ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}

void network_begin_loading_match(int32_t lcg_seed, const Noise& noise) {
    // Set all players to NOT_READY so that they can re-ready themselves once they enter the match
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
            state.players[player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;
        }
    }

    state.status = NETWORK_STATUS_CONNECTED;

    // Build message
    // Message size is 1 byte for type, 4 bytes for LCG seed, 8 bytes for map width / height, and the rest of the bytes are the generated noise values
    size_t message_size = 1 + 4 + 8 + (noise.width * noise.height * sizeof(int8_t));
    uint8_t* message = (uint8_t*)malloc(message_size);
    message[0] = NETWORK_MESSAGE_LOAD_MATCH;
    memcpy(message + 1, &lcg_seed, sizeof(int32_t));
    memcpy(message + 5, &noise.width, sizeof(uint32_t));
    memcpy(message + 9, &noise.height, sizeof(uint32_t));
    memcpy(message + 13, &noise.map[0], noise.width * noise.height * sizeof(int8_t));

    // Send the packet
    ENetPacket* packet = enet_packet_create(message, message_size, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);

    free(message);
}

void network_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    out_buffer[0] = NETWORK_MESSAGE_INPUT;
    ENetPacket* packet = enet_packet_create(out_buffer, out_buffer_length, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(state.host, 0, packet);
    enet_host_flush(state.host);
}