#pragma once

#include "network/host.h"
#include <enet/enet.h>

STATIC_ASSERT(NETWORK_CONNECTION_INFO_BUFFER_SIZE >= sizeof(NetworkConnectionInfoLan));

class NetworkHostLan : public INetworkHost {
public:
    NetworkHostLan();
    ~NetworkHostLan() override;

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
    ENetHost* host;
    ENetSocket host_listener_socket;
};