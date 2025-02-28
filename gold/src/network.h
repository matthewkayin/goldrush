#pragma once

#include "defines.h"
#include "noise.h"
#include "match_settings.h"
#include <cstddef>

#define NAME_BUFFER_SIZE 36
#define LOBBY_NAME_BUFFER_SIZE 64
#define LOBBY_CHAT_BUFFER_SIZE 128
#define LOBBY_NAME_MAX 40
#define LOBBY_SEARCH_BUFFER_SIZE 128

enum NetworkStatus {
    NETWORK_STATUS_OFFLINE,
    NETWORK_STATUS_SERVER,
    NETWORK_STATUS_CONNECTING,
    NETWORK_STATUS_CONNECTED,
    NETWORK_STATUS_DISCONNECTING
};

enum PlayerStatus {
    PLAYER_STATUS_NONE,
    PLAYER_STATUS_HOST,
    PLAYER_STATUS_NOT_READY,
    PLAYER_STATUS_READY,
    PLAYER_STATUS_DISCONNECTED
};

struct player_t {
    uint8_t status;
    uint8_t recolor_id;
    char name[NAME_BUFFER_SIZE];
};

struct lobby_info_t {
    char name[LOBBY_NAME_BUFFER_SIZE];
    uint16_t port;
    uint8_t player_count;
    uint8_t padding;
};

struct lobby_t {
    char name[LOBBY_NAME_BUFFER_SIZE];
    char ip[32];
    uint16_t port;
    uint8_t player_count;
};

enum NetworkEventType {
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

struct network_event_input_t {
    uint8_t in_buffer[INPUT_BUFFER_SIZE];
    uint8_t in_buffer_length;
    uint8_t player_id;
};

struct network_event_player_disconnected_t {
    uint8_t player_id;
};

struct network_event_lobby_chat_t {
    char message[LOBBY_CHAT_BUFFER_SIZE];
};

struct network_event_match_load_t {
    int32_t lcg_seed;
    noise_t noise;
};

struct network_event_t {
    NetworkEventType type;
    union {
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

bool network_is_server();
NetworkStatus network_get_status();
const player_t& network_get_player(uint8_t player_id);
uint8_t network_get_player_id();
bool network_are_all_players_ready();
const size_t network_get_lobby_count();
const lobby_t& network_get_lobby(size_t index);
const char* network_get_lobby_name();
void network_send_lobby_chat_message(const char* message);
uint32_t network_get_match_setting(MatchSetting setting);
void network_set_match_setting(MatchSetting setting, uint32_t value);

void network_toggle_ready();
void network_set_player_color(uint8_t recolor_id);
void network_begin_loading_match(int32_t lcg_seed, const noise_t& noise);
void network_send_input(uint8_t* out_buffer, size_t out_buffer_length);

void network_service();
void network_handle_message(uint8_t* data, size_t length, uint16_t peer_id);
bool network_poll_events(network_event_t* event);