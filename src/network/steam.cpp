#include "steam.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "event.h"
#include <steam/steam_api.h>
#include <vector>

static const char* LOBBY_PROPERTY_NAME = "name";
static const char* PLAYER_PROPERTY_STATUS = "status";
static const char* PLAYER_PROPERTY_RECOLOR_ID = "recolor_id";
static const char* PLAYER_PROPERTY_TEAM = "team";

struct NetworkSteamLobbyInternal {
    CSteamID id;
    NetworkLobby lobby;
};

struct NetworkSteamState {
    NetworkStatus status;

    uint8_t player_id;
    NetworkPlayer players[MAX_PLAYERS];

    char lobby_search_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    std::vector<NetworkSteamLobbyInternal> lobbies;

    CSteamID lobby_id;
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
};
static NetworkSteamState state;

void network_steam_set_player_status(NetworkPlayerStatus status);
void network_steam_cache_player_info();

void network_steam_init() {
    SteamAPI_ManualDispatch_Init();
    log_info("Initialized network steam.");
}

void network_steam_service() {
    HSteamPipe steam_pipe = SteamAPI_GetHSteamPipe();
    SteamAPI_ManualDispatch_RunFrame(steam_pipe);
    CallbackMsg_t callback;
    while (SteamAPI_ManualDispatch_GetNextCallback(steam_pipe, &callback)) {
        switch (callback.m_iCallback) {
            case LobbyMatchList_t::k_iCallback: {
                LobbyMatchList_t* lobby_matchlist = (LobbyMatchList_t*)callback.m_pubParam;
                log_trace("Lobby matchlist count %u", lobby_matchlist->m_nLobbiesMatching);

                for (uint32_t lobby_index = 0; lobby_index < lobby_matchlist->m_nLobbiesMatching; lobby_index++) {
                    NetworkSteamLobbyInternal internal_lobby;
                    internal_lobby.id = SteamMatchmaking()->GetLobbyByIndex(lobby_index);
                    strncpy(internal_lobby.lobby.name, SteamMatchmaking()->GetLobbyData(internal_lobby.id, LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);

                    if (strstr(internal_lobby.lobby.name, state.lobby_search_query) == NULL) {
                        continue;
                    }

                    internal_lobby.lobby.player_count = SteamMatchmaking()->GetNumLobbyMembers(internal_lobby.id);
                    state.lobbies.push_back(internal_lobby);
                }

                break;
            }
            case LobbyCreated_t::k_iCallback: {
                LobbyCreated_t* lobby_created = (LobbyCreated_t*)callback.m_pubParam;

                if (lobby_created->m_eResult != k_EResultOK) {
                    log_error("Error creating lobby");
                    state.status = NETWORK_STATUS_OFFLINE;
                    network_push_event((NetworkEvent) {
                        .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
                    });
                    break;
                }

                state.lobby_id = lobby_created->m_ulSteamIDLobby;
                state.status = NETWORK_STATUS_HOST;

                SteamMatchmaking()->SetLobbyData(state.lobby_id, LOBBY_PROPERTY_NAME, state.lobby_name);
                network_steam_set_player_status(NETWORK_PLAYER_STATUS_HOST);
                network_steam_set_player_color(0);
                network_steam_set_player_team(0);

                network_push_event((NetworkEvent) {
                    .type = NETWORK_EVENT_LOBBY_CREATED
                });
                log_info("Created steam lobby.");

                break;
            }
            case LobbyEnter_t::k_iCallback: {
                LobbyEnter_t* lobby_entered = (LobbyEnter_t*)callback.m_pubParam;

                if (lobby_entered->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess) {
                    log_error("Error joining lobby");
                    state.status = NETWORK_STATUS_OFFLINE;
                    network_push_event((NetworkEvent) {
                        .type = NETWORK_EVENT_LOBBY_CONNECTION_FAILED
                    });
                    break;
                }

                if (state.status == NETWORK_STATUS_HOST) {
                    break;
                }

                state.lobby_id = lobby_entered->m_ulSteamIDLobby;
                strncpy(state.lobby_name, SteamMatchmaking()->GetLobbyData(state.lobby_id, LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);
                state.status = NETWORK_STATUS_CONNECTED;

                network_steam_cache_player_info();

                uint8_t next_recolor_id;
                for (next_recolor_id = 0; next_recolor_id < MAX_PLAYERS; next_recolor_id++) {
                    bool is_color_in_use = false;
                    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                        if (state.players[player_id].status != NETWORK_PLAYER_STATUS_NONE && 
                                state.players[player_id].recolor_id == next_recolor_id) {
                            is_color_in_use = true;
                            break;
                        }
                    }
                    if (!is_color_in_use) {
                        break;
                    }
                }

                // TODO
                uint8_t incoming_player_team = network_steam_get_player_id();

                network_steam_set_player_status(NETWORK_PLAYER_STATUS_NOT_READY);
                network_steam_set_player_color(next_recolor_id);
                network_steam_set_player_team(incoming_player_team);

                network_push_event((NetworkEvent) {
                    .type = NETWORK_EVENT_LOBBY_JOIN_SUCCESS
                });
                log_info("Joined steam lobby.");

                break;
            }
            case LobbyDataUpdate_t::k_iCallback: {
                LobbyDataUpdate_t* lobby_data_update = (LobbyDataUpdate_t*)callback.m_pubParam;

                if (!lobby_data_update->m_bSuccess) {
                    log_warn("Lobby data unable to update.");
                    break;
                }

                // We only care about updates from our own lobby
                if (lobby_data_update->m_ulSteamIDLobby != state.lobby_id.ConvertToUint64()) {
                    break;
                }

                network_steam_cache_player_info();

                break;
            }
        }
        SteamAPI_ManualDispatch_FreeLastCallback(steam_pipe);
    }
}

void network_steam_disconnect() {
    state.lobby_id.Clear();
}

void network_steam_search_lobbies(const char* query) {
    state.lobbies.clear();
    strncpy(state.lobby_search_query, query, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    SteamMatchmaking()->RequestLobbyList();
}

uint32_t network_steam_get_lobby_count() {
    return (uint32_t)state.lobbies.size();
}

const NetworkLobby& network_steam_get_lobby(uint32_t index) {
    return state.lobbies.at(index).lobby;
}

void network_steam_create_lobby(const char* lobby_name) {
    SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, MAX_PLAYERS);
    state.status = NETWORK_STATUS_CONNECTING;
    strncpy(state.lobby_name, lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
}

void network_steam_join_lobby(uint32_t index) {
    SteamMatchmaking()->JoinLobby(state.lobbies[index].id);
    state.status = NETWORK_STATUS_CONNECTING;
}

NetworkStatus network_steam_get_status() {
    return state.status;
}

const NetworkPlayer& network_steam_get_player(uint8_t player_id) {
    return state.players[player_id];
}

uint8_t network_steam_get_player_id() {
    if (state.lobby_id.IsValid()) {
        for (uint8_t player_id = 0; player_id < SteamMatchmaking()->GetNumLobbyMembers(state.lobby_id); player_id++) {
            if (SteamMatchmaking()->GetLobbyMemberByIndex(state.lobby_id, player_id) == SteamUser()->GetSteamID()) {
                return player_id;
            }
        }
        GOLD_ASSERT(false);
    }
    return 0;
}

const char* network_steam_get_lobby_name() {
    return state.lobby_name;
}

void network_steam_send_chat(const char* message) {
    // TODO
}

void network_steam_set_player_ready(bool ready) {
    network_steam_set_player_status(ready ? NETWORK_PLAYER_STATUS_READY : NETWORK_PLAYER_STATUS_NOT_READY);
}

void network_steam_set_player_color(uint8_t color) {
    uint8_t buffer[2] = { color, '\0' };
    SteamMatchmaking()->SetLobbyMemberData(state.lobby_id, PLAYER_PROPERTY_RECOLOR_ID, (const char*)buffer);
}

void network_steam_set_match_setting(uint8_t setting, uint8_t value) {
    // TODO
}

uint8_t network_steam_get_match_setting(uint8_t setting) {
    // TODO
    return 0;
}

void network_steam_set_player_team(uint8_t team) {
    uint8_t buffer[2] = { team, '\0' };
    SteamMatchmaking()->SetLobbyMemberData(state.lobby_id, PLAYER_PROPERTY_TEAM, (const char*)buffer);
}

void network_steam_begin_loading_match(int32_t lcg_seed, const Noise& noise) {
    // TODO
}

void network_steam_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    // TODO
}

// INTERNAL

void network_steam_set_player_status(NetworkPlayerStatus status) {
    uint8_t buffer[2] = { (uint8_t)status, '\0' };
    SteamMatchmaking()->SetLobbyMemberData(state.lobby_id, PLAYER_PROPERTY_STATUS, (const char*)buffer);
}

void network_steam_cache_player_info() {
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (player_id >= SteamMatchmaking()->GetNumLobbyMembers(state.lobby_id)) {
            state.players[player_id].status = NETWORK_PLAYER_STATUS_NONE;
            continue;
        }

        CSteamID steam_id = SteamMatchmaking()->GetLobbyMemberByIndex(state.lobby_id, player_id);
        strncpy(state.players[player_id].name, SteamFriends()->GetFriendPersonaName(steam_id), NETWORK_PLAYER_NAME_BUFFER_SIZE);
        state.players[player_id].status = (uint8_t)(SteamMatchmaking()->GetLobbyMemberData(state.lobby_id, steam_id, PLAYER_PROPERTY_STATUS)[0]);
        state.players[player_id].recolor_id = (uint8_t)(SteamMatchmaking()->GetLobbyMemberData(state.lobby_id, steam_id, PLAYER_PROPERTY_RECOLOR_ID)[0]);
        state.players[player_id].team = (uint8_t)(SteamMatchmaking()->GetLobbyMemberData(state.lobby_id, steam_id, PLAYER_PROPERTY_TEAM)[0]);
    }
}