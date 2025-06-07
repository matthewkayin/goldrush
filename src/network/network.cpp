#include "network.h"

#include "host.h"
#include "core/logger.h"
#include "core/asserts.h"
#include <enet/enet.h>
#include <steam/steam_api.h>
#include <queue>
#include <vector>

struct NetworkState {
    NetworkBackend backend;
    NetworkStatus status;

    NetworkHost* host;
    ENetSocket lan_scanner;
    CSteamID steam_lobby_id;
    uint8_t steam_lobby_player_count;

    NetworkPlayer players[MAX_PLAYERS];
    uint8_t player_id;
    uint8_t match_settings[MATCH_SETTING_COUNT];

    std::queue<NetworkEvent> events;
    std::vector<NetworkLobby> lobbies;

    char username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    char lobby_name_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];
};
static NetworkState state;

bool network_lan_scanner_create() {
    state.lan_scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (state.lan_scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create scanner socket.");
        return false;
    }
    enet_socket_set_option(state.lan_scanner, ENET_SOCKOPT_BROADCAST, 1);

    return true;
}

bool network_lan_listener_create() {
    state.lan_scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (state.lan_scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create listener socket.");
        return false;
    }
    enet_socket_set_option(state.lan_scanner, ENET_SOCKOPT_REUSEADDR, 1);

    ENetAddress scanner_address;
    scanner_address.host = ENET_HOST_ANY;
    scanner_address.port = NETWORK_SCANNER_PORT;
    if (enet_socket_bind(state.lan_scanner, &scanner_address) != 0) {
        log_error("Failed to bind listener socket.");
        return false;
    }

    return true;
}

void network_lan_scanner_destroy() {
    if (state.lan_scanner != ENET_SOCKET_NULL) {
        enet_socket_shutdown(state.lan_scanner, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(state.lan_scanner);
        state.lan_scanner = ENET_SOCKET_NULL;
    }
}

void network_steam_update_lobby_player_count() {
    state.steam_lobby_player_count = network_get_player_count();
    char buffer[2] = { (char)state.steam_lobby_player_count, '\0' };
    SteamMatchmaking()->SetLobbyData(state.steam_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT, buffer);
}

void network_handle_message(uint16_t peer_id, uint8_t* data, size_t length);

bool network_init() {
    SteamNetworkingUtils()->InitRelayNetworkAccess();
    SteamAPI_ManualDispatch_Init();

    if (enet_initialize() != 0) {
        log_error("Error initializing enet.");
        return false;
    }

    strncpy(state.username, SteamFriends()->GetPersonaName(), NETWORK_PLAYER_NAME_BUFFER_SIZE);

    state.host = NULL;
    state.lan_scanner = ENET_SOCKET_NULL;

    return true;
}

void network_quit() {
    if (state.status != NETWORK_STATUS_OFFLINE) {
        network_disconnect();
        if (state.host != NULL) {
            network_host_destroy(state.host);
        }
    }
    enet_deinitialize();
}

void network_set_backend(NetworkBackend backend) {
    if (state.status != NETWORK_STATUS_OFFLINE) {
        log_warn("Called network_set_backend() when not offline yet.");
    }
    state.backend = backend;
}

NetworkBackend network_get_backend() {
    return state.backend;
}

NetworkStatus network_get_status() {
    return state.status;
}

bool network_is_host() {
    return state.status == NETWORK_STATUS_HOST;
}

const NetworkPlayer& network_get_player(uint8_t player_id) {
    return state.players[player_id];
}

uint8_t network_get_player_id() {
    return state.player_id;
}

uint8_t network_get_player_count() {
    uint8_t player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state.players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
            player_count++;
        }
    }

    return player_count;
}

void network_service() {
    if (state.backend == NETWORK_BACKEND_LAN && state.lan_scanner != ENET_SOCKET_NULL) {
        ENetSocketSet set;
        ENET_SOCKETSET_EMPTY(set);
        ENET_SOCKETSET_ADD(set, state.lan_scanner);
        while (enet_socketset_select(state.lan_scanner, &set, NULL, 0) > 0) {
            ENetAddress receive_address;
            ENetBuffer receive_buffer;
            char buffer[128]; 
            receive_buffer.data = &buffer;
            receive_buffer.dataLength = sizeof(buffer);
            if (enet_socket_receive(state.lan_scanner, &receive_address, &receive_buffer, 1) <= 0) {
                continue;
            }

            if (state.status == NETWORK_STATUS_HOST) {
                // Tell the client about this lobby
                NetworkLanLobbyInfo lobby_info;
                strncpy(lobby_info.name, state.lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
                lobby_info.port = state.host->lan.host->address.port;
                lobby_info.player_count = network_get_player_count();
                log_trace("sending lobby info. name %s player count %u port %u", lobby_info.name, lobby_info.player_count, lobby_info.port);

                ENetBuffer response_buffer;
                response_buffer.data = &lobby_info;
                response_buffer.dataLength = sizeof(NetworkLanLobbyInfo);
                enet_socket_send(state.lan_scanner, &receive_address, &response_buffer, 1);
            } else {
                // Server told us about a lobby
                NetworkLanLobbyInfo lobby_info;
                memcpy(&lobby_info, buffer, sizeof(NetworkLanLobbyInfo));
                NetworkLobby lobby;
                memcpy(&lobby.name, lobby_info.name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
                lobby.player_count = lobby_info.player_count;
                lobby.connection_info.lan.port = lobby_info.port;
                enet_address_get_host_ip(&receive_address, lobby.connection_info.lan.ip, NETWORK_IP_BUFFER_SIZE);
                if (strlen(state.lobby_name_query) == 0 || strstr(lobby.name, state.lobby_name_query) != NULL) {
                    state.lobbies.push_back(lobby);
                }
            }
        } // End while socket select
    } // End if lan scanner is not null

    if (state.backend == NETWORK_BACKEND_STEAM) {
        if (state.status == NETWORK_STATUS_HOST && state.steam_lobby_player_count != network_get_player_count()) {
            network_steam_update_lobby_player_count();
        }

        HSteamPipe steam_pipe = SteamAPI_GetHSteamPipe();
        SteamAPI_ManualDispatch_RunFrame(steam_pipe);
        CallbackMsg_t callback;
        while (SteamAPI_ManualDispatch_GetNextCallback(steam_pipe, &callback)) {
            switch (callback.m_iCallback) {
                case LobbyMatchList_t::k_iCallback: {
                    LobbyMatchList_t* lobby_matchlist = (LobbyMatchList_t*)callback.m_pubParam;

                    for (uint32_t lobby_index = 0; lobby_index < lobby_matchlist->m_nLobbiesMatching; lobby_index++) {
                        CSteamID lobby_id = SteamMatchmaking()->GetLobbyByIndex(lobby_index);
                        NetworkLobby lobby;
                        strncpy(lobby.name, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);
                        memcpy(&lobby.player_count, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT), sizeof(uint8_t));
                        strncpy(lobby.connection_info.steam.identity_str, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY), sizeof(lobby.connection_info.steam.identity_str));
                        log_trace("Found lobby %s host steam identity %s", lobby.name, lobby.connection_info.steam.identity_str);

                        if (strlen(state.lobby_name_query) == 0 || strstr(state.lobby_name_query, lobby.name) != NULL) {
                            state.lobbies.push_back(lobby);
                        }
                    }
                    break;
                }
                case LobbyCreated_t::k_iCallback: {
                    LobbyCreated_t* lobby_created = (LobbyCreated_t*)callback.m_pubParam;

                    if (lobby_created->m_eResult != k_EResultOK) {
                        state.status = NETWORK_STATUS_OFFLINE;
                        state.events.push((NetworkEvent) {
                            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
                        });
                        break;
                    }

                    state.steam_lobby_id = lobby_created->m_ulSteamIDLobby;
                    SteamMatchmaking()->SetLobbyData(state.steam_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME, state.lobby_name);
                    NetworkConnectionInfo connection_info = network_host_get_connection_info(state.host);
                    SteamMatchmaking()->SetLobbyData(state.steam_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY, connection_info.steam.identity_str);

                    network_steam_update_lobby_player_count();

                    state.events.push((NetworkEvent) {
                        .type = NETWORK_EVENT_LOBBY_CONNECTED
                    });
                    state.status = NETWORK_STATUS_HOST;
                    break;
                }
                default:
                    break;
            } // End switch steam callback type

            SteamAPI_ManualDispatch_FreeLastCallback(steam_pipe);
        } // End while steam getnextcallback
    } // End if backend is steam

    if (state.host != NULL) {
        network_host_service(state.host);
        NetworkHostEvent event;
        while (state.host != NULL && network_host_poll_events(state.host, &event)) {
            switch (event.type) {
                case NETWORK_HOST_EVENT_CONNECTED: {
                    if (state.status == NETWORK_STATUS_CONNECTING) {
                        // Client greets server
                        log_info("Connected to server. Sending greeting...");
                        NetworkMessageGreetServer message;
                        strncpy(message.username, state.username, NETWORK_PLAYER_NAME_BUFFER_SIZE);
                        strncpy(message.app_version, APP_VERSION, sizeof(APP_VERSION));

                        network_host_send(state.host, event.connected.peer_id, &message, sizeof(message));
                        network_host_flush(state.host);
                    } else if (state.status != NETWORK_STATUS_HOST) {
                        // Client greets client
                        log_info("New client joined. Sending greeting...");
                        NetworkMessageGreetClient message;
                        message.player_id = state.player_id;
                        memcpy(&message.player, &state.players[state.player_id], sizeof(NetworkPlayer));

                        network_host_send(state.host, event.connected.peer_id, &message, sizeof(message));
                        network_host_flush(state.host);
                    } 
                    break;
                } // End case CONNECTED
                case NETWORK_HOST_EVENT_DISCONNECTED: {
                    if (state.status == NETWORK_STATUS_CONNECTING) {
                        state.events.push((NetworkEvent) {
                            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
                        });
                    }

                    if (state.status == NETWORK_STATUS_DISCONNECTING || state.status == NETWORK_STATUS_CONNECTING) {
                        network_host_destroy(state.host);
                        state.host = NULL;
                        state.status = NETWORK_STATUS_OFFLINE;
                        break;
                    }

                    if (event.disconnected.player_id == PLAYER_NONE) {
                        log_warn("Unidentified player disconnected.");
                        break;
                    }

                    state.players[event.disconnected.player_id].status = NETWORK_PLAYER_STATUS_NONE;
                    state.events.push((NetworkEvent) {
                        .type = NETWORK_EVENT_PLAYER_DISCONNECTED,
                        .player_disconnected = (NetworkEventPlayerDisconnected) {
                            .player_id = event.disconnected.player_id
                        }
                    });
                    log_info("Player %u disconnected.", event.disconnected.player_id);
                    break;
                } // End case DISCONNECTED
                case NETWORK_HOST_EVENT_RECEIVED: {
                    if (event.received.packet.data[0] != NETWORK_MESSAGE_INPUT) {
                        log_trace("Received message %b", event.received.packet.data, event.received.packet.length);
                    }
                    network_handle_message(event.received.peer_id, event.received.packet.data, event.received.packet.length);
                    network_host_packet_destroy(event.received.packet);
                    break;
                }
            }
        }
    }
}

bool network_poll_events(NetworkEvent* event) {
    if (state.events.empty()) {
        return false;
    }

    *event = state.events.front();
    state.events.pop();

    return true;
}

void network_disconnect() {
    if (state.status == NETWORK_STATUS_OFFLINE) {
        log_warn("network_disconnect() called while offline.");
        return;
    }

    if (state.backend == NETWORK_BACKEND_LAN && state.lan_scanner != ENET_SOCKET_NULL) {
        network_lan_scanner_destroy();
    }
    if (state.backend == NETWORK_BACKEND_STEAM && state.status == NETWORK_STATUS_HOST) {
        SteamMatchmaking()->LeaveLobby(state.steam_lobby_id);
    }

    if (state.host != NULL) {
        uint16_t connected_peers = 0;
        for (uint16_t peer_id = 0; peer_id < network_host_get_peer_count(state.host); peer_id++) {
            if (network_host_is_peer_connected(state.host, peer_id)) {
                connected_peers++;
                network_host_disconnect_peer(state.host, peer_id, state.status == NETWORK_STATUS_HOST || state.status == NETWORK_STATUS_CONNECTED);
            }
        }
        network_host_flush(state.host);

        if (state.status == NETWORK_STATUS_HOST || 
                state.status == NETWORK_STATUS_CONNECTING || 
                state.status == NETWORK_STATUS_DISCONNECTING || 
                state.backend == NETWORK_BACKEND_STEAM || 
                connected_peers == 0) {
            network_host_destroy(state.host);
            state.host = NULL;
            state.status = NETWORK_STATUS_OFFLINE;
        } else if (state.status == NETWORK_STATUS_CONNECTED) {
            log_info("Client attempting gentle disconnect...");
            state.status = NETWORK_STATUS_DISCONNECTING;
        }
    }
}

// Lobbies

void network_search_lobbies(const char* query) {
    strncpy(state.lobby_name_query, query, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    state.lobbies.clear();

    if (state.backend == NETWORK_BACKEND_LAN) {
        if (state.lan_scanner == ENET_SOCKET_NULL) {
            if (!network_lan_scanner_create()) {
                return;
            }
        }

        ENetAddress scan_address;
        scan_address.host = ENET_HOST_BROADCAST;
        scan_address.port = NETWORK_SCANNER_PORT;

        char buffer = 3;
        ENetBuffer send_buffer;
        send_buffer.data = &buffer;
        send_buffer.dataLength = 1;
        if (enet_socket_send(state.lan_scanner, &scan_address, &send_buffer, 1) != 1) {
            log_error("Failed to scan for LAN servers.");
        }
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        SteamMatchmaking()->RequestLobbyList();
    }
}

size_t network_get_lobby_count() {
    return state.lobbies.size();
}

const NetworkLobby& network_get_lobby(size_t index) {
    return state.lobbies[index];
}

void network_open_lobby(const char* lobby_name) {
    state.host = network_host_create(state.backend);
    if (state.host == NULL) {
        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        state.status = NETWORK_STATUS_OFFLINE;
        return;
    }

    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_scanner_destroy();
        if (!network_lan_listener_create()) {
            network_host_destroy(state.host);
            state.events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
            });
            state.status = NETWORK_STATUS_OFFLINE;
            return;
        }

        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTED
        });
        state.status = NETWORK_STATUS_HOST;
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, 2);
        state.status = NETWORK_STATUS_CONNECTING;
    }

    strncpy(state.lobby_name, lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);

    memset(state.players, 0, sizeof(state.players));
    memset(state.match_settings, 0, sizeof(state.match_settings));

    state.player_id = 0;
    strncpy(state.players[0].name, state.username, NETWORK_PLAYER_NAME_BUFFER_SIZE);
    state.players[0].status = NETWORK_PLAYER_STATUS_HOST;
    state.players[0].team = 0;

    log_info("Created server.");
}

void network_join_lobby(size_t index) {
    state.host = network_host_create(state.backend);
    if (state.host == NULL) {
        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        state.status = NETWORK_STATUS_OFFLINE;
        return;
    }

    NetworkConnectionInfo connection_info = state.lobbies[index].connection_info;
    if (!network_host_connect(state.host, connection_info)) {
        network_host_destroy(state.host);
        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        return;
    }

    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_scanner_destroy();
    }

    memset(state.players, 0, sizeof(state.players));
    state.status = NETWORK_STATUS_CONNECTING;
}

const char* network_get_lobby_name() {
    return state.lobby_name;
}

void network_send_chat(const char* message) {
    NetworkMessageChat out_message;
    strcpy(out_message.message, message);

    network_host_broadcast(state.host, &out_message, sizeof(out_message));
    network_host_flush(state.host);
}

void network_set_player_ready(bool ready) {
    state.players[state.player_id].status = ready ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;

    uint8_t message = ready ? NETWORK_MESSAGE_SET_READY : NETWORK_MESSAGE_SET_NOT_READY;
    network_host_broadcast(state.host, &message, sizeof(message));
    network_host_flush(state.host);
}

void network_set_player_color(uint8_t color) {
    NetworkMessageSetColor message;
    message.recolor_id = color;

    state.players[state.player_id].recolor_id = color;

    network_host_broadcast(state.host, &message, sizeof(message));
    network_host_flush(state.host);
}

void network_set_match_setting(uint8_t setting, uint8_t value) {
    NetworkMessageSetMatchSetting message;
    message.setting = setting;
    message.value = value;
    state.match_settings[setting] = value;

    network_host_broadcast(state.host, &message, sizeof(message));
    network_host_flush(state.host);
}

uint8_t network_get_match_setting(uint8_t setting) {
    return state.match_settings[setting];
}

void network_set_player_team(uint8_t team) {
    state.players[state.player_id].team = team;
    NetworkMessageSetTeam message;
    message.team = team;

    network_host_broadcast(state.host, &message, sizeof(message));
    network_host_flush(state.host);
}

void network_begin_loading_match(int32_t lcg_seed, const Noise& noise) {
    if (state.backend == NETWORK_BACKEND_LAN) {
        network_lan_scanner_destroy();
    } else if (state.backend == NETWORK_BACKEND_STEAM) {
        SteamMatchmaking()->LeaveLobby(state.steam_lobby_id);
    }

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
    network_host_broadcast(state.host, message, message_size);
    network_host_flush(state.host);

    free(message);
}

void network_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    out_buffer[0] = NETWORK_MESSAGE_INPUT;
    network_host_broadcast(state.host, out_buffer, out_buffer_length);
    network_host_flush(state.host);
}

// INTERNAL

void network_handle_message(uint16_t incoming_peer_id, uint8_t* data, size_t length) {
    uint8_t message_type = data[0];

    switch (message_type) {
        case NETWORK_MESSAGE_GREET_SERVER: {
            // Host class will handle the lobby is full scenario

            NetworkMessageGreetServer incoming_message;
            memcpy(&incoming_message, data, sizeof(incoming_message));
            
            // Check that the server is not already in a game
            if (state.players[state.player_id].status != NETWORK_PLAYER_STATUS_HOST) {
                log_info("Client tried to connect while server is no longer in lobby. Rejecting...");

                uint8_t message = NETWORK_MESSAGE_GAME_ALREADY_STARTED;
                network_host_send(state.host, incoming_peer_id, &message, sizeof(message));
                network_host_flush(state.host);

                return;
            }

            // Check the client version
            if (strcmp(incoming_message.app_version, APP_VERSION) != 0) {
                log_info("Client app version mismatch. Rejecting client...");

                uint8_t message = NETWORK_MESSAGE_INVALID_VERSION;
                network_host_send(state.host, incoming_peer_id, &message, sizeof(message));
                network_host_flush(state.host);

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
            network_host_set_peer_player_id(state.host, incoming_peer_id, incoming_player_id);

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

            network_host_send(state.host, incoming_peer_id, &response, sizeof(response));
            log_trace("Sent welcome packet to player %u", incoming_player_id);

            // Build new player message
            uint8_t new_player_message[NETWORK_IP_BUFFER_SIZE + 4];
            new_player_message[0] = NETWORK_MESSAGE_NEW_PLAYER;
            size_t new_player_message_size = 1;
            new_player_message_size = network_host_buffer_peer_connection_info(state.host, incoming_peer_id, new_player_message + new_player_message_size);

            // Tell the other players about this new one
            for (uint16_t peer_id = 0; peer_id < network_host_get_peer_count(state.host); peer_id++) {
                if (peer_id == incoming_peer_id) {
                    continue;
                }

                network_host_send(state.host, peer_id, new_player_message, new_player_message_size);
            }
            network_host_flush(state.host);

            state.events.push((NetworkEvent) {
                .type = NETWORK_EVENT_PLAYER_CONNECTED,
                .player_connected = (NetworkEventPlayerConnected) {
                    .player_id = incoming_player_id
                }
            });
            break;
        }
        case NETWORK_MESSAGE_INVALID_VERSION: {
            log_info("Client version does not match the server.");
            state.events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_INVALID_VERSION
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
            strncpy(state.players[state.player_id].name, state.username, MAX_USERNAME_LENGTH + 1);
            state.players[state.player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;

            // Setup host player info
            state.players[0].status = NETWORK_PLAYER_STATUS_HOST;
            strncpy(state.players[0].name, incoming_message.server_username, MAX_USERNAME_LENGTH + 1);
            state.players[0].recolor_id = incoming_message.server_recolor_id;
            state.players[0].team = incoming_message.server_team;
            network_host_set_peer_player_id(state.host, incoming_peer_id, 0);

            // Get lobby name and settings
            strncpy(state.lobby_name, incoming_message.lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
            memcpy(state.match_settings, incoming_message.match_settings, sizeof(state.match_settings));

            state.events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_CONNECTED
            });
            break;
        }
        case NETWORK_MESSAGE_NEW_PLAYER: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkConnectionInfo connection_info = network_parse_connection_info(state.backend, data + 1); 
            if (!network_host_connect(state.host, connection_info)) {
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
            network_host_set_peer_player_id(state.host, incoming_peer_id, incoming_message.player_id);
            break;
        }
        case NETWORK_MESSAGE_SET_READY:
        case NETWORK_MESSAGE_SET_NOT_READY: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = network_host_get_peer_player_id(state.host, incoming_peer_id); 
            state.players[player_id].status = message_type == NETWORK_MESSAGE_SET_READY ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;
            break;
        }
        case NETWORK_MESSAGE_SET_COLOR: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = network_host_get_peer_player_id(state.host, incoming_peer_id); 
            state.players[player_id].recolor_id = data[1];
            break;
        }
        case NETWORK_MESSAGE_SET_TEAM: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = network_host_get_peer_player_id(state.host, incoming_peer_id); 
            state.players[player_id].team = data[1];

            break;
        }
        case NETWORK_MESSAGE_CHAT: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkEvent event;
            event.type = NETWORK_EVENT_CHAT;
            event.chat.player_id = network_host_get_peer_player_id(state.host, incoming_peer_id);
            strncpy(event.chat.message, (char*)(data + 1), NETWORK_CHAT_BUFFER_SIZE);
            state.events.push(event);

            break;
        }
        case NETWORK_MESSAGE_SET_MATCH_SETTING: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            state.match_settings[data[1]] = data[2];
            break;
        }
        case NETWORK_MESSAGE_LOAD_MATCH: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            if (state.backend == NETWORK_BACKEND_LAN) {
                network_lan_scanner_destroy();
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

            state.events.push(event);
            break;
        }
        case NETWORK_MESSAGE_INPUT: {
            if (state.status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkEvent event;
            event.type = NETWORK_EVENT_INPUT;
            event.input.in_buffer_length = length;
            event.input.player_id = network_host_get_peer_player_id(state.host, incoming_peer_id);
            GOLD_ASSERT(length <= 1024);
            memcpy(event.input.in_buffer, data, length);

            state.events.push(event);

            break;
        }
    }
}