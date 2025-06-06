#include "host.h"

#include "core/logger.h"

NetworkLanHost::NetworkLanHost() {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = NETWORK_BASE_PORT;

    host = NULL;
    while (host == NULL && address.port < NETWORK_BASE_PORT + MAX_PLAYERS) {
        host = enet_host_create(&address, MAX_PLAYERS - 0, 1, 0, 0);
        log_trace("created host with port %u", address.port);
        if (host == NULL) {
            address.port++;
        }
    }

    if (host == NULL) {
        return;
    }

    memset(peers, -1, sizeof(host->peers));
}

NetworkLanHost::~NetworkLanHost() {
    if (host != NULL) {
        enet_host_destroy(host);
    }
}

bool NetworkLanHost::was_create_successful() const {
    return host != NULL;
}

bool NetworkLanHost::connect(NetworkConnectionInfo connection_info) {
    log_trace("connecting to host %s:%u", connection_info.lan.ip, connection_info.lan.port);

    ENetAddress host_address;
    host_address.port = connection_info.lan.port;
    enet_address_set_host_ip(&host_address, connection_info.lan.ip);

    ENetPeer* host_peer = enet_host_connect(host, &host_address, 1, 0);
    if (host_peer == NULL) {
        log_error("Failed to connect to LAN host.");
        return false;
    }

    return true;
}

NetworkConnectionInfo NetworkLanHost::get_connection_info() const {
    NetworkConnectionInfo connection_info;
    enet_address_set_host_ip(&host->address, connection_info.lan.ip);
    connection_info.lan.port = host->address.port;
    
    return connection_info;
}

uint16_t NetworkLanHost::get_peer_count() const {
    return host->peerCount;
}

uint8_t NetworkLanHost::get_peer_player_id(uint16_t peer_id) const {
    uint8_t* player_id_ptr = (uint8_t*)host->peers[peer_id].data;
    if (player_id_ptr == NULL) {
        return PLAYER_NONE;
    }
    return *player_id_ptr;
}

void NetworkLanHost::set_peer_player_id(uint16_t peer_id, uint8_t player_id) {
    uint8_t* player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
    *player_id_ptr = player_id;
    host->peers[peer_id].data = player_id_ptr;
}

bool NetworkLanHost::is_peer_connected(uint16_t peer_id) const {
    return host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED;
}

void NetworkLanHost::disconnect_peer(uint16_t peer_id, bool gently) {
    if (gently) {
        enet_peer_disconnect(&host->peers[peer_id], 0);
    } else {
        enet_peer_reset(&host->peers[peer_id]);
    }
}

size_t NetworkLanHost::buffer_peer_connection_info(uint16_t peer_id, void* buffer) {
    NetworkConnectionInfoLan connection_info;
    memset(&connection_info, 0, sizeof(connection_info));
    enet_address_get_host_ip(&host->peers[peer_id].address, (char*)connection_info.ip, NETWORK_IP_BUFFER_SIZE);
    connection_info.port = host->peers[peer_id].address.port;
    memcpy(buffer, &connection_info, sizeof(connection_info));
    return sizeof(connection_info);
}

NetworkConnectionInfo NetworkLanHost::parse_connection_info(void* buffer) {
    NetworkConnectionInfo connection_info;
    memcpy(&connection_info.lan, buffer, sizeof(connection_info));
    return connection_info;
}

void NetworkLanHost::send(uint16_t peer_id, void* data, size_t length) {
    ENetPacket* packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(&host->peers[peer_id], 0, packet);
}

void NetworkLanHost::broadcast(void* data, size_t length) {
    ENetPacket* packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(host, 0, packet);
}

void NetworkLanHost::flush() {
    enet_host_flush(host);
}

void NetworkLanHost::service() {
    ENetEvent enet_event;
    if (enet_host_service(host, &enet_event, 0) > 0) {
        switch (enet_event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                log_trace("LAN host connect event.");
                events.push((NetworkHostEvent) {
                    .type = NETWORK_HOST_EVENT_CONNECTED,
                    .connected = (NetworkHostEventConnected) {
                        .peer_id = enet_event.peer->incomingPeerID
                    }
                });
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                events.push((NetworkHostEvent) {
                    .type = NETWORK_HOST_EVENT_DISCONNECTED,
                    .disconnected = (NetworkHostEventDisconnected) {
                        .player_id = get_peer_player_id(enet_event.peer->incomingPeerID)
                    }
                });
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                events.push((NetworkHostEvent) {
                    .type = NETWORK_HOST_EVENT_RECEIVED,
                    .received = (NetworkHostEventReceived) {
                        .peer_id = enet_event.peer->incomingPeerID,
                        .packet = (NetworkHostPacket) {
                            .backend = NETWORK_BACKEND_LAN,
                            .data = enet_event.packet->data,
                            .length = enet_event.packet->dataLength,
                            .enet_packet = enet_event.packet
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
