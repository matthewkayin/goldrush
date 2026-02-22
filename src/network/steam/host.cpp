#include "host.h"

#ifdef GOLD_STEAM

#include "core/logger.h"
#include "core/asserts.h"
#include "network/network.h"

NetworkHostSteam::NetworkHostSteam() {
    memset(host_lobby_name, 0, sizeof(host_lobby_name));
    host_lobby_id.Clear();

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
    strncpy(host_lobby_name, lobby_name, NETWORK_LOBBY_NAME_BUFFER_SIZE);

    ELobbyType steam_lobby_type = privacy == NETWORK_LOBBY_PRIVACY_PUBLIC
        ? k_ELobbyTypePublic
        : k_ELobbyTypeFriendsOnly;
    // Only 2 members in the lobby because this lobby exists for discovery purposes only
    SteamMatchmaking()->CreateLobby(steam_lobby_type, 2);
}

void NetworkHostSteam::close_lobby() {
    SteamMatchmaking()->LeaveLobby(host_lobby_id);
    host_lobby_id.Clear();
}

bool NetworkHostSteam::connect(const NetworkConnectionInfo& connection_info) {
    log_info("Connecting to host with identity %s", (char*)connection_info.steam.identity_str);

    SteamNetworkingIdentity identity;
    identity.ParseString(connection_info.steam.identity_str);

    HSteamNetConnection connection = SteamNetworkingSockets()->ConnectP2P(identity, 0, 0, NULL);
    SteamNetworkingSockets()->SetConnectionPollGroup(connection, host_poll_group);
    host_peers[host_peer_count] = connection;
    host_peer_count++;

    log_debug("Steam hots now has peer with connection %u. Peer count %u", host_peers[host_peer_count], host_peer_count);
    return true;
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

NetworkConnectionInfo NetworkHostSteam::get_peer_connection_info(uint16_t peer_id) const {
    NetworkConnectionInfo connection_info;

    SteamNetConnectionInfo_t steam_connection_info;
    if (!SteamNetworkingSockets()->GetConnectionInfo(host_peers[peer_id], &steam_connection_info)) {
        log_warn("Network Steam host tried to get_peer_connection_info on a peer whom we are not connected to.");
        GOLD_ASSERT(false);
        return connection_info;
    }

    steam_connection_info.m_identityRemote.ToString(connection_info.steam.identity_str, sizeof(connection_info.steam.identity_str));

    return connection_info;
}

void NetworkHostSteam::disconnect_peers() {
    for (uint16_t peer_id = 0; peer_id < host_peer_count; peer_id++) {
        SteamNetworkingSockets()->CloseConnection(host_peers[peer_id], 0, "", false);
    }
    host_peer_count = 0;
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
    if (host_lobby_id.IsValid() && host_lobby_player_count != network_get_player_count()) {
        update_lobby_player_count();
    }

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
    host_lobby_player_count = network_get_player_count();
    char buffer[2] = { (char)host_lobby_player_count, '\0' };
    SteamMatchmaking()->SetLobbyData(host_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT, buffer);
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
    SteamMatchmaking()->SetLobbyData(host_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_NAME, host_lobby_name);

    // Set steam lobby property `host_identity`
    char identity_str[SteamNetworkingIdentity::k_cchMaxString];
    SteamNetworkingIdentity identity;
    SteamNetworkingSockets()->GetIdentity(&identity);
    identity.ToString(identity_str, sizeof(identity_str));
    SteamMatchmaking()->SetLobbyData(host_lobby_id, NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY, identity_str);

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
        if (network_get_player_count() == MAX_PLAYERS) {
            log_debug("Lobby is full. Rejecting.");
            SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
            return;
        }
        GOLD_ASSERT(host_peer_count < MAX_PLAYERS - 1);

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
        // Determine peer_id to remove
        uint16_t peer_id;
        for (peer_id = 0; peer_id < host_peer_count; peer_id++) {
            if (callback->m_hConn == host_peers[peer_id]) {
                break;
            }
        }

        // Post an event
        // If we pass PLAYER_NONE, the network will usually ignore the disconnection event
        // However it is useful to pass the event because this might represent a failure to 
        // disconnect to a lobby, so the event can trigger the client to back out to menu
        const uint8_t disconnected_player_id = peer_id != host_peer_count
            ? get_peer_player_id(peer_id)
            : PLAYER_NONE;
        host_events.push((NetworkHostEvent) {
            .type = NETWORK_HOST_EVENT_DISCONNECTED,
            .disconnected = (NetworkHostEventDisconnected) {
                .player_id = disconnected_player_id
            }
        });

        // If we know who this is, remove them
        if (peer_id != host_peer_count) {
            host_peers[peer_id] = host_peers[host_peer_count - 1];
            host_peer_count--;
        }

        // Disconnect 
        SteamNetworkingSockets()->CloseConnection(callback->m_hConn, 0, "", false);
    }
}

#endif