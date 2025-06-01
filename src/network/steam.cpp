#include "steam.h"

#include "core/logger.h"
#include "event.h"
#include <steam/steam_api.h>
#include <vector>

static const char* LOBBY_PROPERTY_NAME = "name";

struct NetworkSteamLobbyInternal {
    CSteamID id;
    NetworkLobby lobby;
};

struct NetworkSteamState {
    NetworkStatus status;
    char lobby_search_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    std::vector<NetworkSteamLobbyInternal> lobbies;
    NetworkSteamLobbyInternal internal_lobby;
};
static NetworkSteamState state;

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

                log_info("Created lobby.");

                state.internal_lobby.id = lobby_created->m_ulSteamIDLobby;
                SteamMatchmaking()->SetLobbyData(state.internal_lobby.id, LOBBY_PROPERTY_NAME, state.internal_lobby.lobby.name);
                state.status = NETWORK_STATUS_HOST;

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

                log_info("Joined lobby.");

                // TODO: server will experience this callback so don't do anything if we are the server
                state.internal_lobby.id = lobby_entered->m_ulSteamIDLobby;
                strncpy(state.internal_lobby.lobby.name, SteamMatchmaking()->GetLobbyData(state.internal_lobby.id, LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);
                state.internal_lobby.lobby.player_count = SteamMatchmaking()->GetNumLobbyMembers(state.internal_lobby.id);

                break;
            }
        }
        SteamAPI_ManualDispatch_FreeLastCallback(steam_pipe);
    }
}

void network_steam_disconnect() {

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
    strncpy(state.internal_lobby.lobby.name, lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    state.internal_lobby.lobby.player_count = 1;
    log_info("requested lobby creation");
}

void network_steam_join_lobby(uint32_t index) {
    SteamMatchmaking()->JoinLobby(state.lobbies[index].id);
    state.status = NETWORK_STATUS_CONNECTING;
    log_info("requested lobby join");
}

NetworkStatus network_steam_get_status() {
    return state.status;
}

const NetworkPlayer& network_steam_get_player(uint8_t player_id) {
    // TODO
    static NetworkPlayer player;
    return player;
}

uint8_t network_steam_get_player_id() {
    // TODO
    return 0;
}

const char* network_steam_get_lobby_name() {
    // TODO
    return "";
}

void network_steam_send_chat(const char* message) {
    // TODO
}

void network_steam_set_player_ready(bool ready) {
    // TODO
}

void network_steam_set_player_color(uint8_t color) {
    // TODO
}

void network_steam_set_match_setting(uint8_t setting, uint8_t value) {
    // TODO
}

uint8_t network_steam_get_match_setting(uint8_t setting) {
    // TODO
    return 0;
}

void network_steam_set_player_team(uint8_t team) {
    // TODO
}

void network_steam_begin_loading_match(int32_t lcg_seed, const Noise& noise) {
    // TODO
}

void network_steam_send_input(uint8_t* out_buffer, size_t out_buffer_length) {
    // TODO
}