#include "lobby.h"

#include "core/logger.h"

void NetworkSteamLobby::open(const char* name, NetworkConnectionInfo connection_info) {
    strncpy(lobby_name, name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    lobby_player_count = 1;
    SteamAPICall_t api_call = SteamMatchmaking()->CreateLobby(k_ELobbyTypePublic, 2);
    call_result_lobby_created.Set(api_call, this, &NetworkSteamLobby::on_lobby_created);
    status = NETWORK_LOBBY_OPENING;
}

void NetworkSteamLobby::close() {
    SteamMatchmaking()->LeaveLobby(lobby_id);
    status = NETWORK_LOBBY_CLOSED;
}

void NetworkSteamLobby::service() {

}

void NetworkSteamLobby::set_player_count(uint8_t player_count) {
    lobby_player_count = player_count;
    char buffer[2] = { (char)player_count, '\0' };
    SteamMatchmaking()->SetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT, buffer);
}

void NetworkSteamLobby::on_lobby_created(LobbyCreated_t* lobby_created, bool io_failure) {
    if (lobby_created->m_eResult != k_EResultOK) {
        log_error("Error creating steam lobby.");
        status = NETWORK_LOBBY_ERROR;
        return;
    }

    lobby_id = lobby_created->m_ulSteamIDLobby;
    SteamMatchmaking()->SetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME, lobby_name);

    // Set lobby connection info
    char host_steam_id[16];
    uint64_t steam_id = SteamUser()->GetSteamID().ConvertToUint64();
    memcpy(host_steam_id, &steam_id, sizeof(uint64_t));
    host_steam_id[sizeof(uint64_t)] = '\0';
    SteamMatchmaking()->SetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_STEAM_ID, host_steam_id);

    set_player_count(lobby_player_count);
    status = NETWORK_LOBBY_OPEN;
}