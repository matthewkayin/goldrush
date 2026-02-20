#pragma once

#include "defines.h"

#ifdef GOLD_STEAM

#include "network/host.h"

class NetworkHostSteam : public INetworkHost {
public:
    NetworkHostSteam();
    ~NetworkHostSteam();

    bool is_initialized_successfully() const override;
    NetworkBackend get_backend() const override;

    void open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy) override;
    void close_lobby() override;

    bool connect(void* connection_info) override;
    void buffer_connection_info(void* buffer) const override;

    uint16_t get_peer_count() const override;
    uint8_t get_peer_player_id(uint16_t peer_id) const override;
    void set_peer_player_id(uint16_t peer_id, uint8_t player_id) override;
    bool is_peer_connected(uint16_t peer_id) const override;
    void disconnect_peer(uint16_t peer_id, bool gently) override;

    size_t buffer_peer_connection_info(uint16_t peer_id, void* buffer) const override;

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