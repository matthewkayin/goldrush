#include "scanner.h"

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
        // Get the lobby_id of this result
        CSteamID lobby_id = SteamMatchmaking()->GetLobbyByIndex(lobby_index);

        // Populate the lobby struct by querying the data for this lobby "lobby_id"
        NetworkLobby lobby;

        // Get lobby name
        strncpy(lobby.name, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);

        // Get lobby player count
        memcpy(&lobby.player_count, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT), sizeof(uint8_t));

        // Get lobby connection string
        strncpy(lobby.connection_info.steam.identity_str, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY), sizeof(lobby.connection_info));

        log_debug("Found lobby %s host steam identity %s lobby query %s", lobby.name, lobby.connection_info, scanner_lobby_name_query);

        if (strlen(scanner_lobby_name_query) == 0 || strstr(lobby.name, scanner_lobby_name_query) != NULL) {
            scanner_lobbies.push_back(lobby);
        }
    }
}

#endif