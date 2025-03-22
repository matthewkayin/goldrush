#pragma once

#include "core/animation.h"
#include "core/network.h"
#include "render/sprite.h"
#include "render/font.h"
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
    MENU_ITEM_BUTTON,
    MENU_ITEM_TEXTBOX,
    MENU_ITEM_LOBBY,
    MENU_ITEM_SPRITE_BUTTON,
    MENU_ITEM_TEXT,
    MENU_ITEM_FRAME,
    MENU_ITEM_DROPDOWN,
    MENU_ITEM_DROPDOWN_ITEM,
    MENU_ITEM_TEAM_PICKER
};

enum MenuButtonName {
    MENU_BUTTON_MAIN_PLAY,
    MENU_BUTTON_MAIN_EXIT,
    MENU_BUTTON_USERNAME_BACK,
    MENU_BUTTON_USERNAME_OK,
    MENU_BUTTON_MATCHLIST_BACK,
    MENU_BUTTON_MATCHLIST_HOST,
    MENU_BUTTON_MATCHLIST_JOIN,
    MENU_BUTTON_MATCHLIST_REFRESH,
    MENU_BUTTON_MATCHLIST_PAGE_LEFT,
    MENU_BUTTON_MATCHLIST_PAGE_RIGHT,
    MENU_BUTTON_LOBBY_BACK,
    MENU_BUTTON_LOBBY_START,
    MENU_BUTTON_LOBBY_READY
};

struct MenuItemButton {
    MenuButtonName name;
    const char* text;
};

struct MenuItemTextbox {
    const char* prompt;
    std::string* value;
    size_t max_length;
};

struct MenuItemSprite {
    MenuButtonName name;
    SpriteName sprite;
    bool disabled;
    bool flip_h;
};

struct MenuItemLobby {
    size_t index;
};

struct MenuItemText {
    FontName font;
    const char* text;
};

struct MenuItemTeamPicker {
    char value;
    bool disabled;
};

enum MenuDropdownName {
    MENU_DROPDOWN_PLAYER_COLOR
};

struct MenuItemDropdown {
    MenuDropdownName name;
    int value;
    bool disabled;
};

struct MenuItemDropdownItem {
    MenuDropdownName name;
    int value;
};

struct MenuItem {
    MenuItemType type;
    Rect rect;
    union {
        MenuItemButton button;
        MenuItemTextbox textbox;
        MenuItemLobby lobby;
        MenuItemSprite sprite;
        MenuItemText text;
        MenuItemTeamPicker team;
        MenuItemDropdown dropdown;
        MenuItemDropdownItem dropdown_item;
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
    uint32_t connection_timeout;

    std::vector<MenuItem> menu_items;
    std::vector<std::string> chat;
    int item_selected;
    uint32_t matchlist_page;
};

MenuState menu_init();
void menu_handle_network_event(MenuState& state, NetworkEvent event);
void menu_update(MenuState& state);
void menu_render(const MenuState& state);