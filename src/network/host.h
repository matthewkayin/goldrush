#pragma once

#include "types.h"
#include <enet/enet.h>
#include <steam/steam_api.h>
#include <queue>

enum NetworkHostEventType {
    NETWORK_HOST_EVENT_CONNECTED,
    NETWORK_HOST_EVENT_DISCONNECTED,
    NETWORK_HOST_EVENT_RECEIVED
};

struct NetworkHostPacket {
    NetworkBackend backend;
    uint8_t* data;
    size_t length;
    union {
        ENetPacket* enet_packet;
        SteamNetworkingMessage_t* steam_message;
    };
};

struct NetworkHostEventConnected {
    uint16_t peer_id;
};

struct NetworkHostEventDisconnected {
    uint8_t player_id;
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

class NetworkHost {
public:
    virtual ~NetworkHost() = default;
    virtual bool was_create_successful() const = 0;
    virtual NetworkConnectionInfo get_connection_info() const = 0;
    virtual bool connect(NetworkConnectionInfo connection_info) = 0;
    virtual uint16_t get_peer_count() const = 0;
    virtual uint8_t get_peer_player_id(uint16_t peer_id) const = 0;
    virtual void set_peer_player_id(uint16_t peer_id, uint8_t player_id) = 0;
    virtual bool is_peer_connected(uint16_t peer_id) const = 0;
    virtual void disconnect_peer(uint16_t peer_id, bool gently) = 0;
    virtual size_t buffer_peer_connection_info(uint16_t peer_id, void* buffer) = 0;
    virtual NetworkConnectionInfo parse_connection_info(void* buffer) = 0;
    virtual void send(uint16_t peer_id, void* data, size_t length) = 0;
    virtual void broadcast(void* data, size_t length) = 0;
    virtual void flush() = 0;
    virtual void service() = 0;
    bool poll_events(NetworkHostEvent* event);
protected:
    std::queue<NetworkHostEvent> events;
};

class NetworkLanHost : public NetworkHost {
public:
    NetworkLanHost();
    ~NetworkLanHost();
    bool was_create_successful() const override;
    bool connect(NetworkConnectionInfo connection_info) override;
    NetworkConnectionInfo get_connection_info() const override;
    uint16_t get_peer_count() const override;
    uint8_t get_peer_player_id(uint16_t peer_id) const override;
    void set_peer_player_id(uint16_t peer_id, uint8_t player_id) override;
    bool is_peer_connected(uint16_t peer_id) const override;
    void disconnect_peer(uint16_t peer_id, bool gently) override;
    size_t buffer_peer_connection_info(uint16_t peer_id, void* buffer) override;
    NetworkConnectionInfo parse_connection_info(void* buffer) override;
    void send(uint16_t peer_id, void* data, size_t length) override;
    void broadcast(void* data, size_t length) override;
    void flush() override;
    void service() override;
private:
    ENetHost* host;
    ENetPeer* peers[MAX_PLAYERS];
};

class NetworkSteamHost : public NetworkHost {
public:
    NetworkSteamHost();
    ~NetworkSteamHost();
    bool was_create_successful() const override;
    bool connect(NetworkConnectionInfo connection_info) override;
    NetworkConnectionInfo get_connection_info() const override;
    uint16_t get_peer_count() const override;
    uint8_t get_peer_player_id(uint16_t peer_id) const override;
    void set_peer_player_id(uint16_t peer_id, uint8_t player_id) override;
    bool is_peer_connected(uint16_t peer_id) const override;
    void disconnect_peer(uint16_t peer_id, bool gently) override;
    size_t buffer_peer_connection_info(uint16_t peer_id, void* buffer) override;
    NetworkConnectionInfo parse_connection_info(void* buffer) override;
    void send(uint16_t peer_id, void* data, size_t length) override;
    void broadcast(void* data, size_t length) override;
    void flush() override;
    void service() override;
private:
    HSteamListenSocket listen_socket;
    HSteamNetPollGroup poll_group;
    HSteamNetConnection peers[MAX_PLAYERS - 1];
    uint16_t peer_count;
    STEAM_CALLBACK(NetworkSteamHost, on_connection_status_changed, SteamNetConnectionStatusChangedCallback_t);
};

void network_host_packet_destroy(NetworkHostPacket packet);