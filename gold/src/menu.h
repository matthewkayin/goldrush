#pragma once

#include "defines.h"
#include "engine.h"
#include "network.h"
#include "animation.h"
#include "options.h"

#include <string>

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_USERNAME,
    MENU_MODE_MATCHLIST,
    MENU_MODE_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_EXIT,
    MENU_MODE_OPTIONS,
    MENU_MODE_LOAD_MATCH
};

enum MenuButton {
    MENU_BUTTON_NONE,
    MENU_BUTTON_PLAY,
    MENU_BUTTON_REPLAYS,
    MENU_BUTTON_OPTIONS,
    MENU_BUTTON_EXIT,
    MENU_BUTTON_HOST,
    MENU_BUTTON_JOIN,
    MENU_BUTTON_MATCHLIST_BACK,
    MENU_BUTTON_USERNAME_BACK,
    MENU_BUTTON_USERNAME_OK,
    MENU_BUTTON_LOBBY_BACK,
    MENU_BUTTON_LOBBY_READY,
    MENU_BUTTON_LOBBY_START,
    MENU_BUTTON_CONNECTING_BACK
};

enum MenuHoverType {
    MENU_HOVER_NONE,
    MENU_HOVER_BUTTON,
    MENU_HOVER_ITEM,
    MENU_HOVER_REFRESH,
    MENU_HOVER_PREVIOUS,
    MENU_HOVER_NEXT,
    MENU_HOVER_DROPDOWN,
    MENU_HOVER_DROPDOWN_ITEM
};

enum MenuHoverDropdown {
    MENU_DROPDOWN_MAP_SIZE,
    MENU_DROPDOWN_TEAMS,
    MENU_DROPDOWN_COLOR
};

struct menu_hover_t {
    MenuHoverType type;
    union {
        MenuButton button;
        int item;
    };
};

struct menu_state_t {
    MenuMode mode;
    struct menu_hover_t hover;

    std::string status_text;
    uint32_t status_timer;
    uint32_t text_input_blink_timer;
    bool text_input_blink;

    std::string text_input;
    std::string username;
    std::vector<std::string> chat;
    int item_selected;
    int dropdown_open;
    int matchlist_page;

    animation_t wagon_animation;
    int wagon_x;
    int parallax_x;
    int parallax_cloud_x;
    int parallax_timer;
    int parallax_cactus_offset;

    options_menu_state_t options_state;
};

menu_state_t menu_init();
void menu_handle_input(menu_state_t& state, SDL_Event event);
void menu_handle_network_event(menu_state_t& state, const network_event_t& network_event);
void menu_update(menu_state_t& state);
void menu_show_status(menu_state_t& state, const char* text);
void menu_set_mode(menu_state_t& state, MenuMode mode);
size_t menu_get_text_input_max(const menu_state_t& state);
bool menu_is_button_disabled(const menu_state_t& state, MenuButton button);
void menu_render(const menu_state_t& state);
SDL_Rect menu_get_lobby_text_frame_rect(int lobby_index);
void menu_render_lobby_text(const menu_state_t& state, int lobby_index);
SDL_Rect menu_get_dropdown_rect(int index);

SDL_Rect menu_get_match_setting_rect(int index);
const char* menu_match_setting_value_str(MatchSetting setting, uint32_t value);
size_t menu_get_matchlist_page_count();
void menu_add_chat_message(menu_state_t& state, std::string message);