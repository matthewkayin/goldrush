#pragma once

#include "common.h"

bool network_init();
void network_quit();

void network_set_backend(NetworkBackend backend);
void network_service();

void network_search_lobbies(const char* query);
uint32_t network_get_lobby_count();
const NetworkLobby& network_get_lobby(uint32_t index);

void network_create_lobby(const char* lobby_name);
void network_join_lobby(uint32_t index);

NetworkStatus network_get_status();
bool network_is_host();
const NetworkPlayer& network_get_player(uint8_t player_id);
uint8_t network_get_player_id();
const char* network_get_lobby_name();