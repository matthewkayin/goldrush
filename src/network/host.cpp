#include "host.h"

#include "core/logger.h"

NetworkHost* network_host_create(NetworkBackend backend) {
    NetworkHost* host = (NetworkHost*)malloc(sizeof(NetworkHost));
    host->backend = backend;
    host->events = new std::queue<NetworkHostEvent>();

    if (host->backend == NETWORK_BACKEND_LAN) {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = NETWORK_BASE_PORT;

        host->lan.host = NULL;
        while (host->lan.host == NULL && address.port < NETWORK_BASE_PORT + MAX_PLAYERS) {
            host->lan.host = enet_host_create(&address, MAX_PLAYERS - 0, 1, 0, 0);
            log_trace("created host with port %u", address.port);
            if (host->lan.host == NULL) {
                address.port++;
            }
        }

        if (host->lan.host == NULL) {
            free(host);
            return NULL;
        }
    } else {
        host->steam.listen_socket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, NULL);
        host->steam.poll_group = SteamNetworkingSockets()->CreatePollGroup();
        host->steam.peer_count = 0;
    }
    log_trace("Created host with backend %u", backend);

    return host;
}

void network_host_destroy(NetworkHost* host) {
    log_trace("Destroying host with backend %u", host->backend);
    delete host->events;
    if (host->backend == NETWORK_BACKEND_LAN) {
        enet_host_destroy(host->lan.host);
    } else {
        SteamNetworkingSockets()->DestroyPollGroup(host->steam.poll_group);
        SteamNetworkingSockets()->CloseListenSocket(host->steam.listen_socket);
    }
    free(host);
}

bool network_host_connect(NetworkHost* host, NetworkConnectionInfo connection_info) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        log_trace("connecting to host %s:%u", connection_info.lan.ip, connection_info.lan.port);

        ENetAddress host_address;
        host_address.port = connection_info.lan.port;
        enet_address_set_host_ip(&host_address, connection_info.lan.ip);

        ENetPeer* host_peer = enet_host_connect(host->lan.host, &host_address, 1, 0);
        if (host_peer == NULL) {
            log_error("Failed to connect to LAN host.");
            return false;
        }
    } else {
        log_trace("Connecting to host with steam identity %s", connection_info.steam.identity_str);
        SteamNetworkingIdentity identity;
        identity.ParseString(connection_info.steam.identity_str);
        HSteamNetConnection connection = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, NULL);
        SteamNetworkingSockets()->SetConnectionPollGroup(connection, host->steam.poll_group);
        host->steam.peers[host->steam.peer_count] = connection;
        host->steam.peer_count++;
        log_trace("Host has new peer with connection %u. Peer count %u", host->steam.peers[host->steam.peer_count], host->steam.peer_count);
    }

    return true;
}

NetworkConnectionInfo network_host_get_connection_info(NetworkHost* host) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        NetworkConnectionInfo connection_info;
        enet_address_set_host_ip(&host->lan.host->address, connection_info.lan.ip);
        connection_info.lan.port = host->lan.host->address.port;
        
        return connection_info;
    } else {
        NetworkConnectionInfo connection_info;
        SteamNetworkingIdentity identity;
        SteamNetworkingSockets()->GetIdentity(&identity);
        identity.ToString(connection_info.steam.identity_str, sizeof(connection_info.steam.identity_str));
        return connection_info;
    }
}

uint16_t network_host_get_peer_count(NetworkHost* host) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        return host->lan.host->peerCount;
    } else {
        return host->steam.peer_count;
    }
}

uint8_t network_host_get_peer_player_id(NetworkHost* host, uint16_t peer_id) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        uint8_t* player_id_ptr = (uint8_t*)host->lan.host->peers[peer_id].data;
        if (player_id_ptr == NULL) {
            return PLAYER_NONE;
        }
        return *player_id_ptr;
    } else {
        int64_t player_id = SteamNetworkingSockets()->GetConnectionUserData(host->steam.peers[peer_id]);
        if (player_id == -1) {
            return PLAYER_NONE;
        }
        return (uint8_t)player_id;
    }
}

void network_host_set_peer_player_id(NetworkHost* host, uint16_t peer_id, uint8_t player_id) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        uint8_t* player_id_ptr = (uint8_t*)malloc(sizeof(uint8_t));
        *player_id_ptr = player_id;
        host->lan.host->peers[peer_id].data = player_id_ptr;
    } else {
        SteamNetworkingSockets()->SetConnectionUserData(host->steam.peers[peer_id], player_id);
    }
}

bool network_host_is_peer_connected(NetworkHost* host, uint16_t peer_id) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        return host->lan.host->peers[peer_id].state == ENET_PEER_STATE_CONNECTED;
    } else {
        SteamNetConnectionInfo_t connection_info;
        if (!SteamNetworkingSockets()->GetConnectionInfo(host->steam.peers[peer_id], &connection_info)) {
            return false;
        }
        return connection_info.m_eState == k_ESteamNetworkingConnectionState_Connected;
    }
}

void network_host_disconnect_peer(NetworkHost* host, uint16_t peer_id, bool gently) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        if (gently) {
            enet_peer_disconnect(&host->lan.host->peers[peer_id], 0);
        } else {
            enet_peer_reset(&host->lan.host->peers[peer_id]);
        }
    } else {
        SteamNetworkingSockets()->CloseConnection(host->steam.peers[peer_id], 0, "", false);
        host->steam.peers[peer_id] = host->steam.peers[host->steam.peer_count - 1];
        host->steam.peer_count--;
        log_trace("Disconnected peer. peer count %u", host->steam.peer_count);
    }
}

size_t network_host_buffer_peer_connection_info(NetworkHost* host, uint16_t peer_id, void* buffer) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        NetworkConnectionInfoLan connection_info;
        memset(&connection_info, 0, sizeof(connection_info));
        enet_address_get_host_ip(&host->lan.host->peers[peer_id].address, (char*)connection_info.ip, NETWORK_IP_BUFFER_SIZE);
        connection_info.port = host->lan.host->peers[peer_id].address.port;
        memcpy(buffer, &connection_info, sizeof(connection_info));
        return sizeof(connection_info);
    } else {
        SteamNetConnectionInfo_t connection_info;
        if (!SteamNetworkingSockets()->GetConnectionInfo(host->steam.peers[peer_id], &connection_info)) {
            return 0;
        }
        uint64_t steam_id = connection_info.m_identityRemote.GetSteamID64();
        memcpy(buffer, &steam_id, sizeof(steam_id));
        return sizeof(steam_id);
    }
}

NetworkConnectionInfo network_parse_connection_info(NetworkBackend backend, void* buffer) {
    if (backend == NETWORK_BACKEND_LAN) {
        NetworkConnectionInfo connection_info;
        memcpy(&connection_info.lan, buffer, sizeof(connection_info));
        return connection_info;
    } else {
        NetworkConnectionInfo connection_info;
        strncpy(connection_info.steam.identity_str, (char*)buffer, sizeof(connection_info.steam.identity_str));
        return connection_info;
    }
}

void network_host_send(NetworkHost* host, uint16_t peer_id, void* data, size_t length) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        ENetPacket* packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
        enet_peer_send(&host->lan.host->peers[peer_id], 0, packet);
    } else {
        SteamNetworkingSockets()->SendMessageToConnection(host->steam.peers[peer_id], data, length, k_nSteamNetworkingSend_Reliable, NULL);
    }
}

void network_host_broadcast(NetworkHost* host, void* data, size_t length) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        ENetPacket* packet = enet_packet_create(data, length, ENET_PACKET_FLAG_RELIABLE);
        enet_host_broadcast(host->lan.host, 0, packet);
    } else {
        for (uint16_t peer_id = 0; peer_id < host->steam.peer_count; peer_id++) {
            SteamNetworkingSockets()->SendMessageToConnection(host->steam.peers[peer_id], data, length, k_nSteamNetworkingSend_Reliable, NULL);
        }
    }
}

void network_host_flush(NetworkHost* host) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        enet_host_flush(host->lan.host);
    } else {
        for (uint16_t peer_id = 0; peer_id < host->steam.peer_count; peer_id++) {
            SteamNetworkingSockets()->FlushMessagesOnConnection(host->steam.peers[peer_id]);
        }
    }
}

void network_host_service(NetworkHost* host) {
    if (host->backend == NETWORK_BACKEND_LAN) {
        ENetEvent enet_event;
        if (enet_host_service(host->lan.host, &enet_event, 0) > 0) {
            switch (enet_event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    log_trace("LAN host connect event.");
                    host->events->push((NetworkHostEvent) {
                        .type = NETWORK_HOST_EVENT_CONNECTED,
                        .connected = (NetworkHostEventConnected) {
                            .peer_id = enet_event.peer->incomingPeerID
                        }
                    });
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    host->events->push((NetworkHostEvent) {
                        .type = NETWORK_HOST_EVENT_DISCONNECTED,
                        .disconnected = (NetworkHostEventDisconnected) {
                            .player_id = network_host_get_peer_player_id(host, enet_event.peer->incomingPeerID)
                        }
                    });
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    host->events->push((NetworkHostEvent) {
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
    } else {
        SteamNetworkingMessage_t* messages[32];
        int message_count = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(host->steam.poll_group, messages, 32);
        for (int message_index = 0; message_index < message_count; message_index++) {
            SteamNetworkingMessage_t* message = messages[message_index];
            uint16_t peer_id;
            for (peer_id = 0; peer_id < host->steam.peer_count; peer_id++) {
                if (message->m_conn == host->steam.peers[peer_id]) {
                    break;
                }
            }
            if (peer_id == host->steam.peer_count) {
                log_warn("Received message from peer who is not in the peer list.");
                message->Release();
                continue;
            }

            host->events->push((NetworkHostEvent) {
                .type = NETWORK_HOST_EVENT_RECEIVED,
                .received = (NetworkHostEventReceived) {
                    .peer_id = peer_id,
                    .packet = (NetworkHostPacket) {
                        .backend = NETWORK_BACKEND_STEAM,
                        .data = (uint8_t*)message->m_pData,
                        .length = (size_t)message->m_cbSize,
                        .steam_message = message
                    }
                }
            });
        }
    }
}

void network_host_steam_on_connection_status_changed(NetworkHost* host, SteamNetConnectionStatusChangedCallback_t* callback) {
    log_trace("Steam on_connection_status_changed connection %u status %u -> %u", callback->m_hConn, callback->m_eOldState, callback->m_info.m_eState);

    // On host connecting
    if (callback->m_eOldState == k_ESteamNetworkingConnectionState_None && 
            callback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting) {
        // If we already have this connection in our peers list, it means we reached out to them
        // So there is nothing for us to do here
        for (uint16_t peer_id = 0; peer_id < host->steam.peer_count; peer_id++) {
            if (host->steam.peers[peer_id] == callback->m_hConn) {
                log_trace("Peer is already recognized. Doing nothing.");
                return;
            }
        }

        // Otherwise, they reached out to us. 
        // Do we have enough space in the peer list to accept them?
        // If we don't, it should mean that we're the host and the lobby is full.
        // TODO: replace this logic with a check on network_get_player_count() so that we can account for bots taking up lobby space
        if (host->steam.peer_count == MAX_PLAYERS - 1) {
            log_trace("Peer count is full. Rejecting.");
            SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
            return;
        }

        // If we do, accept the connection
        log_trace("Accepted connection.");
        host->steam.peers[host->steam.peer_count] = callback->m_hConn;
        SteamNetworkingSockets()->AcceptConnection(host->steam.peers[host->steam.peer_count]);
        SteamNetworkingSockets()->SetConnectionPollGroup(host->steam.peers[host->steam.peer_count], host->steam.poll_group);
        host->steam.peer_count++;
        return;
    }

    // Connection
    if (callback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
        // Determine incoming peer id
        uint16_t peer_id;
        for (peer_id = 0; peer_id < host->steam.peer_count; peer_id++) {
            if (host->steam.peers[peer_id] == callback->m_hConn) {
                break;
            }
        }

        if (peer_id == host->steam.peer_count) {
            log_warn("Received connection event from a host that is not in the peer list.");
            return;
        }

        host->events->push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_CONNECTED,
            .connected = (NetworkHostEventConnected) {
                .peer_id = peer_id
            }
        });
        return;
    }

    // Connection has been rejected or closed by the remote host
    if (callback->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || 
            callback->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally) {
        uint16_t peer_id;
        for (peer_id = 0; peer_id < host->steam.peer_count; peer_id++) {
            if (callback->m_hConn == host->steam.peers[peer_id]) {
                break;
            }
        }

        // If we know who this is, so post an event. 
        if (peer_id != host->steam.peer_count) {
            host->events->push((NetworkHostEvent) {
                .type = NETWORK_HOST_EVENT_DISCONNECTED,
                .disconnected = (NetworkHostEventDisconnected) {
                    .player_id = network_host_get_peer_player_id(host, peer_id)
                }
            });
            network_host_disconnect_peer(host, peer_id, false);
        } else {
            SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
        }
    }
}

bool network_host_poll_events(NetworkHost* host, NetworkHostEvent* event) {
    if (host->events->empty()) {
        return false;
    }

    *event = host->events->front();
    host->events->pop();

    return true;
}

// PACKET

void network_host_packet_destroy(NetworkHostPacket packet) {
    if (packet.backend == NETWORK_BACKEND_LAN) {
        enet_packet_destroy(packet.enet_packet);
    } else if (packet.backend == NETWORK_BACKEND_STEAM) {
        packet.steam_message->Release();
    }
}