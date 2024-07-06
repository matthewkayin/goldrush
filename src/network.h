#pragma once

#include "defines.h"
#include <cstdint>
#include <cstddef>

const size_t MAX_USERNAME_LENGTH = 14;
const size_t INPUT_BUFFER_SIZE = 1024;

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
    char name[16];
    uint8_t status;
    uint8_t padding[3];
};

enum NetworkEventType {
    NETWORK_EVENT_CONNECTION_FAILED,
    NETWORK_EVENT_SERVER_DISCONNECTED,
    NETWORK_EVENT_CLIENT_DISCONNECTED,
    NETWORK_EVENT_JOINED_LOBBY,
    NETWORK_EVENT_LOBBY_FULL,
    NETWORK_EVENT_INVALID_VERSION,
    NETWORK_EVENT_CLIENT_READY,
    NETWORK_EVENT_MATCH_LOAD,
    NETWORK_EVENT_MATCH_START,
    NETWORK_EVENT_INPUT
};

struct network_event_input_t {
    uint8_t data[INPUT_BUFFER_SIZE];
    uint8_t data_length;
};

struct network_event_t {
    NetworkEventType type;
    union {
        network_event_input_t input;
    };
};

bool network_init();
void network_quit();
void network_disconnect();

bool network_is_server();
NetworkStatus network_get_status();
uint8_t network_get_player_id();
const player_t& network_get_player(uint8_t player_id);

void network_service();
bool network_poll_events(network_event_t* event);

bool network_server_create(const char* username);
void network_server_broadcast_playerlist();
void network_server_start_loading();
void network_server_start_match();
void network_server_send_input(uint8_t* data, size_t data_length);

bool network_client_create(const char* username, const char* server_ip);
void network_client_toggle_ready();
void network_client_send_input(uint8_t* data, size_t data_length);