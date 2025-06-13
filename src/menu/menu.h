#pragma once

#include "network/network.h"
#include "core/animation.h"
#include "match_setting.h"
#include "options.h"
#include <vector>
#include <string>

#define MENU_ITEM_NONE -1
#define MENU_CHAT_MAX_LINE_LENGTH 64
#define MENU_CHAT_MAX_LINE_COUNT 11
#define MENU_CHAT_MESSAGE_BUFFER_SIZE 60
#define MENU_CHAT_MAX_MESSAGE_LENGTH (MENU_CHAT_MESSAGE_BUFFER_SIZE - 1)

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_SINGLEPLAYER,
    MENU_MODE_MULTIPLAYER,
    MENU_MODE_CREATE_LOBBY,
    MENU_MODE_LOBBYLIST,
    MENU_MODE_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_REPLAYS,
    MENU_MODE_REPLAY_RENAME,
    MENU_MODE_EXIT,
    MENU_MODE_OPTIONS,
    MENU_MODE_LOAD_MATCH,
    MENU_MODE_LOAD_REPLAY,
    MENU_MODE_COUNT
};

struct MenuState {
    MenuMode mode;

    Animation wagon_animation;
    int wagon_x;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;

    std::string lobby_name;
    std::string lobby_search_query;
    std::string chat_message;
    std::string replay_rename;
    uint32_t status_timer;
    char status_text[128];
    uint32_t connection_timeout;
    uint32_t lobbylist_page;
    uint32_t lobbylist_item_selected;
    uint32_t lobby_privacy;

    std::vector<std::string> chat;
    std::vector<std::string> replay_filenames;

    OptionsMenuState options_menu;
};

MenuState menu_init();
void menu_handle_network_event(MenuState& state, NetworkEvent event);
void menu_update(MenuState& state);

void menu_set_mode(MenuState& state, MenuMode mode);
void menu_show_status(MenuState& state, const char* message);
void menu_add_chat_message(MenuState& state, const char* message);
size_t menu_get_lobbylist_page_count(size_t count);
const char* menu_get_player_status_string(NetworkPlayerStatus status);

void menu_render(const MenuState& state);
void menu_render_decoration(const MenuState& state, int index);