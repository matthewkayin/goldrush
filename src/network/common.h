#pragma once

#include <cstdint>

#define NETWORK_LOBBY_NAME_BUFFER_SIZE 40
#define NETWORK_IP_BUFFER_SIZE 32
#define NETWORK_PLAYER_NAME_BUFFER_SIZE 36
#define NETWORK_APP_VERSION_BUFFER_SIZE 16
#define NETWORK_CHAT_BUFFER_SIZE 128
#define NETWORK_SCANNER_PORT 6529
#define NETWORK_BASE_PORT 6530

enum NetworkBackend {
    NETWORK_BACKEND_NONE,
    NETWORK_BACKEND_LAN,
    NETWORK_BACKEND_STEAM
};

enum NetworkStatus {
    NETWORK_STATUS_OFFLINE,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_HOST,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_DISCONNECTING
};

struct NetworkLobby {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
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

void network_common_init();
const char* network_get_username();