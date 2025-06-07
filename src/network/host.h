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

struct NetworkLanHost {
    ENetHost* host;
};

struct NetworkSteamHost {
    HSteamListenSocket listen_socket;
    HSteamNetPollGroup poll_group;
    HSteamNetConnection peers[MAX_PLAYERS - 1];
    uint16_t peer_count;
};

struct NetworkHost {
    NetworkBackend backend;
    std::queue<NetworkHostEvent>* events;
    union {
        NetworkLanHost lan;
        NetworkSteamHost steam;
    };
};

NetworkHost* network_host_create(NetworkBackend backend);
void network_host_destroy(NetworkHost* host);
bool network_host_connect(NetworkHost* host, NetworkConnectionInfo connection_info);
NetworkConnectionInfo network_host_get_connection_info(NetworkHost* host);
uint16_t network_host_get_peer_count(NetworkHost* host);
uint8_t network_host_get_peer_player_id(NetworkHost* host, uint16_t peer_id);
void network_host_set_peer_player_id(NetworkHost* host, uint16_t peer_id, uint8_t player_id);
bool network_host_is_peer_connected(NetworkHost* host, uint16_t peer_id);
void network_host_disconnect_peer(NetworkHost* host, uint16_t peer_id, bool gently);
size_t network_host_buffer_peer_connection_info(NetworkHost* host, uint16_t peer_id, void* buffer);
NetworkConnectionInfo network_parse_connection_info(NetworkBackend backend, void* buffer);
void network_host_send(NetworkHost* host, uint16_t peer_id, void* data, size_t length);
void network_host_broadcast(NetworkHost* host, void* data, size_t length);
void network_host_flush(NetworkHost* host);
void network_host_service(NetworkHost* host);
void network_host_steam_on_connection_status_changed(NetworkHost* host, SteamNetConnectionStatusChangedCallback_t* callback);
bool network_host_poll_events(NetworkHost* host, NetworkHostEvent* event);

void network_host_packet_destroy(NetworkHostPacket packet);