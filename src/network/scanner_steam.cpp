#include "scanner_steam.h"

#ifdef GOLD_STEAM

#include "core/logger.h"

bool NetworkScannerSteam::is_initialized_successfully() const {
    return true;
}

void NetworkScannerSteam::search_lobbies(const char* query) {
    strncpy(scanner_lobby_name_query, query, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    scanner_lobbies.clear();

    SteamMatchmaking()->RequestLobbyList();
}

void NetworkScannerSteam::service() {
    // does nothing
}

void NetworkScannerSteam::on_lobby_matchlist(LobbyMatchList_t* lobby_matchlist) {
    for (uint32_t lobby_index = 0; lobby_index < lobby_matchlist->m_nLobbiesMatching; lobby_index++) {
        CSteamID lobby_id = SteamMatchmaking()->GetLobbyByIndex(lobby_index);
        NetworkLobby lobby;
        strncpy(lobby.name, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);
        memcpy(&lobby.player_count, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT), sizeof(uint8_t));
        strncpy(lobby.connection_info.data, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY), sizeof(lobby.connection_info));
        log_debug("Found lobby %s host steam identity %s lobby query %s", lobby.name, lobby.connection_info, scanner_lobby_name_query);

        if (strlen(scanner_lobby_name_query) == 0 || strstr(lobby.name, scanner_lobby_name_query) != NULL) {
            log_debug("added lobby.");
            scanner_lobbies.push_back(lobby);
        }
    }
}

#endif