#pragma once

#include "network/types.h"
#include <cstdint>
#include <queue>

enum NetworkHostEventType {
    NETWORK_HOST_EVENT_LOBBY_CREATE_SUCCESS,
    NETWORK_HOST_EVENT_LOBBY_CREATE_FAILED,
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
    virtual ~INetworkHost() = default;
    virtual bool is_initialized_successfully() const = 0;
    virtual NetworkBackend get_backend() const = 0;

    virtual void open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy) = 0;
    virtual void close_lobby() = 0;
    virtual bool connect(const NetworkConnectionInfo& connection_info) = 0;

    virtual uint16_t get_peer_count() const = 0;
    virtual uint8_t get_peer_player_id(uint16_t peer_id) const = 0;
    virtual void set_peer_player_id(uint16_t peer_id, uint8_t player_id) = 0;
    virtual NetworkConnectionInfo get_peer_connection_info(uint16_t peer_id) const = 0;
    virtual void disconnect_peers() = 0;

    virtual void send(uint16_t peer_id, void* data, size_t length) = 0;
    virtual void broadcast(void* data, size_t length) = 0;
    virtual void flush() = 0;
    virtual void service() = 0;

    virtual void destroy_packet(NetworkHostPacket* packet) = 0;

    bool poll_events(NetworkHostEvent* event);

    const char* get_lobby_name() const;
    void set_lobby_name(const char* value);
protected:
    std::queue<NetworkHostEvent> host_events;
    char host_lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
};