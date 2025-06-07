#include "host.h"

#include "core/logger.h"

NetworkSteamHost::NetworkSteamHost() {
    listen_socket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, NULL);
    poll_group = SteamNetworkingSockets()->CreatePollGroup();
    peer_count = 0;
}

NetworkSteamHost::~NetworkSteamHost() {
    SteamNetworkingSockets()->DestroyPollGroup(poll_group);
    SteamNetworkingSockets()->CloseListenSocket(listen_socket);
}

bool NetworkSteamHost::was_create_successful() const {
    return true;
}

bool NetworkSteamHost::connect(NetworkConnectionInfo connection_info) {
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(connection_info.steam.id);
    HSteamNetConnection connection = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, NULL);
    SteamNetworkingSockets()->SetConnectionPollGroup(connection, poll_group);
    log_trace("Connecting to host with steam id %u", connection_info.steam.id);

    return true;
}

NetworkConnectionInfo NetworkSteamHost::get_connection_info() const {
    NetworkConnectionInfo connection_info;
    connection_info.steam.id = SteamUser()->GetSteamID().ConvertToUint64();
    return connection_info;
}

uint16_t NetworkSteamHost::get_peer_count() const {
    return peer_count;
}

uint8_t NetworkSteamHost::get_peer_player_id(uint16_t peer_id) const {
    int64_t player_id = SteamNetworkingSockets()->GetConnectionUserData(peers[peer_id]);
    if (player_id == -1) {
        return PLAYER_NONE;
    }
    return (uint8_t)player_id;
}

void NetworkSteamHost::set_peer_player_id(uint16_t peer_id, uint8_t player_id) {
    SteamNetworkingSockets()->SetConnectionUserData(peers[peer_id], player_id);
}

bool NetworkSteamHost::is_peer_connected(uint16_t peer_id) const {
    SteamNetConnectionInfo_t connection_info;
    if (!SteamNetworkingSockets()->GetConnectionInfo(peers[peer_id], &connection_info)) {
        return false;
    }
    return connection_info.m_eState == k_ESteamNetworkingConnectionState_Connected;
}

void NetworkSteamHost::disconnect_peer(uint16_t peer_id, bool gently) {
    SteamNetworkingSockets()->CloseConnection(peers[peer_id], 0, "", false);
    peers[peer_id] = peers[peer_count - 1];
    peer_count--;
}

size_t NetworkSteamHost::buffer_peer_connection_info(uint16_t peer_id, void* buffer) {
    SteamNetConnectionInfo_t connection_info;
    if (!SteamNetworkingSockets()->GetConnectionInfo(peers[peer_id], &connection_info)) {
        return 0;
    }
    uint64_t steam_id = connection_info.m_identityRemote.GetSteamID64();
    memcpy(buffer, &steam_id, sizeof(steam_id));
    return sizeof(steam_id);
}

NetworkConnectionInfo NetworkSteamHost::parse_connection_info(void* buffer) {
    NetworkConnectionInfo connection_info;
    memcpy(&connection_info.steam.id, buffer, sizeof(uint64_t));
    return connection_info;
}

void NetworkSteamHost::send(uint16_t peer_id, void* data, size_t length) {
    SteamNetworkingSockets()->SendMessageToConnection(peers[peer_id], data, length, k_nSteamNetworkingSend_Reliable, NULL);
}

void NetworkSteamHost::broadcast(void* data, size_t length) {
    for (uint16_t peer_id = 0; peer_id < get_peer_count(); peer_id++) {
        send(peer_id, data, length);
    }
}

void NetworkSteamHost::flush() {
    for (uint16_t peer_id = 0; peer_id < peer_count; peer_id++) {
        SteamNetworkingSockets()->FlushMessagesOnConnection(peers[peer_id]);
    }
}

void NetworkSteamHost::service() {
    SteamNetworkingMessage_t* messages[32];
    int message_count = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(poll_group, messages, 32);
    for (int message_index = 0; message_index < message_count; message_index++) {
        SteamNetworkingMessage_t* message = messages[message_index];
        uint16_t peer_id;
        for (peer_id = 0; peer_id < peer_count; peer_id++) {
            if (message->m_conn == peers[peer_id]) {
                break;
            }
        }
        if (peer_id == peer_count) {
            log_warn("Received message from peer who is not in the peer list.");
            message->Release();
            continue;
        }

        events.push((NetworkHostEvent) {
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

void NetworkSteamHost::on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* callback) {
    log_trace("Steam on_connection_status_changed %u -> %u", callback->m_eOldState, callback->m_info.m_eState);
    // Connection
    if (callback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
        log_trace("Connected");
        // New connection has reached out to this host
        if (callback->m_eOldState == k_ESteamNetworkingConnectionState_None) {
            if (peer_count == MAX_PLAYERS - 1) {
                SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
                return;
            }

            SteamNetworkingSockets()->AcceptConnection(callback->m_hConn);
        }

        // Otherwise it is a connection we initiated which has been accepted by the remote host
        // Either way, add the peer to the peer list and send a connected event
        peers[peer_count] = callback->m_hConn;
        SteamNetworkingSockets()->SetConnectionPollGroup(peers[peer_count], poll_group);
        events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_CONNECTED,
            .connected = (NetworkHostEventConnected) {
                .peer_id = peer_count
            }
        });

        peer_count++;
    }

    // Connection has been rejected or closed by the remote host
    if ((callback->m_eOldState == k_ESteamNetworkingConnectionState_Connecting ||
            callback->m_eOldState == k_ESteamNetworkingConnectionState_Connecting) &&
            (callback->m_info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || 
             callback->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)) {
        log_trace("Disconnected");
        uint16_t peer_id;
        for (peer_id = 0; peer_id < peer_count; peer_id++) {
            if (callback->m_hConn == peers[peer_id]) {
                break;
            }
        }

        // If we know who this is, so post an event. 
        if (peer_id != peer_count) {
            events.push((NetworkHostEvent) {
                .type = NETWORK_HOST_EVENT_DISCONNECTED,
                .disconnected = (NetworkHostEventDisconnected) {
                    .player_id = get_peer_player_id(peer_id)
                }
            });
            disconnect_peer(peer_id, false);
        } else {
            SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
        }
    }
}
