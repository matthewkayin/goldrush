#pragma once

#include "network/types.h"
#include <cstdint>
#include <queue>

enum NetworkHostEventType {
    NETWORK_HOST_EVENT_CONNECTED,
    NETWORK_HOST_EVENT_DISCONNECTED,
    NETWORK_HOST_EVENT_RECEIVED
};

struct NetworkHostEventConnected {
    uint16_t peer_id;
};

struct NetworkHostEventDisconnected {
    uint8_t player_id;
};

struct NetworkHostPacket {
    uint8_t* data;
    size_t length;
    void* _impl;
};

struct NetworkHostEventReceived {
    uint16_t peer_id;
    NetworkHostPacket packet;
};

struct NetworkHostEvent {
    NetworkHostEventType type;
    union {
        NetworkHostEventConnected connected;
        NetworkHostEventDisconnected disconnected;
        NetworkHostEventReceived received;
    };
};


class INetworkHost {
public:
    ~INetworkHost() {};
    virtual bool is_initialized_successfully() const = 0;
    virtual NetworkBackend get_backend() const = 0;

    virtual bool connect(void* connection_info) = 0;
    virtual void buffer_connection_info(void* connection_info) const = 0;

    virtual uint16_t get_peer_count() const = 0;
    virtual uint8_t get_peer_player_id(uint16_t peer_id) const = 0;
    virtual void set_peer_player_id(uint16_t peer_id, uint8_t player_id) = 0;
    virtual bool is_peer_connected(uint16_t peer_id) const = 0;
    virtual void disconnect_peer(uint16_t peer_id, bool gently) = 0;

    virtual size_t buffer_peer_connection_info(uint16_t peer_id, void* buffer) const = 0;
    virtual void* parse_connection_info(void* buffer) const = 0;

    virtual void send(uint16_t peer_id, void* data, size_t length) = 0;
    virtual void broadcast(void* data, size_t length) = 0;
    virtual void flush() = 0;
    virtual void service() = 0;

    virtual void destroy_packet(NetworkHostPacket* packet) = 0;

    bool poll_events(NetworkHostEvent* event);
protected:
    std::queue<NetworkHostEvent> host_events;
};