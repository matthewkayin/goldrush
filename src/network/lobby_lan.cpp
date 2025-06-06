#include "lobby.h"

#include "core/logger.h"
#include "core/asserts.h"

void NetworkLanLobby::open(const char* name, NetworkConnectionInfo connection_info) {
    scanner = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (scanner == ENET_SOCKET_NULL) {
        log_error("Failed to create lobby scanner");
        status = NETWORK_LOBBY_ERROR;
        return;
    }
    enet_socket_set_option(scanner, ENET_SOCKOPT_REUSEADDR, 1);
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = NETWORK_SCANNER_PORT;
    if (enet_socket_bind(scanner, &address) != 0) {
        log_error("Failed to bind to lobby listener socket.");
        enet_socket_destroy(scanner);
        scanner = ENET_SOCKET_NULL;
        status = NETWORK_LOBBY_ERROR;
        return;
    }

    strncpy(lobby_info.name, name, NETWORK_LOBBY_NAME_BUFFER_SIZE);
    lobby_info.port = connection_info.lan.port;
    lobby_info.player_count = 1;
    status = NETWORK_LOBBY_OPEN;
}

void NetworkLanLobby::close() {
    if (scanner != ENET_SOCKET_NULL) {
        enet_socket_shutdown(scanner, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(scanner);
        status = NETWORK_LOBBY_CLOSED;
    }
}

void NetworkLanLobby::service() {
    GOLD_ASSERT(status == NETWORK_LOBBY_OPEN);

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

        ENetBuffer response_buffer;
        response_buffer.data = &lobby_info;
        response_buffer.dataLength = sizeof(NetworkLanLobbyInfo);
        enet_socket_send(scanner, &receive_address, &response_buffer ,1);
    }
}

void NetworkLanLobby::set_player_count(uint8_t player_count) {
    lobby_info.player_count = player_count;
}
