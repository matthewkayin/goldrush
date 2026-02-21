#include "scanner.h"

#include "network/types.h"
#include "core/logger.h"

NetworkScannerLan::NetworkScannerLan() {
    scanner_socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (scanner_socket == ENET_SOCKET_NULL) {
        log_error("Failed to create scanner socket.");
        return;
    }
    enet_socket_set_option(scanner_socket, ENET_SOCKOPT_BROADCAST, 1);
}

NetworkScannerLan::~NetworkScannerLan() {
    if (scanner_socket != ENET_SOCKET_NULL) {
        enet_socket_shutdown(scanner_socket, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(scanner_socket);
    }
}

void NetworkScannerLan::search_lobbies(const char* query) {
    strncpy(scanner_lobby_name_query, query, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    scanner_lobbies.clear();

    ENetAddress scan_address;
    scan_address.host = ENET_HOST_BROADCAST;
    scan_address.port = NETWORK_SCANNER_PORT;

    char buffer = 3;
    ENetBuffer send_buffer;
    send_buffer.data = &buffer;
    send_buffer.dataLength = 1;
    if (enet_socket_send(scanner_socket, &scan_address, &send_buffer, 1) != 1) {
        log_error("Failed to scan for LAN servers.");
    }
}

bool NetworkScannerLan::is_initialized_successfully() const {
    return scanner_socket != ENET_SOCKET_NULL;
}

void NetworkScannerLan::service() {
    ENetSocketSet set;
    ENET_SOCKETSET_EMPTY(set);
    ENET_SOCKETSET_ADD(set, scanner_socket);
    while (enet_socketset_select(scanner_socket, &set, NULL, 0) > 0) {
        ENetAddress receive_address;
        ENetBuffer receive_buffer;
        NetworkLanLobbyInfo lobby_info;

        receive_buffer.data = &lobby_info;
        receive_buffer.dataLength = sizeof(lobby_info);

        if (enet_socket_receive(scanner_socket, &receive_address, &receive_buffer, 1) <= 0) {
            continue;
        }

        // A server told us about a lobby

        // Create a new lobby entry to remember
        NetworkLobby lobby;
        strncpy(lobby.name, lobby_info.name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
        lobby.player_count = lobby_info.player_count;

        // Set connnection info
        enet_address_get_host_ip(&receive_address, lobby.connection_info.lan.ip, NETWORK_IP_BUFFER_SIZE);
        lobby.connection_info.lan.port = lobby_info.port;

        // Filter by search query and add to lobby list
        if (strlen(scanner_lobby_name_query) == 0 || strstr(lobby.name, scanner_lobby_name_query) != NULL) {
            scanner_lobbies.push_back(lobby);
        }
    }
}