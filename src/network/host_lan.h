#pragma once

#include "network/host.h"
#include <enet/enet.h>

class NetworkHostLan : public INetworkHost {
public:
    NetworkHostLan();
    ~NetworkHostLan();

    bool is_initialized_successfully() const override;
    NetworkBackend get_backend() const override;

    bool connect(void* connection_info) override;
    void buffer_connection_info(void* connection_info) const override;

    uint16_t get_peer_count() const override;
    uint8_t get_peer_player_id(uint16_t peer_id) const override;
    void set_peer_player_id(uint16_t peer_id, uint8_t player_id) override;
    bool is_peer_connected(uint16_t peer_id) const override;
    void disconnect_peer(uint16_t peer_id, bool gently) override;

    size_t buffer_peer_connection_info(uint16_t peer_id, void* buffer) const override;
    void* parse_connection_info(void* buffer) const override;

    void send(uint16_t peer_id, void* data, size_t length) override;
    void broadcast(void* data, size_t length) override;
    void flush() override;
    void service() override;

    void destroy_packet(NetworkHostPacket* packet) override;
private:
    ENetHost* host;
};