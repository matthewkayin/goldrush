#pragma once

#include "core/animation.h"
#include "core/network.h"
#include "resource/sprite.h"
#include <vector>
#include <string>

#define MENU_CHAT_MAX_LINE_LENGTH 64
#define MENU_CHAT_MAX_LINE_COUNT 12
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

enum MenuItemType {
    MENU_ITEM_TYPE_BUTTON,
    MENU_ITEM_TYPE_TEXTBOX,
    MENU_ITEM_TYPE_MATCHLIST_LOBBY,
    MENU_ITEM_TYPE_SPRITE_BUTTON
};

enum MenuItemName {
    MENU_ITEM_MAIN_BUTTON_PLAY,
    MENU_ITEM_MAIN_BUTTON_EXIT,
    MENU_ITEM_USERNAME_TEXTBOX,
    MENU_ITEM_USERNAME_BUTTON_BACK,
    MENU_ITEM_USERNAME_BUTTON_OK,
    MENU_ITEM_MATCHLIST_LOBBY,
    MENU_ITEM_MATCHLIST_BUTTON_BACK,
    MENU_ITEM_MATCHLIST_BUTTON_HOST,
    MENU_ITEM_MATCHLIST_BUTTON_JOIN,
    MENU_ITEM_MATCHLIST_SEARCH,
    MENU_ITEM_MATCHLIST_REFRESH,
    MENU_ITEM_MATCHLIST_PAGE_LEFT,
    MENU_ITEM_MATCHLIST_PAGE_RIGHT,
    MENU_ITEM_LOBBY_CHAT_TEXTBOX,
    MENU_ITEM_COUNT
};

struct MenuItemButton {
    const char* text;
};

struct MenuItemTextbox {
    const char* prompt;
    std::string* value;
    size_t max_length;
};

struct MenuItemSprite {
    SpriteName sprite;
    bool disabled;
    bool flip_h;
};

struct MenuItem {
    MenuItemType type;
    MenuItemName name;
    Rect rect;
    union {
        MenuItemButton button;
        MenuItemTextbox textbox;
        NetworkLobby lobby;
        MenuItemSprite sprite;
    };
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
    bool text_input_show_cursor;
    uint32_t text_input_cursor_blink_timer;

    uint32_t status_timer;
    char status_text[128];

    std::vector<MenuItem> menu_items;
    std::vector<std::string> chat;
    int item_selected;
    uint32_t matchlist_page;
};

MenuState menu_init();
void menu_handle_network_event(MenuState& state, NetworkEvent event);
void menu_update(MenuState& state);
void menu_render(const MenuState& state);