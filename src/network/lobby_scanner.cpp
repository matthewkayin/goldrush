#include "lobby_scanner.h"

#include "core/logger.h"
#include "core/asserts.h"

// BASE

size_t NetworkLobbyScanner::get_matchlist_size() const {
    return matchlist.size();
}

const NetworkMatchlistEntry& NetworkLobbyScanner::get_matchlist_entry(size_t index) const {
    return matchlist[index];
}

// LAN

NetworkLanLobbyScanner::NetworkLanLobbyScanner() {
    scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create lobby scanner");
        return;
    }
    enet_socket_set_option(scanner, ENET_SOCKOPT_BROADCAST, 1);
}

NetworkLanLobbyScanner::~NetworkLanLobbyScanner() {
    if (scanner != ENET_SOCKET_NULL) {
        enet_socket_shutdown(scanner, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(scanner);
    }
}

bool NetworkLanLobbyScanner::was_create_successful() const {
    return scanner != ENET_SOCKET_NULL;
}

void NetworkLanLobbyScanner::search(const char* query) {
    GOLD_ASSERT(scanner != ENET_SOCKET_NULL);

    strncpy(lobby_name_query, query, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    matchlist.clear();

    ENetAddress scan_address;
    scan_address.host = ENET_HOST_BROADCAST;
    scan_address.port = NETWORK_SCANNER_PORT;

    char buffer = 3;
    ENetBuffer send_buffer;
    send_buffer.data = &buffer;
    send_buffer.dataLength = 1;
    if (enet_socket_send(scanner, &scan_address, &send_buffer, 1) != 1) {
        log_error("Failed to scan for LAN servers.");
    }
}

void NetworkLanLobbyScanner::service() {
    GOLD_ASSERT(scanner != ENET_SOCKET_NULL);

    ENetSocketSet set;
    ENET_SOCKETSET_EMPTY(set);
    ENET_SOCKETSET_ADD(set, scanner);
    while (enet_socketset_select(scanner, &set, NULL, 0) > 0) {
        ENetAddress receive_address;
        ENetBuffer receive_buffer;
        char buffer[128];
        receive_buffer.data = &buffer;
        receive_buffer.dataLength = sizeof(buffer);
        if (enet_socket_receive(scanner, &receive_address, &receive_buffer, 1) <= 0) {
            continue;
        }

        NetworkLanLobbyInfo lobby_info;
        memcpy(&lobby_info, buffer, sizeof(NetworkLanLobbyInfo));
        NetworkMatchlistEntry entry;
        memcpy(&entry.name, lobby_info.name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
        entry.player_count = lobby_info.player_count;
        entry.connection_info.lan.port = lobby_info.port;
        enet_address_get_host_ip(&receive_address, entry.connection_info.lan.ip, NETWORK_IP_BUFFER_SIZE);

        if (strlen(lobby_name_query) == 0 || strstr(lobby_name_query, entry.name) != NULL) {
            matchlist.push_back(entry);
        }
    }
}

// STEAM

bool NetworkSteamLobbyScanner::was_create_successful() const {
    return true;
}

void NetworkSteamLobbyScanner::search(const char* query) {
    strncpy(lobby_name_query, query, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    matchlist.clear();

    SteamAPICall_t api_call = SteamMatchmaking()->RequestLobbyList();
    call_result_lobby_matchlist.Set(api_call, this, &NetworkSteamLobbyScanner::on_lobby_matchlist);
}

void NetworkSteamLobbyScanner::service() {

}

void NetworkSteamLobbyScanner::on_lobby_matchlist(LobbyMatchList_t* lobby_matchlist, bool io_failure) {
    if (io_failure) {
        log_error("IO failure on NetworkSteamLobbyScanner::on_lobby_matchlist");
        return;
    }

    for (uint32_t lobby_index = 0; lobby_index < lobby_matchlist->m_nLobbiesMatching; lobby_index++) {
        CSteamID lobby_id = SteamMatchmaking()->GetLobbyByIndex(lobby_index);
        NetworkMatchlistEntry entry;
        strncpy(entry.name, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME), NETWORK_LOBBY_NAME_BUFFER_SIZE);
        memcpy(&entry.player_count, SteamMatchmaking()->GetLobbyData(lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT), sizeof(uint8_t));
        entry.connection_info.steam.id = SteamMatchmaking()->GetLobbyOwner(lobby_id).ConvertToUint64();
        matchlist.push_back(entry);

        if (strlen(lobby_name_query) == 0 || strstr(lobby_name_query, entry.name) != NULL) {
            matchlist.push_back(entry);
        }
    }
}