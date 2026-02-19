#include "host_steam.h"

#ifdef GOLD_STEAM

#include "core/logger.h"

NetworkHostSteam::NetworkHostSteam() {
    host_listen_socket = SteamNetworkingSockets()->CreateListenSocketP2P(0, 0, NULL);
    host_poll_group = SteamNetworkingSockets()->CreatePollGroup();
    host_peer_count = 0;

    log_info("Created Steam host.");
}

NetworkHostSteam::~NetworkHostSteam() {
    SteamNetworkingSockets()->DestroyPollGroup(host_poll_group);
    SteamNetworkingSockets()->CloseListenSocket(host_listen_socket);
}

bool NetworkHostSteam::is_initialized_successfully() const {
    return true;
}

NetworkBackend NetworkHostSteam::get_backend() const {
    return NETWORK_BACKEND_STEAM;
}

bool NetworkHostSteam::connect(void* connection_info) {
    log_info("Connecting to host with identity %s", (char*)connection_info);

    SteamNetworkingIdentity identity;
    identity.ParseString((char*)connection_info);

    HSteamNetConnection connection = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, NULL);
    SteamNetworkingSockets()->SetConnectionPollGroup(connection, host_poll_group);
    host_peers[host_peer_count] = connection;
    host_peer_count++;

    log_debug("Steam hots now has peer with connection %u. Peer count %u", host_peers[host_peer_count], host_peer_count);
    return true;
}

void NetworkHostSteam::buffer_connection_info(void* buffer) const {
    SteamNetworkingIdentity identity;
    SteamNetworkingSockets()->GetIdentity(&identity);
    identity.ToString((char*)buffer, NETWORK_CONNECTION_INFO_BUFFER_SIZE);
}

uint16_t NetworkHostSteam::get_peer_count() const {
    return host_peer_count;
}

uint8_t NetworkHostSteam::get_peer_player_id(uint16_t peer_id) const {
    int64_t player_id = SteamNetworkingSockets()->GetConnectionUserData(host_peers[peer_id]);
    if (player_id == -1) {
        return PLAYER_NONE;
    }
    return (uint8_t)player_id;
}

void NetworkHostSteam::set_peer_player_id(uint16_t peer_id, uint8_t player_id) {
    SteamNetworkingSockets()->SetConnectionUserData(host_peers[peer_id], player_id);
}

bool NetworkHostSteam::is_peer_connected(uint16_t peer_id) const {
    SteamNetConnectionInfo_t connection_info;
    if (!SteamNetworkingSockets()->GetConnectionInfo(host_peers[peer_id], &connection_info)) {
        return false;
    }
    return connection_info.m_eState == k_ESteamNetworkingConnectionState_Connected;
}

void NetworkHostSteam::disconnect_peer(uint16_t peer_id, bool gently) {
    SteamNetworkingSockets()->CloseConnection(host_peers[peer_id], 0, "", false);
    host_peers[peer_id] = host_peers[host_peer_count - 1];
    host_peer_count--;
    log_debug("Disconnected peer. peer count %u", host_peer_count);
}

void NetworkHostSteam::send(uint16_t peer_id, void* data, size_t length) {
    SteamNetworkingSockets()->SendMessageToConnection(host_peers[peer_id], data, length, k_nSteamNetworkingSend_Reliable, NULL);
}

void NetworkHostSteam::broadcast(void* data, size_t length) {
    for (uint16_t peer_id = 0; peer_id < host_peer_count; peer_id++) {
        SteamNetworkingSockets()->SendMessageToConnection(host_peers[peer_id], data, length, k_nSteamNetworkingSend_Reliable, NULL);
    }
}

void NetworkHostSteam::flush() {
    for (uint16_t peer_id = 0; peer_id < host_peer_count; peer_id++) {
        SteamNetworkingSockets()->FlushMessagesOnConnection(host_peers[peer_id]);
    }
}

void NetworkHostSteam::service() {
    SteamNetworkingMessage_t* messages[32];
    int message_count = SteamNetworkingSockets()->ReceiveMessagesOnPollGroup(host_poll_group, messages, 32);
    for (int message_index = 0; message_index < message_count; message_index++) {
        SteamNetworkingMessage_t* message = messages[message_index];
        uint16_t peer_id;
        for (peer_id = 0; peer_id < host_peer_count; peer_id++) {
            if (message->m_conn == host_peers[peer_id]) {
                break;
            }
        }
        if (peer_id == host_peer_count) {
            log_warn("Received message from peer who is not in the peer list.");
            message->Release();
            continue;
        }

        host_events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_RECEIVED,
            .received = (NetworkHostEventReceived) {
                .peer_id = peer_id,
                .packet = (NetworkHostPacket) {
                    .data = (uint8_t*)message->m_pData,
                    .length = (size_t)message->m_cbSize,
                    ._impl = message
                }
            }
        });
    }
}

void NetworkHostSteam::destroy_packet(NetworkHostPacket* packet) {
    SteamNetworkingMessage_t* message = (SteamNetworkingMessage_t*)packet->_impl;
    message->Release();
}

#endif