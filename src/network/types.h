#pragma once

#include "match/noise.h"
#include "menu/match_setting.h"
#include <cstdint>
#include <steam/steam_api.h>

#define NETWORK_INPUT_BUFFER_SIZE 1024
#define NETWORK_LOBBY_NAME_BUFFER_SIZE 40
#define NETWORK_IP_BUFFER_SIZE 32
#define NETWORK_PLAYER_NAME_BUFFER_SIZE 36
#define NETWORK_APP_VERSION_BUFFER_SIZE 16
#define NETWORK_CHAT_BUFFER_SIZE 128
#define NETWORK_SCANNER_PORT 6529
#define NETWORK_BASE_PORT 6530

#define NETWORK_STEAM_LOBBY_PROPERTY_NAME "name"
#define NETWORK_STEAM_LOBBY_PROPERTY_HOST_IDENTITY "host_identity"
#define NETWORK_STEAM_LOBBY_PROPERTY_PLAYER_COUNT "player_count"

enum NetworkBackend {
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

struct NetworkConnectionInfoLan {
    char ip[NETWORK_IP_BUFFER_SIZE];
    uint16_t port;
};

struct NetworkConnectionInfoSteam {
    char identity_str[SteamNetworkingIdentity::k_cchMaxString];
};

union NetworkConnectionInfo {
    NetworkConnectionInfoLan lan;
    NetworkConnectionInfoSteam steam;
};

struct NetworkLobby {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint8_t player_count;
    NetworkConnectionInfo connection_info;
};

struct NetworkLanLobbyInfo {
    char name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint16_t port;
    uint8_t player_count;
    uint8_t padding; 
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

enum NetworkEventType {
    NETWORK_EVENT_LOBBY_CONNECTION_FAILED,
    NETWORK_EVENT_LOBBY_INVALID_VERSION,
    NETWORK_EVENT_LOBBY_CONNECTED,
    NETWORK_EVENT_PLAYER_DISCONNECTED,
    NETWORK_EVENT_PLAYER_CONNECTED,
    NETWORK_EVENT_CHAT,
    NETWORK_EVENT_MATCH_LOAD,
    NETWORK_EVENT_INPUT,
    NETWORK_EVENT_STEAM_INVITE
};

struct NetworkEventPlayerDisconnected {
    uint8_t player_id;
};

struct NetworkEventPlayerConnected {
    uint8_t player_id;
};

struct NetworkEventChat {
    char message[NETWORK_CHAT_BUFFER_SIZE];
    uint8_t player_id;
};

struct NetworkEventMatchLoad {
    int32_t lcg_seed;
    Noise noise;
};

struct NetworkEventInput {
    uint8_t in_buffer[NETWORK_INPUT_BUFFER_SIZE];
    uint8_t in_buffer_length;
    uint8_t player_id;
};

struct NetworkEventSteamInvite {
    NetworkConnectionInfo connection_info;
};

struct NetworkEvent {
    NetworkEventType type;
    union {
        NetworkEventPlayerDisconnected player_disconnected;
        NetworkEventPlayerConnected player_connected;
        NetworkEventChat chat;
        NetworkEventMatchLoad match_load;
        NetworkEventInput input;
        NetworkEventSteamInvite steam_invite;
    };
};

enum NetworkMessageType {
    NETWORK_MESSAGE_GREET_SERVER,
    NETWORK_MESSAGE_INVALID_VERSION,
    NETWORK_MESSAGE_GAME_ALREADY_STARTED,
    NETWORK_MESSAGE_WELCOME,
    NETWORK_MESSAGE_NEW_PLAYER,
    NETWORK_MESSAGE_GREET_CLIENT,
    NETWORK_MESSAGE_SET_READY,
    NETWORK_MESSAGE_SET_NOT_READY,
    NETWORK_MESSAGE_SET_COLOR,
    NETWORK_MESSAGE_SET_MATCH_SETTING,
    NETWORK_MESSAGE_SET_TEAM,
    NETWORK_MESSAGE_CHAT,
    NETWORK_MESSAGE_LOAD_MATCH,
    NETWORK_MESSAGE_INPUT
};

struct NetworkMessageGreetServer {
    const uint8_t type = NETWORK_MESSAGE_GREET_SERVER;
    char username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char app_version[NETWORK_APP_VERSION_BUFFER_SIZE];
};

struct NetworkMessageWelcome {
    const uint8_t type = NETWORK_MESSAGE_WELCOME;
    uint8_t incoming_player_id;
    uint8_t incoming_recolor_id;
    uint8_t incoming_team;
    uint8_t server_recolor_id;
    uint8_t server_team;
    char server_username[NETWORK_PLAYER_NAME_BUFFER_SIZE];
    char lobby_name[NETWORK_LOBBY_NAME_BUFFER_SIZE];
    uint8_t match_settings[MATCH_SETTING_COUNT];
};

struct NetworkMessageGreetClient {
    const uint8_t type = NETWORK_MESSAGE_GREET_CLIENT;
    uint8_t player_id;
    uint8_t padding[2];
    NetworkPlayer player;
};

struct NetworkMessageSetColor {
    const uint8_t type = NETWORK_MESSAGE_SET_COLOR;
    uint8_t recolor_id;
};

struct NetworkMessageSetTeam {
    const uint8_t type = NETWORK_MESSAGE_SET_TEAM;
    uint8_t team;
};

struct NetworkMessageChat {
    const uint8_t type = NETWORK_MESSAGE_CHAT;
    char message[NETWORK_CHAT_BUFFER_SIZE];
};

struct NetworkMessageSetMatchSetting {
    const uint8_t type = NETWORK_MESSAGE_SET_MATCH_SETTING;
    uint8_t setting;
    uint8_t value;
};
