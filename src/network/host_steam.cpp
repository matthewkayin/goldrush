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

void NetworkHostSteam::open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy) {
    set_lobby_name(lobby_name);

    ELobbyType steam_lobby_type = privacy == NETWORK_LOBBY_PRIVACY_PUBLIC
        ? k_ELobbyTypePublic
        : k_ELobbyTypeFriendsOnly;
    // Only 2 members in the lobby because this lobby exists for discovery purposes only
    SteamMatchmaking()->CreateLobby(steam_lobby_type, 2);
}

void NetworkHostSteam::close_lobby() {
    SteamMatchmaking()->LeaveLobby(host_lobby_id);
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

size_t NetworkHostSteam::buffer_peer_connection_info(uint16_t peer_id, void* buffer) const {
    SteamNetConnectionInfo_t connection_info;
    if (!SteamNetworkingSockets()->GetConnectionInfo(host_peers[peer_id], &connection_info)) {
        return 0;
    }

    uint64_t steam_id = connection_info.m_identityRemote.GetSteamID64();
    memcpy(buffer, &steam_id, sizeof(steam_id));
    return sizeof(steam_id);
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

void NetworkHostSteam::update_lobby_player_count() {

}

void NetworkHostSteam::on_lobby_created(LobbyCreated_t* lobby_created) {
    if (lobby_created->m_eResult != k_EResultOK) {
        host_events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_LOBBY_CREATE_FAILED
        });

        log_error("Steam host failed to create lobby %u", lobby_created->m_eResult);
        return;
    }

    // Set steam lobby property `name`
    host_lobby_id = lobby_created->m_ulSteamIDLobby;
    SteamMatchmaking()->SetLobbyData(host_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME, get_lobby_name());

    // Set steam lobby property `host_identity`
    NetworkConnectionInfoSteam connection_info;
    SteamNetworkingIdentity identity;
    SteamNetworkingSockets()->GetIdentity(&identity);
    identity.ToString(connection_info.identity_str, sizeof(connection_info.identity_str));
    SteamMatchmaking()->SetLobbyData(host_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY, connection_info.identity_str);

    update_lobby_player_count();

    host_events.push((NetworkHostEvent) {
        .type = NETWORK_HOST_EVENT_LOBBY_CREATE_SUCCESS
    });
    log_info("Steam host created lobby successfully.");
}

void NetworkHostSteam::on_connection_status_changed(SteamNetConnectionStatusChangedCallback_t* callback) {
    log_debug("Steam on_connection_status_changed connection %u status %u -> %u", callback->m_hConn, callback->m_eOldState, callback->m_info.m_eState);

    // On host connecting
    if (callback->m_eOldState == k_ESteamNetworkingConnectionState_None && 
            callback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting) {
        // If we already have this connection in our peers list, it means we reached out to them
        // So there is nothing for us to do here
        for (uint16_t peer_id = 0; peer_id < host_peer_count; peer_id++) {
            if (host_peers[peer_id] == callback->m_hConn) {
                log_debug("Peer is already recognized. Doing nothing.");
                return;
            }
        }

        // Otherwise, they reached out to us. 
        // Do we have enough space in the peer list to accept them?
        // If we don't, it should mean that we're the host and the lobby is full.
        // TODO: replace this logic with a check on network_get_player_count() so that we can account for bots taking up lobby space
        if (host_peer_count == MAX_PLAYERS - 1) {
            log_debug("Peer count is full. Rejecting.");
            SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
            return;
        }

        // If we do, accept the connection
        log_debug("Accepted connection.");
        host_peers[host_peer_count] = callback->m_hConn;
        SteamNetworkingSockets()->AcceptConnection(host_peers[host_peer_count]);
        SteamNetworkingSockets()->SetConnectionPollGroup(host_peers[host_peer_count], host_poll_group);
        host_peer_count++;
        return;
    }

    // Connection
    if (callback->m_info.m_eState == k_ESteamNetworkingConnectionState_Connected) {
        // Determine incoming peer id
        uint16_t peer_id;
        for (peer_id = 0; peer_id < host_peer_count; peer_id++) {
            if (host_peers[peer_id] == callback->m_hConn) {
                break;
            }
        }

        if (peer_id == host_peer_count) {
            log_warn("Received connection event from a host that is not in the peer list.");
            return;
        }

        host_events.push((NetworkHostEvent) {
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
        for (peer_id = 0; peer_id < host_peer_count; peer_id++) {
            if (callback->m_hConn == host_peers[peer_id]) {
                break;
            }
        }

        // If we know who this is, so post an event. 
        if (peer_id != host_peer_count) {
            host_events.push((NetworkHostEvent) {
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

#endif