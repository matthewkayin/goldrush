#pragma once

#include "defines.h"

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

void network_quit();
void network_disconnect();

bool network_is_server();
NetworkStatus network_get_status();
PlayerStatus network_get_player_status(uint8_t player_id);
const char* network_get_player_name(uint8_t player_id);

void network_service();
void network_handle_message(uint8_t* data, size_t length, uint16_t peer_id);

bool network_server_create(const char* username);
bool network_client_create(const char* username, const char* server_ip);