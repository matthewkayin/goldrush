#pragma once

#include <cstdint>

#define NETWORK_LOBBY_NAME_BUFFER_SIZE 40
#define NETWORK_IP_BUFFER_SIZE 32
#define NETWORK_PLAYER_NAME_BUFFER_SIZE 36
#define NETWORK_APP_VERSION_BUFFER_SIZE 16
#define NETWORK_LOBBY_CHAT_BUFFER_SIZE 64
#define NETWORK_SCANNER_PORT 6529
#define NETWORK_BASE_PORT 6530

enum NetworkStatus {
    NETWORK_STATUS_OFFLINE,
    NETWORK_STATUS_SERVER,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_DISCONNECTING
};

struct NetworkLobbyInfo {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint16_t port;
    uint8_t player_count;
    uint8_t padding;
};

struct NetworkLobby {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    char ip[NETWORK_IP_BUFFER_SIZE];
    uint16_t port;
    uint8_t player_count;
};

enum NetworkPlayerStatus {
    NETWORK_PLAYER_STATUS_NONE,
    NETWORK_PLAYER_STATUS_HOST,
    NETWORK_PLAYER_STATUS_NOT_READY,
    NETWORK_PLAYER_STATUS_READY,
    NETWORK_PLAYER_STATUS_DISCONNECTED
};

struct NetworkPlayer {
    uint8_t status;
    uint8_t recolor_id;
    uint8_t team;
    char name[NETWORK_PLAYER_NAME_BUFFER_SIZE];
};

enum NetworkEventType {
    NETWORK_EVENT_CONNECTION_FAILED,
    NETWORK_EVENT_PLAYER_DISCONNECTED,
    NETWORK_EVENT_INVALID_VERSION,
    NETWORK_EVENT_JOINED_LOBBY,
    NETWORK_EVENT_LOBBY_CHAT,
    NETWORK_EVENT_RECEIVED_LOBBY_INFO
};

struct NetworkEventPlayerDisconnected {
    uint8_t player_id;
};

struct NetworkEventLobbyChat {
    char message[NETWORK_LOBBY_CHAT_BUFFER_SIZE];
};

struct NetworkEvent {
    NetworkEventType type;
    union {
        NetworkEventPlayerDisconnected player_disconnected;
        NetworkEventLobbyChat lobby_chat;
    };
};

bool network_init();
void network_quit();
void network_disconnect();

bool network_scanner_create();
void network_scanner_destroy();
void network_scanner_search(const char* query);

bool network_server_create(const char* username);
bool network_client_create(const char* username, const char* server_ip, uint16_t port);

void network_service();
bool network_poll_events(NetworkEvent* event);

NetworkStatus network_get_status();
bool network_is_server();
const NetworkPlayer& network_get_player(uint8_t player_id);
uint8_t network_get_player_id();
const size_t network_get_lobby_count();
const NetworkLobby& network_get_lobby(size_t index);
const char* network_get_lobby_name();