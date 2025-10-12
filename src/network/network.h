#pragma once

#include "types.h"

bool network_init();
void network_quit();

void network_set_backend(NetworkBackend backend);
NetworkBackend network_get_backend();
const char* network_get_username();
NetworkStatus network_get_status();
bool network_is_host();
const NetworkPlayer& network_get_player(uint8_t player_id);
uint8_t network_get_player_id();
uint8_t network_get_player_count();

void network_service();
bool network_poll_events(NetworkEvent* event);
void network_disconnect();

void network_search_lobbies(const char* query);
size_t network_get_lobby_count();
const NetworkLobby& network_get_lobby(size_t index);
void network_open_lobby(const char* lobby_name, NetworkLobbyPrivacy privacy);
void network_join_lobby(NetworkConnectionInfo connection_info);
#ifdef GOLD_STEAM
void network_steam_accept_invite(CSteamID lobby_id);
#endif
const char* network_get_lobby_name();
void network_send_chat(const char* message);
void network_set_player_ready(bool ready);
void network_set_player_color(uint8_t player_id, uint8_t color);
void network_set_match_setting(uint8_t setting, uint8_t value);
uint8_t network_get_match_setting(uint8_t setting);
void network_set_player_team(uint8_t player_id, uint8_t team);
void network_add_bot();
void network_remove_bot(uint8_t player_id);
void network_begin_loading_match(int32_t lcg_seed, const Noise& noise);
void network_send_input(uint8_t* out_buffer, size_t out_buffer_length);