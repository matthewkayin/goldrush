#include "network.h"

#include "lobby_scanner.h"
#include "lobby.h"
#include "host.h"
#include "core/logger.h"
#include "core/asserts.h"
#include <enet/enet.h>
#include <steam/steam_api.h>
#include <queue>

struct NetworkState {
    NetworkBackend backend;
    NetworkStatus status;

    NetworkHost* host;
    NetworkLobbyScanner* lobby_scanner;
    NetworkLobby* lobby;

    NetworkPlayer players[MAX_PLAYERS];
    uint8_t player_id;
    uint8_t match_settings[MATCH_SETTING_COUNT];

    std::queue<NetworkEvent> events;

    char username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
};
static NetworkState state;

bool network_host_create();
bool network_lobby_create();
void network_handle_message(uint16_t peer_id, uint8_t* data, size_t length);

bool network_init() {
    SteamNetworkingUtils()->InitRelayNetworkAccess();

    if (enet_initialize() != 0) {
        log_error("Error initializing enet.");
        return false;
    }

    strncpy(state.username, SteamFriends()->GetPersonaName(), NETWORK_PLAYER_NAME_BUFFER_SIZE);

    state.host = NULL;
    state.lobby_scanner = NULL;
    state.lobby = NULL;

    return true;
}

void network_quit() {
    enet_deinitialize();
}

void network_set_backend(NetworkBackend backend) {
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

void network_service() {
    if (state.backend == NETWORK_BACKEND_STEAM) {
        SteamAPI_RunCallbacks();
    }
    if (state.lobby_scanner != NULL) {
        state.lobby_scanner->service();
    }
    if (state.lobby != NULL) {
        if (state.status == NETWORK_STATUS_CONNECTING && state.lobby->get_status() == NETWORK_LOBBY_OPEN) {
            state.events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_CONNECTED
            });
            state.status = NETWORK_STATUS_HOST;
        }
        if (state.lobby->get_status() == NETWORK_LOBBY_OPEN) {
            state.lobby->service();
        } 
    }
    if (state.host != NULL) {
        state.host->service();
        NetworkHostEvent event;
        while (state.host != NULL && state.status != NETWORK_STATUS_OFFLINE && state.host->poll_events(&event)) {
            switch (event.type) {
                case NETWORK_HOST_EVENT_CONNECTED: {
                    if (state.status == NETWORK_STATUS_CONNECTING) {
                        // Client greets server
                        log_info("Connected to server. Sending greeting...");
                        NetworkMessageGreetServer message;
                        strncpy(message.username, state.username, NETWORK_PLAYER_NAME_BUFFER_SIZE);
                        strncpy(message.app_version, APP_VERSION, sizeof(APP_VERSION));

                        state.host->send(event.connected.peer_id, &message, sizeof(message));
                        state.host->flush();
                    } else if (state.status != NETWORK_STATUS_HOST) {
                        // Client greets client
                        log_info("New client joined. Sending greeting...");
                        NetworkMessageGreetClient message;
                        message.player_id = state.player_id;
                        memcpy(&message.player, &state.players[state.player_id], sizeof(NetworkPlayer));

                        state.host->send(event.connected.peer_id, &message, sizeof(message));
                        state.host->flush();
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
                        delete state.host;
                        state.host = NULL;
                        state.status = NETWORK_STATUS_OFFLINE;
                        break;
                    }

                    if (event.disconnected.player_id == PLAYER_NONE) {
                        log_warn("Unidentified player disconnected.");
                        break;
                    }

                    log_info("Player %u disconnected.", event.disconnected.player_id);

                    state.players[event.disconnected.player_id].status = NETWORK_PLAYER_STATUS_NONE;
                    state.events.push((NetworkEvent) {
                        .type = NETWORK_EVENT_PLAYER_DISCONNECTED,
                        .player_disconnected = (NetworkEventPlayerDisconnected) {
                            .player_id = event.disconnected.player_id
                        }
                    });
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
    if (state.lobby_scanner != NULL) {
        delete state.lobby_scanner;
        state.lobby_scanner = NULL;
    }

    if (state.lobby != NULL) {
        state.lobby->close();
        delete state.lobby;
        state.lobby = NULL;
    }

    if (state.status == NETWORK_STATUS_OFFLINE) {
        log_warn("network_disconnect() called while offline.");
        return;
    }
    if (state.host != NULL) {
        uint16_t connected_peers = 0;
        for (uint16_t peer_id = 0; peer_id < state.host->get_peer_count(); peer_id++) {
            if (state.host->is_peer_connected(peer_id)) {
                connected_peers++;
                state.host->disconnect_peer(peer_id, state.status == NETWORK_STATUS_HOST || state.status == NETWORK_STATUS_CONNECTED);
            }
        }
        state.host->flush();

        if (state.status == NETWORK_STATUS_HOST || state.status == NETWORK_STATUS_CONNECTING || state.status == NETWORK_STATUS_DISCONNECTING || connected_peers == 0) {
            delete state.host;
            state.host = NULL;
            state.status = NETWORK_STATUS_OFFLINE;
        } else if (state.status == NETWORK_STATUS_CONNECTED) {
            log_info("Client attempting gently disconnect...");
            state.status = NETWORK_STATUS_DISCONNECTING;
        }
    }
}

// Lobbies

void network_search_lobbies(const char* query) {
    if (state.lobby_scanner == NULL) {
        if (state.backend == NETWORK_BACKEND_LAN) {
            state.lobby_scanner = new NetworkLanLobbyScanner();
        } else {
            state.lobby_scanner = new NetworkSteamLobbyScanner();
        }
    }

    state.lobby_scanner->search(query);
}

size_t network_get_matchlist_size() {
    return state.lobby_scanner->get_matchlist_size();
}

const NetworkMatchlistEntry& network_get_matchlist_entry(size_t index) {
    return state.lobby_scanner->get_matchlist_entry(index);
}

void network_open_lobby(const char* lobby_name) {
    if (!network_host_create()) {
        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        state.status = NETWORK_STATUS_OFFLINE;
        return;
    }

    if (!network_lobby_create()) {
        delete state.host;
        state.host = NULL;
        return;
    }
    state.lobby->open(lobby_name, state.host->get_connection_info());

    delete state.lobby_scanner;
    state.lobby_scanner = NULL;

    strncpy(state.lobby_name, lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);

    state.player_id = 0;
    strncpy(state.players[0].name, state.username, NETWORK_PLAYER_NAME_BUFFER_SIZE);
    state.players[0].status = NETWORK_PLAYER_STATUS_HOST;
    state.players[0].team = 0;

    memset(state.match_settings, 0, sizeof(state.match_settings));
    state.status = NETWORK_STATUS_CONNECTING;

    log_info("Created server.");
}

void network_join_lobby(size_t index) {
    if (!network_host_create()) {
        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        state.status = NETWORK_STATUS_OFFLINE;
        return;
    }

    NetworkConnectionInfo connection_info = state.lobby_scanner->get_matchlist_entry(index).connection_info;
    if (!state.host->connect(connection_info)) {
        state.events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        delete state.host;
        state.host = NULL;
        state.status = NETWORK_STATUS_OFFLINE;
        return;
    }

    delete state.lobby_scanner;
    state.lobby_scanner = NULL;

    memset(state.players, 0, sizeof(state.players));
    state.status = NETWORK_STATUS_CONNECTING;
}

const char* network_get_lobby_name() {
    return state.lobby_name;
}

void network_send_chat(const char* message) {
    NetworkMessageChat out_message;
    strcpy(out_message.message, message);

    state.host->broadcast(&out_message, sizeof(out_message));
    state.host->flush();
}

void network_set_player_ready(bool ready) {
    state.players[state.player_id].status = ready ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;

    uint8_t message = ready ? NETWORK_MESSAGE_SET_READY : NETWORK_MESSAGE_SET_NOT_READY;
    state.host->broadcast(&message, sizeof(message));
    state.host->flush();
}

void network_set_player_color(uint8_t color) {
    NetworkMessageSetColor message;
    message.recolor_id = color;

    state.players[state.player_id].recolor_id = color;

    state.host->broadcast(&message, sizeof(message));
    state.host->flush();
}

void network_set_match_setting(uint8_t setting, uint8_t value) {
    NetworkMessageSetMatchSetting message;
    message.setting = setting;
    message.value = value;
    state.match_settings[setting] = value;

    state.host->broadcast(&message, sizeof(message));
    state.host->flush();
}

uint8_t network_get_match_setting(uint8_t setting) {
    return state.match_settings[setting];
}

void network_set_player_team(uint8_t team) {
    state.players[state.player_id].team = team;
    NetworkMessageSetTeam message;
    message.team = team;

    state.host->broadcast(&message, sizeof(message));
    state.host->flush();
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
    state.host->broadcast(message, message_size);
    state.host->flush();

    free(message);
}

void network_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    out_buffer[0] = NETWORK_MESSAGE_INPUT;
    state.host->broadcast(out_buffer, out_buffer_length);
    state.host->flush();
}

// INTERNAL

bool network_host_create() {
    if (state.backend == NETWORK_BACKEND_LAN) {
        state.host = new NetworkLanHost();
    } else {
        state.host = new NetworkSteamHost();
    }
    if (!state.host->was_create_successful()) {
        delete state.host;
        return false;
    }
    return true;
}

bool network_lobby_create() {
    if (state.backend == NETWORK_BACKEND_LAN) {
        state.lobby = new NetworkLanLobby();
    } else {
        state.lobby = new NetworkSteamLobby();
    }
    if (state.lobby->get_status() == NETWORK_LOBBY_ERROR) {
        delete state.lobby;
        state.lobby = NULL;
        return false;
    }

    return true;
}

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
                state.host->send(incoming_peer_id, &message, sizeof(message));
                state.host->flush();

                return;
            }

            // Check the client version
            if (strcmp(incoming_message.app_version, APP_VERSION) != 0) {
                log_info("Client app version mismatch. Rejecting client...");

                uint8_t message = NETWORK_MESSAGE_INVALID_VERSION;
                state.host->send(incoming_peer_id, &message, sizeof(message));
                state.host->flush();

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
            state.host->set_peer_player_id(incoming_peer_id, incoming_player_id);

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

            state.host->send(incoming_peer_id, &response, sizeof(response));
            log_trace("Sent welcome packet to player %u", incoming_player_id);

            // Build new player message
            uint8_t new_player_message[NETWORK_IP_BUFFER_SIZE + 4];
            new_player_message[0] = NETWORK_MESSAGE_NEW_PLAYER;
            size_t new_player_message_size = 1;
            new_player_message_size += state.host->buffer_peer_connection_info(incoming_peer_id, new_player_message + new_player_message_size);

            // Tell the other players about this new one
            for (uint16_t peer_id = 0; peer_id < state.host->get_peer_count(); peer_id++) {
                if (peer_id == incoming_peer_id) {
                    continue;
                }

                state.host->send(peer_id, new_player_message, new_player_message_size);
            }
            state.host->flush();

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
            state.host->set_peer_player_id(incoming_peer_id, 0);

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

            NetworkConnectionInfo connection_info = state.host->parse_connection_info(data + 1);
            if (!state.host->connect(connection_info)) {
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
            state.host->set_peer_player_id(incoming_peer_id, incoming_message.player_id);
            break;
        }
        case NETWORK_MESSAGE_SET_READY:
        case NETWORK_MESSAGE_SET_NOT_READY: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = state.host->get_peer_player_id(incoming_peer_id); 
            state.players[player_id].status = message_type == NETWORK_MESSAGE_SET_READY ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;
            break;
        }
        case NETWORK_MESSAGE_SET_COLOR: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = state.host->get_peer_player_id(incoming_peer_id); 
            state.players[player_id].recolor_id = data[1];
            break;
        }
        case NETWORK_MESSAGE_SET_TEAM: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = state.host->get_peer_player_id(incoming_peer_id); 
            state.players[player_id].team = data[1];

            break;
        }
        case NETWORK_MESSAGE_CHAT: {
            if (!(state.status == NETWORK_STATUS_CONNECTED || state.status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkEvent event;
            event.type = NETWORK_EVENT_CHAT;
            event.chat.player_id = state.host->get_peer_player_id(incoming_peer_id);
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
            event.input.player_id = state.host->get_peer_player_id(incoming_peer_id);
            GOLD_ASSERT(length <= 1024);
            memcpy(event.input.in_buffer, data, length);

            state.events.push(event);

            break;
        }
    }
}