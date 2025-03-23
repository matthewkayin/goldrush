#pragma once

#include "core/animation.h"
#include "core/network.h"
#include <vector>
#include <string>

#define MENU_ITEM_NONE -1
#define MENU_CHAT_MAX_LINE_LENGTH 64
#define MENU_CHAT_MAX_LINE_COUNT 11
#define MENU_CHAT_MESSAGE_BUFFER_SIZE 60
#define MENU_CHAT_MAX_MESSAGE_LENGTH (MENU_CHAT_MESSAGE_BUFFER_SIZE - 1)

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_USERNAME,
    MENU_MODE_MATCHLIST,
    MENU_MODE_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_EXIT,
    MENU_MODE_OPTIONS,
    MENU_MODE_LOAD_MATCH,
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

    std::string username;
    std::string lobby_search_query;
    std::string chat_message;
    uint32_t status_timer;
    char status_text[128];
    uint32_t connection_timeout;
    uint32_t matchlist_page;
    uint32_t matchlist_item_selected;

    std::vector<std::string> chat;
};

MenuState menu_init();
void menu_handle_network_event(MenuState& state, NetworkEvent event);
void menu_update(MenuState& state);
void menu_render(const MenuState& state);