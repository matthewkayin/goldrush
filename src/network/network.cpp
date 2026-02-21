#include "network.h"

#include "network/lan/host.h"
#include "network/lan/scanner.h"
#include "network/steam/host.h"
#include "network/steam/scanner.h"
#include "core/logger.h"
#include "core/asserts.h"
#include <enet/enet.h>
#include <queue>
#include <vector>
#include <cstring>

#ifdef GOLD_STEAM
    #include <steam/steam_api.h>
#endif

struct NetworkState {
    NetworkBackend backend;
    NetworkStatus status;

    INetworkHost* host;
    INetworkScanner* scanner;

    NetworkPlayer players[MAX_PLAYERS];
    uint8_t player_id;
    uint8_t match_settings[MATCH_SETTING_COUNT];

    std::queue<NetworkEvent> events;

    char username[NETWORK_PLAYER_NAME_BUFFER_SIZE];

#ifdef GOLD_STEAM
    CSteamID steam_invite_lobby_id;

    STEAM_CALLBACK(NetworkState, on_steam_lobby_join_requested, GameLobbyJoinRequested_t);
    STEAM_CALLBACK(NetworkState, on_steam_lobby_data_update, LobbyDataUpdate_t);
#endif
};
static NetworkState* state = nullptr;

bool network_create_host();
void network_destroy_host();
bool network_create_scanner();
void network_destroy_scanner();
void network_set_players_not_ready();
void network_handle_message(uint16_t peer_id, uint8_t* data, size_t length);

bool network_init() {
    state = new NetworkState();

    if (enet_initialize() != 0) {
        log_error("Error initializing enet.");
        return false;
    }

#ifdef GOLD_STEAM
    SteamNetworkingUtils()->InitRelayNetworkAccess();
    strncpy(state->username, SteamFriends()->GetPersonaName(), NETWORK_PLAYER_NAME_BUFFER_SIZE);
#else
    state->username[0] = '\0';
#endif

    state->host = nullptr;
    state->scanner = nullptr;
#ifdef GOLD_STEAM
    state->steam_invite_lobby_id.Clear();
#endif

    return true;
}

void network_quit() {
    if (state->status != NETWORK_STATUS_OFFLINE) {
        network_disconnect();
    }
    enet_deinitialize();
    delete state;
}

NetworkBackend network_get_backend() {
    return state->backend;
}

void network_set_backend(NetworkBackend backend) {
    if (state->status != NETWORK_STATUS_OFFLINE) {
        log_warn("Called network_set_backend() when not offline yet.");
    }
    state->backend = backend;
}

const char* network_get_username() {
    return state->username;
}

#ifndef GOLD_STEAM

void network_set_username(const char* value) {
    strncpy(state->username, value, NETWORK_PLAYER_NAME_BUFFER_SIZE - 1);
}

#endif

NetworkStatus network_get_status() {
    return state->status;
}

bool network_is_host() {
    return state->status == NETWORK_STATUS_HOST;
}

const NetworkPlayer& network_get_player(uint8_t player_id) {
    return state->players[player_id];
}

uint8_t network_get_player_id() {
    return state->player_id;
}

uint8_t network_get_player_count() {
    uint8_t player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
            player_count++;
        }
    }

    return player_count;
}

// SERVICE

void network_service() {
    if (state->scanner != nullptr) {
        state->scanner->service();
    }

    if (state->host != nullptr) {
        state->host->service();
        NetworkHostEvent event;
        while (state->host != nullptr && state->host->poll_events(&event)) {
            switch (event.type) {
                case NETWORK_HOST_EVENT_LOBBY_CREATE_FAILED: {
                    network_destroy_host();
                    state->status = NETWORK_STATUS_OFFLINE;
                    state->events.push((NetworkEvent) {
                        .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
                    });
                    break;
                }
                case NETWORK_HOST_EVENT_LOBBY_CREATE_SUCCESS: {
                    state->status = NETWORK_STATUS_HOST;
                    state->events.push((NetworkEvent) {
                        .type = NETWORK_EVENT_LOBBY_CONNECTED
                    });
                    break;
                }
                case NETWORK_HOST_EVENT_CONNECTED: {
                    if (state->status == NETWORK_STATUS_CONNECTING) {
                        // Client greets server
                        log_info("Connected to server. Sending greeting...");
                        NetworkMessageGreetServer message;
                        strncpy(message.username, state->username, NETWORK_PLAYER_NAME_BUFFER_SIZE);
                        strncpy(message.app_version, APP_VERSION, sizeof(APP_VERSION));

                        state->host->send(event.connected.peer_id, &message, sizeof(message));
                        state->host->flush();
                    } else if (state->status != NETWORK_STATUS_HOST) {
                        // Client greets client
                        log_info("New client joined. Sending greeting...");
                        NetworkMessageGreetClient message;
                        message.player_id = state->player_id;
                        memcpy(&message.player, &state->players[state->player_id], sizeof(NetworkPlayer));

                        state->host->send(event.connected.peer_id, &message, sizeof(message));
                        state->host->flush();
                    } 
                    break;
                } // End case CONNECTED
                case NETWORK_HOST_EVENT_DISCONNECTED: {
                    if (state->status == NETWORK_STATUS_CONNECTING) {
                        state->events.push((NetworkEvent) {
                            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
                        });
                    }

                    if (state->status == NETWORK_STATUS_CONNECTING) {
                        network_destroy_host();
                        state->status = NETWORK_STATUS_OFFLINE;
                        break;
                    }

                    if (event.disconnected.player_id == PLAYER_NONE) {
                        log_warn("Unidentified player disconnected.");
                        break;
                    }

                    state->players[event.disconnected.player_id].status = NETWORK_PLAYER_STATUS_NONE;
                    state->events.push((NetworkEvent) {
                        .type = NETWORK_EVENT_PLAYER_DISCONNECTED,
                        .player_disconnected = (NetworkEventPlayerDisconnected) {
                            .player_id = event.disconnected.player_id
                        }
                    });
                    log_info("Player %u disconnected.", event.disconnected.player_id);
                    break;
                } // End case DISCONNECTED
                case NETWORK_HOST_EVENT_RECEIVED: {
                    network_handle_message(event.received.peer_id, event.received.packet.data, event.received.packet.length);
                    state->host->destroy_packet(&event.received.packet);
                    break;
                }
            }
        }
    }
}

bool network_poll_events(NetworkEvent* event) {
    if (state->events.empty()) {
        return false;
    }

    *event = state->events.front();
    state->events.pop();

    return true;
}

void network_cleanup_event(const NetworkEvent& event) {
    if (event.type == NETWORK_EVENT_MATCH_LOAD) {
        free(event.match_load.noise);
    }
}

void network_disconnect() {
    if (state->status == NETWORK_STATUS_OFFLINE) {
        log_warn("network_disconnect() called while offline."); 
    }

    // Delete scanner
    if (state->scanner != nullptr) {
        network_destroy_scanner();
    }

    // Delete host
    if (state->host != nullptr) {
        if (state->status == NETWORK_STATUS_HOST) {
            state->host->close_lobby();
        }

        state->host->disconnect_peers();
        network_destroy_host();
    }

    state->status = NETWORK_STATUS_OFFLINE;
}

// LOBBIES

void network_search_lobbies(const char* query) {
    if (state->scanner == nullptr) {
        if (!network_create_scanner()) {
            log_error("Network backend %u lobby search failed. Could not create scanner.", state->backend);
            return;
        }
    }

    state->scanner->search_lobbies(query);
}

size_t network_get_lobby_count() {
    if (state->scanner == nullptr) {
        return 0;
    }
    return state->scanner->get_lobby_count();
}

const NetworkLobby& network_get_lobby(size_t index) {
    GOLD_ASSERT(state->scanner != nullptr);
    return state->scanner->get_lobby(index);
}

void network_open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy) {
    if (!network_create_host()) {
        state->events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        state->status = NETWORK_STATUS_OFFLINE;
        return;
    }

    if (state->scanner != nullptr) {
        network_destroy_scanner();
    }

    state->host->open_lobby(lobby_name, privacy);

    memset(state->players, 0, sizeof(state->players));
    memset(state->match_settings, 0, sizeof(state->match_settings));

    state->player_id = 0;
    strncpy(state->players[0].name, state->username, NETWORK_PLAYER_NAME_BUFFER_SIZE);
    state->players[0].status = NETWORK_PLAYER_STATUS_HOST;
    state->players[0].team = 0;

    log_info("Network %u opened lobby.", state->backend);
}

void network_join_lobby(const NetworkConnectionInfo& connection_info) {
    if (!network_create_host()) {
        state->events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        state->status = NETWORK_STATUS_OFFLINE;
        return;
    }

    if (!state->host->connect(connection_info)) {
        network_destroy_host();
        state->events.push((NetworkEvent) {
            .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
        });
        return;
    }

    if (state->scanner != nullptr) {
        network_destroy_scanner();
    }

    memset(state->players, 0, sizeof(state->players));
    state->status = NETWORK_STATUS_CONNECTING;
}

const char* network_get_lobby_name() {
    GOLD_ASSERT(state->host != nullptr);
    if (state->host == nullptr) {
        return "";
    }
    return state->host->get_lobby_name();
}

// Steam lobby invite

#ifdef GOLD_STEAM

void network_steam_accept_invite(CSteamID lobby_id) {
    log_debug("Called network_steam_accept_invite with lobby_id %u", lobby_id.ConvertToUint64());

    state->steam_invite_lobby_id = lobby_id;
    bool success = SteamMatchmaking()->RequestLobbyData(state->steam_invite_lobby_id);

    log_debug("Network steam requested lobby data. success? %i", (int)success);
}

// This is called whenever the player accepts an invite from Steam while in-game
// network_steam_accept_invite handles the actual logic. This is done so that we can accept
// invites that players accept while in-game in the same fashion as we accept invites from 
// the command line (when steam launches goldrush with +connect_lobby <lobby_id>)
void NetworkState::on_steam_lobby_join_requested(GameLobbyJoinRequested_t* join_requested) {
    network_steam_accept_invite(join_requested->m_steamIDLobby);
}

void NetworkState::on_steam_lobby_data_update(LobbyDataUpdate_t* lobby_data_update) {
    log_debug("Network Steam received LobbyDataUpdate callback.");

    if (state->status != NETWORK_STATUS_OFFLINE || !state->steam_invite_lobby_id.IsValid()) {
        log_debug("We are in a game are have been invited to something already, ignoring.");
        return;
    }

    if (!lobby_data_update->m_bSuccess) {
        log_warn("LobbyDataUpdate was not successful.");
        return;
    }
    if (lobby_data_update->m_ulSteamIDLobby != state->steam_invite_lobby_id.ConvertToUint64()) {
        log_debug("LobbyDataUpdate lobby ID does not match the steam invite lobby ID.");
        return;
    }

    NetworkEvent event;
    event.type = NETWORK_EVENT_STEAM_INVITE;
    strncpy(event.steam_invite.connection_info.data, SteamMatchmaking()->GetLobbyData(state->steam_invite_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY), sizeof(event.steam_invite.connection_info.data));
    state->events.push(event);
    state->steam_invite_lobby_id.Clear();

    log_debug("Network Steam pushed NETWORK_EVENT_STEAM_INVITE");
}

#endif

// MESSAGES

void network_send_chat(const char* message) {
    NetworkMessageChat out_message;
    strcpy(out_message.message, message);

    state->host->broadcast(&out_message, sizeof(out_message));
    state->host->flush();
}

void network_set_player_ready(bool ready) {
    state->players[state->player_id].status = ready ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;

    uint8_t message = ready ? NETWORK_MESSAGE_SET_READY : NETWORK_MESSAGE_SET_NOT_READY;
    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_set_player_color(uint8_t player_id, uint8_t color) {
    state->players[player_id].recolor_id = color;

    NetworkMessageSetColor message;
    message.recolor_id = color;
    message.player_id = player_id;

    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_set_match_setting(uint8_t setting, uint8_t value) {
    NetworkMessageSetMatchSetting message;
    message.setting = setting;
    message.value = value;
    state->match_settings[setting] = value;

    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

uint8_t network_get_match_setting(uint8_t setting) {
    return state->match_settings[setting];
}

void network_set_player_team(uint8_t player_id, uint8_t team) {
    state->players[player_id].team = team;

    NetworkMessageSetTeam message;
    message.player_id = player_id;
    message.team = team;

    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_add_bot() {
    if (network_get_player_count() == MAX_PLAYERS) {
        log_warn("Called network_add_bot() in a full lobby.");
        return;
    }

    // Determine bot player ID
    uint8_t bot_player_id;
    for (bot_player_id = 0; bot_player_id < MAX_PLAYERS; bot_player_id++) {
        if (state->players[bot_player_id].status == NETWORK_PLAYER_STATUS_NONE) {
            break;
        }
    }
    GOLD_ASSERT(bot_player_id != MAX_PLAYERS);

    // Determine bot team
    uint8_t team0_player_count = 0;
    uint8_t team1_player_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->players[player_id].status != NETWORK_PLAYER_STATUS_NONE) {
            if (state->players[player_id].team == 0) {
                team0_player_count++;
            } else if (state->players[player_id].team == 1) {
                team1_player_count++;
            }
        }
    }
    uint8_t bot_team = team0_player_count < team1_player_count ? 0 : 1;

    // Determine bot color
    uint8_t recolor_id;
    for (recolor_id = 0; recolor_id < MAX_PLAYERS; recolor_id++) {
        bool player_has_color = false;
        for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
            if (state->players[player_id].status != NETWORK_PLAYER_STATUS_NONE && state->players[player_id].recolor_id == recolor_id) {
                player_has_color = true;
                break;
            }
        }
        if (!player_has_color) {
            break;
        }
    }
    GOLD_ASSERT(recolor_id != MAX_PLAYERS);

    NetworkMessageAddBot message;
    message.player_id = bot_player_id;
    message.team = bot_team;
    message.recolor_id = recolor_id;

    state->players[bot_player_id].status = NETWORK_PLAYER_STATUS_BOT;
    sprintf(state->players[bot_player_id].name, "Bot");
    state->players[bot_player_id].team = bot_team;
    state->players[bot_player_id].recolor_id = recolor_id;

    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_remove_bot(uint8_t player_id) {
    if (state->players[player_id].status != NETWORK_PLAYER_STATUS_BOT) {
        log_warn("Tried to kick non-bot player with id %u, status %u", player_id, state->players[player_id].status);
        return;
    }

    state->players[player_id].status = NETWORK_PLAYER_STATUS_NONE;

    NetworkMessageRemoveBot message;
    message.player_id = player_id;

    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_begin_load_match_countdown() {
    uint8_t message = NETWORK_MESSAGE_LOAD_MATCH_COUNTDOWN;
    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_begin_loading_match(int32_t lcg_seed, const Noise* noise) {
    // It should be fine to call close_lobby() because network_begin_loading_match() is only called on the match host side

    // Set all players to NOT_READY so that they can re-ready themselves once they enter the match
    network_set_players_not_ready();

    state->status = NETWORK_STATUS_CONNECTED;

    // Build message
    // Message size is 1 byte for type, 4 bytes for LCG seed, 8 bytes for map width / height, and the rest of the bytes are the generated noise values
    size_t message_size = sizeof(uint8_t) + sizeof(int32_t) + noise_serialized_size(noise);
    uint8_t* message = (uint8_t*)malloc(message_size);
    size_t message_offset = 0;

    message[0] = NETWORK_MESSAGE_LOAD_MATCH;
    message_offset += sizeof(uint8_t);

    memcpy(message + message_offset, &lcg_seed, sizeof(int32_t));
    message_offset += sizeof(int32_t);

    noise_serialize(noise, message + message_offset);

    // Send the packet
    state->host->broadcast(message, message_size);
    state->host->flush();

    free(message);
}

void network_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    out_buffer[0] = NETWORK_MESSAGE_INPUT;
    state->host->broadcast(out_buffer, out_buffer_length);
    state->host->flush();
}

void network_send_checksum(uint32_t checksum) {
    NetworkMessageChecksum message;
    message.checksum = checksum;

    state->host->broadcast(&message, sizeof(message));
    state->host->flush();
}

void network_send_serialized_frame(uint8_t* state_buffer, size_t state_buffer_length) {
    state_buffer[0] = NETWORK_MESSAGE_SERIALIZED_FRAME;
    state->host->broadcast(state_buffer, state_buffer_length);
    state->host->flush();
}

// INTERNAL

bool network_create_host() {
    switch (state->backend) {
        case NETWORK_BACKEND_LAN:
            state->host = new NetworkHostLan();
            break;
    #ifdef GOLD_STEAM
        case NETWORK_BACKEND_STEAM:
            state->host = new NetworkHostSteam();
            break;
    #endif
    }

    if (!state->host->is_initialized_successfully()) {
        network_destroy_host();
        return false;
    }

    return true;
}

void network_destroy_host() {
    delete state->host;
    state->host = nullptr;
}

bool network_create_scanner() {
    switch (state->backend) {
        case NETWORK_BACKEND_LAN:
            state->scanner = new NetworkScannerLan();
            break;
    #ifdef GOLD_STEAM
        case NETWORK_BACKEND_STEAM:
            state->scanner = new NetworkScannerSteam();
            break;
    #endif
    }

    if (!state->scanner->is_initialized_successfully()) {
        network_destroy_scanner();
        return false;
    }

    return true;
}

void network_destroy_scanner() {
    delete state->scanner;
    state->scanner = nullptr;
}

void network_set_players_not_ready() {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->players[player_id].status == NETWORK_PLAYER_STATUS_READY || state->players[player_id].status == NETWORK_PLAYER_STATUS_HOST) {
            state->players[player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;
        }
    }
}

void network_handle_message(uint16_t incoming_peer_id, uint8_t* data, size_t length) {
    uint8_t message_type = data[0];

    switch (message_type) {
        case NETWORK_MESSAGE_GREET_SERVER: {
            // Host class will handle the lobby is full scenario
            // Check if lobby is full
            if (network_get_player_count() == MAX_PLAYERS) {
                log_info("Received new player but lobby is full. Rejecting client...");

                uint8_t message = NETWORK_MESSAGE_LOBBY_FULL;
                state->host->send(incoming_peer_id, &message, sizeof(message));
                state->host->flush();

                return;
            }

            NetworkMessageGreetServer* incoming_message = (NetworkMessageGreetServer*)data;
            
            // Check that the server is not already in a game
            if (state->players[state->player_id].status != NETWORK_PLAYER_STATUS_HOST) {
                log_info("Client tried to connect while server is no longer in lobby. Rejecting...");

                uint8_t message = NETWORK_MESSAGE_GAME_ALREADY_STARTED;
                state->host->send(incoming_peer_id, &message, sizeof(message));
                state->host->flush();

                return;
            }

            // Check the client version
            if (strcmp(incoming_message->app_version, APP_VERSION) != 0) {
                log_info("Client app version mismatch. Rejecting client...");

                uint8_t message = NETWORK_MESSAGE_INVALID_VERSION;
                state->host->send(incoming_peer_id, &message, sizeof(message));
                state->host->flush();

                return;
            }

            // Determine incoming player id
            uint8_t incoming_player_id;
            for (incoming_player_id = 0; incoming_player_id < MAX_PLAYERS; incoming_player_id++) {
                if (state->players[incoming_player_id].status == NETWORK_PLAYER_STATUS_NONE) {
                    break;
                }
            }
            // Determine incoming player recolor id
            uint8_t incoming_player_recolor_id;
            for (incoming_player_recolor_id = 0; incoming_player_recolor_id < MAX_PLAYERS; incoming_player_recolor_id++) {
                bool is_recolor_id_in_use = false;
                for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                    if (player_id == incoming_player_id || state->players[player_id].status == NETWORK_PLAYER_STATUS_NONE) {
                        continue;
                    }

                    if (state->players[player_id].recolor_id == incoming_player_recolor_id) {
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
                if (state->players[player_id].status == NETWORK_PLAYER_STATUS_NONE || player_id == incoming_player_id) {
                    continue;
                }

                if (state->players[player_id].team == 0) {
                    team1_count++;
                } else {
                    team2_count++;
                }
            }

            GOLD_ASSERT(incoming_player_id != MAX_PLAYERS && incoming_player_recolor_id != MAX_PLAYERS);

            // Set incoming player info
            strncpy(state->players[incoming_player_id].name, incoming_message->username, MAX_USERNAME_LENGTH + 1);
            state->players[incoming_player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;
            state->players[incoming_player_id].recolor_id = incoming_player_recolor_id;
            state->players[incoming_player_id].team = team1_count <= team2_count ? 0 : 1;

            // Setup incoming player / peer mapping
            state->host->set_peer_player_id(incoming_peer_id, incoming_player_id);

            // Send the welcome packet
            NetworkMessageWelcome response;
            response.incoming_player_id = incoming_player_id;
            response.incoming_recolor_id = incoming_player_recolor_id;
            response.incoming_team = state->players[incoming_player_id].team;
            response.server_recolor_id = state->players[0].recolor_id;
            response.server_team = state->players[0].team;
            strncpy(response.server_username, state->players[0].name, MAX_USERNAME_LENGTH + 1);
            strncpy(response.lobby_name, state->host->get_lobby_name(), NETWORK_LOBBY_NAME_BUFFER_SIZE);
            memcpy(response.match_settings, state->match_settings, sizeof(state->match_settings));

            state->host->send(incoming_peer_id, &response, sizeof(response));
            log_debug("Sent welcome packet to player %u", incoming_player_id);

            // Notify the new player of any bots
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                if (state->players[player_id].status == NETWORK_PLAYER_STATUS_BOT) {
                    NetworkMessageAddBot add_bot_message;
                    add_bot_message.player_id = player_id;
                    add_bot_message.team = state->players[player_id].team;
                    add_bot_message.recolor_id = state->players[player_id].recolor_id;
                    state->host->send(incoming_peer_id, &add_bot_message, sizeof(add_bot_message));
                }
            }

            // Build new player message
            NetworkMessageNewPlayer new_player_message;
            new_player_message.connection_info = state->host->get_peer_connection_info(incoming_peer_id);

            // Tell the other players about this new one
            for (uint16_t peer_id = 0; peer_id < state->host->get_peer_count(); peer_id++) {
                if (peer_id == incoming_peer_id) {
                    continue;
                }

                state->host->send(peer_id, &new_player_message, sizeof(new_player_message));
            }
            state->host->flush();

            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_PLAYER_CONNECTED,
                .player_connected = (NetworkEventPlayerConnected) {
                    .player_id = incoming_player_id
                }
            });
            break;
        }
        case NETWORK_MESSAGE_INVALID_VERSION: {
            log_info("Client version does not match the server.");
            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_INVALID_VERSION
            });
            break;
        }
        case NETWORK_MESSAGE_LOBBY_FULL: {
            log_info("Client received lobby full from server.");
            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_FULL
            });
            break;
        }
        case NETWORK_MESSAGE_WELCOME: {
            log_info("Joined lobby.");
            state->status = NETWORK_STATUS_CONNECTED;

            NetworkMessageWelcome* incoming_message = (NetworkMessageWelcome*)data;

            // Setup current player info
            state->player_id = incoming_message->incoming_player_id;
            state->players[state->player_id].recolor_id = incoming_message->incoming_recolor_id;
            state->players[state->player_id].team = incoming_message->incoming_team;
            strncpy(state->players[state->player_id].name, state->username, MAX_USERNAME_LENGTH + 1);
            state->players[state->player_id].status = NETWORK_PLAYER_STATUS_NOT_READY;

            // Setup host player info
            state->players[0].status = NETWORK_PLAYER_STATUS_HOST;
            strncpy(state->players[0].name, incoming_message->server_username, MAX_USERNAME_LENGTH + 1);
            state->players[0].recolor_id = incoming_message->server_recolor_id;
            state->players[0].team = incoming_message->server_team;
            state->host->set_peer_player_id(incoming_peer_id, 0);

            // Get lobby name and settings
            state->host->set_lobby_name(incoming_message->lobby_name);
            memcpy(state->match_settings, incoming_message->match_settings, sizeof(state->match_settings));

            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_LOBBY_CONNECTED
            });
            break;
        }
        case NETWORK_MESSAGE_NEW_PLAYER: {
            if (state->status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkMessageNewPlayer* incoming_message = (NetworkMessageNewPlayer*)data;
            if (!state->host->connect(incoming_message->connection_info)) {
                log_error("Unable to connect to new player.");
                return;
            }

            break;
        }
        case NETWORK_MESSAGE_GREET_CLIENT: {
            if (state->status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkMessageGreetClient* incoming_message = (NetworkMessageGreetClient*)data;

            memcpy(&state->players[incoming_message->player_id], &incoming_message->player, sizeof(NetworkPlayer));
            state->host->set_peer_player_id(incoming_peer_id, incoming_message->player_id);
            break;
        }
        case NETWORK_MESSAGE_SET_READY:
        case NETWORK_MESSAGE_SET_NOT_READY: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            uint8_t player_id = state->host->get_peer_player_id(incoming_peer_id); 
            state->players[player_id].status = message_type == NETWORK_MESSAGE_SET_READY ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY;
            break;
        }
        case NETWORK_MESSAGE_SET_COLOR: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkMessageSetColor* incoming_message = (NetworkMessageSetColor*)data;
            state->players[incoming_message->player_id].recolor_id = incoming_message->recolor_id;
            break;
        }
        case NETWORK_MESSAGE_SET_TEAM: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkMessageSetTeam* incoming_message = (NetworkMessageSetTeam*)data;
            state->players[incoming_message->player_id].team = incoming_message->team;
            break;
        }
        case NETWORK_MESSAGE_CHAT: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkMessageChat* incoming_message = (NetworkMessageChat*)data;

            NetworkEvent event;
            event.type = NETWORK_EVENT_CHAT;
            event.chat.player_id = state->host->get_peer_player_id(incoming_peer_id);
            strncpy(event.chat.message, incoming_message->message, NETWORK_CHAT_BUFFER_SIZE);
            state->events.push(event);

            break;
        }
        case NETWORK_MESSAGE_SET_MATCH_SETTING: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkMessageSetMatchSetting* incoming_message = (NetworkMessageSetMatchSetting*)data;
            state->match_settings[incoming_message->setting] = incoming_message->value;
            break;
        }
        case NETWORK_MESSAGE_ADD_BOT: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkMessageAddBot* incoming_message = (NetworkMessageAddBot*)data;

            state->players[incoming_message->player_id].status = NETWORK_PLAYER_STATUS_BOT;
            sprintf(state->players[incoming_message->player_id].name, "Bot");
            state->players[incoming_message->player_id].team = incoming_message->team;
            state->players[incoming_message->player_id].recolor_id = incoming_message->recolor_id;
            break;
        }
        case NETWORK_MESSAGE_REMOVE_BOT: {
            if (!(state->status == NETWORK_STATUS_CONNECTED || state->status == NETWORK_STATUS_HOST)) {
                return;
            }

            NetworkMessageRemoveBot* incoming_message = (NetworkMessageRemoveBot*)data;

            state->players[incoming_message->player_id].status = NETWORK_PLAYER_STATUS_NONE;

            break;
        }
        case NETWORK_MESSAGE_LOAD_MATCH_COUNTDOWN: {
            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_MATCH_LOAD_COUNTDOWN
            });
            break;
        }
        case NETWORK_MESSAGE_LOAD_MATCH: {
            if (state->status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            if (state->scanner != nullptr) {
                network_destroy_scanner();
            }

            network_set_players_not_ready();

            NetworkEvent event;
            event.type = NETWORK_EVENT_MATCH_LOAD;
            memcpy(&event.match_load.lcg_seed, data + sizeof(uint8_t), sizeof(int));
            event.match_load.noise = noise_deserialize(data + sizeof(uint8_t) + sizeof(uint32_t));

            state->events.push(event);
            break;
        }
        case NETWORK_MESSAGE_INPUT: {
            if (state->status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            NetworkEvent event;
            event.type = NETWORK_EVENT_INPUT;
            event.input.in_buffer_length = (uint32_t)length;
            event.input.player_id = state->host->get_peer_player_id(incoming_peer_id);
            GOLD_ASSERT(length <= 1024);
            memcpy(event.input.in_buffer, data, length);

            state->events.push(event);

            break;
        }
        case NETWORK_MESSAGE_CHECKSUM: {
            if (state->status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            uint8_t player_id = state->host->get_peer_player_id(incoming_peer_id); 

            NetworkMessageChecksum* incoming_message = (NetworkMessageChecksum*)data;

            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_CHECKSUM,
                .checksum = (NetworkEventChecksum) {
                    .player_id = player_id,
                    .checksum = incoming_message->checksum
                }
            });
            break;
        }
        case NETWORK_MESSAGE_SERIALIZED_FRAME: {
            if (state->status != NETWORK_STATUS_CONNECTED) {
                return;
            }

            log_debug("NETWORK Received serialized frame.");
            uint8_t* state_buffer = (uint8_t*)malloc(length);
            if (state_buffer == NULL) {
                log_error("NETWORK could not malloc state_buffer");
                return;
            }
            memcpy(state_buffer, data, length);

            state->events.push((NetworkEvent) {
                .type = NETWORK_EVENT_SERIALIZED_FRAME,
                .serialized_frame = (NetworkEventSerializedFrame) {
                    .state_buffer = state_buffer
                }
            });
            break;
        }
        default: {
            log_warn("NETWORK Received unrecognized message type %u", message_type);
            break;
        }
    }
}