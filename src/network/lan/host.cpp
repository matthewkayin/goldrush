#include "host.h"

#include "core/logger.h"
#include "network/network.h"

NetworkHostLan::NetworkHostLan() {
    memset(host_lobby_name, 0, sizeof(host_lobby_name));

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = NETWORK_BASE_PORT;

    host = NULL;
    while (host == NULL && address.port < NETWORK_BASE_PORT + MAX_PLAYERS) {
        host = enet_host_create(&address, MAX_PLAYERS - 0, 1, 0, 0);
        log_info("Created host with port %u", address.port);
        if (host == NULL) {
            address.port++;
        }
    }

    if (host == NULL) {
        log_error("Unable to initialize LAN host.");
        return;
    }

    host_listener_socket = ENET_SOCKET_NULL;

    log_info("Created LAN host.");
}

NetworkHostLan::~NetworkHostLan() {
    if (host != NULL) {
        enet_host_destroy(host);
    }
    if (host_listener_socket != ENET_SOCKET_NULL) {
        enet_socket_shutdown(host_listener_socket, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(host_listener_socket);
    }
    
    log_info("Destroyed LAN host.");
}

bool NetworkHostLan::is_initialized_successfully() const {
    return host != NULL;
}

NetworkBackend NetworkHostLan::get_backend() const {
    return NETWORK_BACKEND_LAN;
}

void NetworkHostLan::open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy) {
    strncpy(host_lobby_name, lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);

    if (privacy == NETWORK_LOBBY_PRIVACY_SINGLEPLAYER) {
        host_events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_LOBBY_CREATE_SUCCESS
        });
        log_info("LAN host opened singleplayer lobby.");
        return;
    }

    host_listener_socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (host_listener_socket == ENET_SOCKET_NULL) {
        log_error("LAN host failed to create listener socket.");
        host_events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_LOBBY_CREATE_FAILED
        });
        return;
    }

    enet_socket_set_option(host_listener_socket, ENET_SOCKOPT_REUSEADDR, 1);

    ENetAddress listener_address;
    listener_address.host = ENET_HOST_ANY;
    listener_address.port = NETWORK_SCANNER_PORT;
    if (enet_socket_bind(host_listener_socket, &listener_address) != 0) {
        log_error("LAN host failed to bind listener socket.");
        host_events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_LOBBY_CREATE_FAILED
        });
        return;
    }

    host_events.push((NetworkHostEvent) {
        .type = NETWORK_HOST_EVENT_LOBBY_CREATE_SUCCESS
    });

    log_info("LAN host opened lobby.");
}

void NetworkHostLan::close_lobby() {
    if (host_listener_socket != ENET_SOCKET_NULL) {
        enet_socket_shutdown(host_listener_socket, ENET_SOCKET_SHUTDOWN_READ_WRITE);
        enet_socket_destroy(host_listener_socket);
        host_listener_socket = ENET_SOCKET_NULL;
    }
}

bool NetworkHostLan::connect(const NetworkConnectionInfo& p_connection_info) {
    NetworkConnectionInfoLan* connection_info = (NetworkConnectionInfoLan*)p_connection_info.data;
    log_info("Connecting to host %s:%u", connection_info->ip, connection_info->port);

    ENetAddress host_address;
    host_address.port = connection_info->port;
    enet_address_set_host_ip(&host_address, connection_info->ip);

    ENetPeer* host_peer = enet_host_connect(host, &host_address, 1, 0);
    if (host_peer == NULL) {
        log_error("Failed to connect to LAN host.");
        return false;
    }

    return true;
}

uint16_t NetworkHostLan::get_peer_count() const {
    return host->peerCount;
}

uint8_t NetworkHostLan::get_peer_player_id(uint16_t peer_id) const {
    uint8_t* player_id_ptr = (uint8_t*)host->peers[peer_id].data;
    if (player_id_ptr == NULL) {
        return PLAYER_NONE;
    }
    return *player_id_ptr;
}

void NetworkHostLan::set_peer_player_id(uint16_t peer_id, uint8_t player_id) {
    uint8_t* player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
    *player_id_ptr = player_id;
    host->peers[peer_id].data = player_id_ptr;
}

NetworkConnectionInfo NetworkHostLan::get_peer_connection_info(uint16_t peer_id) const {
    NetworkConnectionInfo result;
    memset(result.data, 0, sizeof(result.data));

    NetworkConnectionInfoLan* connection_info = (NetworkConnectionInfoLan*)result.data;
    enet_address_get_host_ip(&host->peers[peer_id].address, connection_info->ip, NETWORK_IP_BUFFER_SIZE);
    connection_info->port = host->peers[peer_id].address.port;

    return result;
}

void NetworkHostLan::disconnect_peers() {
    for (uint16_t peer_id = 0; peer_id < host->peerCount; peer_id++) {
        if (host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED) {
            enet_peer_disconnect(&host->peers[peer_id], 0);
        }
    }
    enet_host_flush(host);

    for (uint16_t peer_id = 0; peer_id < host->peerCount; peer_id++) {
        enet_peer_reset(&host->peers[peer_id]);
    }
}

void NetworkHostLan::send(uint16_t peer_id, void* data, size_t length) {
    ENetPacket* packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(&host->peers[peer_id], 0, packet);
}

void NetworkHostLan::broadcast(void* data, size_t length) {
    ENetPacket* packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(host, 0, packet);
}

void NetworkHostLan::flush() {
    enet_host_flush(host);
}

void NetworkHostLan::service() {
    // Listen on listener socket
    if (host_listener_socket != ENET_SOCKET_NULL) {
        ENetSocketSet set;
        ENET_SOCKETSET_EMPTY(set);
        ENET_SOCKETSET_ADD(set, host_listener_socket);
        while (enet_socketset_select(host_listener_socket, &set, NULL, 0) > 0) {
            ENetAddress receive_address;
            ENetBuffer receive_buffer;
            char buffer[128];
            receive_buffer.data = &buffer;
            receive_buffer.dataLength = sizeof(buffer);
            if (enet_socket_receive(host_listener_socket, &receive_address, &receive_buffer, 1) <= 0) {
                continue;
            }

            // Tell the client about this lobby
            NetworkLanLobbyInfo lobby_info;
            strncpy(lobby_info.name, host_lobby_name, sizeof(lobby_info.name));
            lobby_info.port = host->address.port;
            lobby_info.player_count = network_get_player_count();

            ENetBuffer response_buffer;
            response_buffer.data = &lobby_info;
            response_buffer.dataLength = sizeof(lobby_info);
            enet_socket_send(host_listener_socket, &receive_address, &response_buffer, 1);
            log_debug("Network LAN host sent lobby info. Lobby name %s player count %u port %u", lobby_info.name, lobby_info.player_count, lobby_info.port);
        }
    }

    // Service host
    ENetEvent enet_event;
    if (enet_host_service(host, &enet_event, 0) > 0) {
        switch (enet_event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                host_events.push((NetworkHostEvent) {
                    .type = NETWORK_HOST_EVENT_CONNECTED,
                    .connected = (NetworkHostEventConnected) {
                        .peer_id = enet_event.peer->incomingPeerID
                    }
                });
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                host_events.push((NetworkHostEvent) {
                    .type = NETWORK_HOST_EVENT_DISCONNECTED,
                    .disconnected = (NetworkHostEventDisconnected) {
                        .player_id = get_peer_player_id(enet_event.peer->incomingPeerID)
                    }
                });
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                host_events.push((NetworkHostEvent) {
                    .type = NETWORK_HOST_EVENT_RECEIVED,
                    .received = (NetworkHostEventReceived) {
                        .peer_id = enet_event.peer->incomingPeerID,
                        .packet = (NetworkHostPacket) {
                            .data = enet_event.packet->data,
                            .length = enet_event.packet->dataLength,
                            ._impl = enet_event.packet
                        }
                    }
                });
                break;
            }
            default:
                break;
        }
    }
}

void NetworkHostLan::destroy_packet(NetworkHostPacket* packet) {
    enet_packet_destroy((ENetPacket*)packet->_impl);
}