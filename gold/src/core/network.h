#pragma once

#include <cstdint>

#define NETWORK_LOBBY_NAME_BUFFER_SIZE 64
#define NETWORK_LOBBY_NAME_MAX 40
#define NETWORK_PLAYER_NAME_BUFFER_SIZE 36
#define NETWORK_IP_BUFFER_SIZE 32
#define NETWORK_LOBBY_CHAT_BUFFER_SIZE 128
#define NETWORK_LOBBY_SEARCH_BUFFER_SIZE 128
#define NETWORK_INPUT_BUFFER_SIZE 1024

enum network_status {
    NETWORK_STATUS_OFFLINE,
    NETWORK_STATUS_SERVER,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_DISCONNECTING
};

enum player_status {
    PLAYER_STATUS_NONE,
    PLAYER_STATUS_HOST,
    PLAYER_STATUS_NOT_READY,
    PLAYER_STATUS_READY,
    PLAYER_STATUS_DISCONNECTED
};

enum network_error {
    NETWORK_ERROR_NONE,
    NETWORK_ERROR_NOT_READY,
    NETWORK_ERROR_SAME_COLOR
};

struct player_t {
    uint8_t status;
    uint8_t color;
    uint8_t team;
    char name[NETWORK_PLAYER_NAME_BUFFER_SIZE];
};

struct lobby_info_t {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint16_t port;
    uint8_t player_count;
    uint8_t padding;
};

struct lobby_t {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    char ip[NETWORK_IP_BUFFER_SIZE];
    uint16_t port;
    uint8_t player_count;
};

enum network_event_type {
    NETWORK_EVENT_LOBBY_INFO,
    NETWORK_EVENT_CONNECTION_FAILED,
    NETWORK_EVENT_PLAYER_DISCONNECTED,
    NETWORK_EVENT_JOINED_LOBBY,
    NETWORK_EVENT_LOBBY_FULL,
    NETWORK_EVENT_INVALID_VERSION,
    NETWORK_EVENT_GAME_ALREADY_STARTED,
    NETWORK_EVENT_MATCH_LOAD,
    NETWORK_EVENT_LOBBY_CHAT,
    NETWORK_EVENT_INPUT
};

struct network_event_lobby_t {
    lobby_t lobby;
};

struct network_event_input_t {
    uint8_t in_buffer[NETWORK_INPUT_BUFFER_SIZE];
    uint8_t in_buffer_length;
    uint8_t player_id;
};

struct network_event_player_disconnected_t {
    uint8_t player_id;
};

struct network_event_lobby_chat_t {
    char message[NETWORK_LOBBY_CHAT_BUFFER_SIZE];
};

struct network_event_match_load_t {
    int32_t lcg_seed;
    // noise_t noise;
};

struct network_event_t {
    network_event_type type;
    union {
        network_event_lobby_t lobby;
        network_event_player_disconnected_t player_disconnected;
        network_event_input_t input;
        network_event_lobby_chat_t lobby_chat;
        network_event_match_load_t match_load;
    };
};

bool network_init();
void network_quit();
void network_disconnect();

bool network_scanner_create();
void network_scanner_search(const char* query);
void network_scanner_destroy();
bool network_server_create(const char* username);
bool network_client_create(const char* username, const char* server_ip, uint16_t port);

void network_service();
void network_handle_message(uint8_t* data, size_t length, uint16_t incoming_peer_id);
bool network_poll_events(network_event_t* event);