#pragma once

#include "common.h"
#include "match/noise.h"
#include <cstdint>

void network_steam_init();
void network_steam_service();
void network_steam_disconnect();

void network_steam_search_lobbies(const char* query);
uint32_t network_steam_get_lobby_count();
const NetworkLobby& network_steam_get_lobby(uint32_t index);

void network_steam_create_lobby(const char* lobby_name);
void network_steam_join_lobby(uint32_t index);

NetworkStatus network_steam_get_status();
const NetworkPlayer& network_steam_get_player(uint8_t player_id);
uint8_t network_steam_get_player_id();
const char* network_steam_get_lobby_name();
void network_steam_send_chat(const char* message);
void network_steam_set_player_ready(bool ready);
void network_steam_set_player_color(uint8_t color);
void network_steam_set_match_setting(uint8_t setting, uint8_t value);
uint8_t network_steam_get_match_setting(uint8_t setting);
void network_steam_set_player_team(uint8_t team);
void network_steam_begin_loading_match(int32_t lcg_seed, const Noise& noise);
void network_steam_send_input(uint8_t* out_buffer, size_t out_buffer_length);