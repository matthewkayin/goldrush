#pragma once

#include "common.h"
#include <cstdint>

void network_steam_init();
void network_steam_service();

void network_steam_search_lobbies(const char* query);
uint32_t network_steam_get_lobby_count();
const NetworkLobby& network_steam_get_lobby(uint32_t index);

void network_steam_create_lobby(const char* lobby_name);
void network_steam_join_lobby(uint32_t index);