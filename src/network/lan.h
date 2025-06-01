#pragma once

#include "common.h"
#include "match/noise.h"

bool network_lan_init();
void network_lan_quit();

void network_lan_service();
void network_lan_disconnect();

void network_lan_search_lobbies(const char* query);
uint32_t network_lan_get_lobby_count();
const NetworkLobby& network_lan_get_lobby(uint32_t index);

void network_lan_create_lobby(const char* lobby_name);
void network_lan_join_lobby(uint32_t index);

NetworkStatus network_lan_get_status();
const NetworkPlayer& network_lan_get_player(uint8_t player_id);
uint8_t network_lan_get_player_id();
const char* network_lan_get_lobby_name();
void network_lan_send_chat(const char* message);
void network_lan_set_player_ready(bool ready);
void network_lan_set_player_color(uint8_t color);
void network_lan_set_match_setting(uint8_t setting, uint8_t value);
uint8_t network_lan_get_match_setting(uint8_t setting);
void network_lan_set_player_team(uint8_t team);
void network_lan_begin_loading_match(int32_t lcg_seed, const Noise& noise);
void network_lan_send_input(uint8_t* out_buffer, size_t out_buffer_length);