#pragma once

#include "defines.h"

#ifdef GOLD_STEAM

#include "network/interface/host.h"
#include <steam/steam_api.h>

STATIC_ASSERT(NETWORK_CONNECTION_INFO_BUFFER_SIZE >= SteamNetworkingIdentity::k_cchMaxString);

class NetworkHostSteam : public INetworkHost {
public:
    NetworkHostSteam();
    ~NetworkHostSteam() override;

    bool is_initialized_successfully() const override;
    NetworkBackend get_backend() const override;

    void open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy) override;
    void close_lobby() override;
    bool connect(const NetworkConnectionInfo& connection_info) override;

    uint16_t get_peer_count() const override;
    uint8_t get_peer_player_id(uint16_t peer_id) const override;
    void set_peer_player_id(uint16_t peer_id, uint8_t player_id) override;
    NetworkConnectionInfo get_peer_connection_info(uint16_t peer_id) const override;
    void disconnect_peers() override;

    void send(uint16_t peer_id, void* data, size_t length) override;
    void broadcast(void* data, size_t length) override;
    void flush() override;
    void service() override;

    void destroy_packet(NetworkHostPacket* packet) override;
private:
    HSteamListenSocket host_listen_socket;
    HSteamNetPollGroup host_poll_group;
    HSteamNetConnection host_peers[MAX_PLAYERS - 1];
    uint16_t host_peer_count;
    uint8_t host_lobby_player_count;
    CSteamID host_lobby_id;

    void update_lobby_player_count();

    STEAM_CALLBACK(NetworkHostSteam, on_lobby_created, LobbyCreated_t);
    STEAM_CALLBACK(NetworkHostSteam, on_connection_status_changed, SteamNetConnectionStatusChangedCallback_t);
};

#endif