#include "host_lan.h"

#include "core/logger.h"

NetworkHostLan::NetworkHostLan() {
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
    set_lobby_name(lobby_name);

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

bool NetworkHostLan::connect(void* p_connection_info) {
    NetworkConnectionInfoLan* connection_info = (NetworkConnectionInfoLan*)p_connection_info;
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

void NetworkHostLan::buffer_connection_info(void* buffer) const {
    NetworkConnectionInfoLan connection_info;
    enet_address_set_host_ip(&host->address, connection_info.ip);
    connection_info.port = host->address.port;

    memcpy(buffer, &connection_info, sizeof(connection_info));
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

bool NetworkHostLan::is_peer_connected(uint16_t peer_id) const {
    return host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED;
}

void NetworkHostLan::disconnect_peer(uint16_t peer_id, bool gently) {
    if (gently) {
        enet_peer_disconnect(&host->peers[peer_id], 0);
    } else {
        enet_peer_reset(&host->peers[peer_id]);
    }
}

size_t NetworkHostLan::buffer_peer_connection_info(uint16_t peer_id, void* buffer) const {
    NetworkConnectionInfoLan connection_info;
    memset(&connection_info, 0, sizeof(connection_info));
    enet_address_get_host_ip(&host->peers[peer_id].address, (char*)connection_info.ip, NETWORK_IP_BUFFER_SIZE);
    connection_info.port = host->peers[peer_id].address.port;
    memcpy(buffer, &connection_info, sizeof(connection_info));
    return sizeof(connection_info);
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