#pragma once

#include "math/gmath.h"
#include <SDL3/SDL.h>
#include <string>

#define INPUT_KEY_NONE SDL_SCANCODE_UNKNOWN

enum InputAction {
    INPUT_ACTION_LEFT_CLICK,
    INPUT_ACTION_RIGHT_CLICK,
    INPUT_ACTION_SHIFT,
    INPUT_ACTION_CTRL,
    INPUT_ACTION_SPACE,
    INPUT_ACTION_F1,
    INPUT_ACTION_F2,
    INPUT_ACTION_F3,
    INPUT_ACTION_F4,
    INPUT_ACTION_F5,
    INPUT_ACTION_F6,
    INPUT_ACTION_F7,
    INPUT_ACTION_F8,
    #ifdef GOLD_DEBUG
    INPUT_ACTION_TURBO,
    #endif
    INPUT_ACTION_MATCH_MENU,
    INPUT_ACTION_ENTER,
    INPUT_ACTION_NUM1,
    INPUT_ACTION_NUM2,
    INPUT_ACTION_NUM3,
    INPUT_ACTION_NUM4,
    INPUT_ACTION_NUM5,
    INPUT_ACTION_NUM6,
    INPUT_ACTION_NUM7,
    INPUT_ACTION_NUM8,
    INPUT_ACTION_NUM9,
    INPUT_ACTION_NUM0,
#ifdef GOLD_DEBUG
    INPUT_ACTION_EDITOR_SAVE,
    INPUT_ACTION_EDITOR_UNDO,
    INPUT_ACTION_EDITOR_REDO,
    INPUT_ACTION_EDITOR_COPY,
    INPUT_ACTION_EDITOR_CUT,
    INPUT_ACTION_EDITOR_PASTE,
    INPUT_ACTION_EDITOR_TOOL_BRUSH,
    INPUT_ACTION_EDITOR_TOOL_FILL,
    INPUT_ACTION_EDITOR_TOOL_RECT,
    INPUT_ACTION_EDITOR_TOOL_SELECT,
    INPUT_ACTION_EDITOR_TOOL_DECORATE,
    INPUT_ACTION_EDITOR_TOOL_ADD_ENTITY,
    INPUT_ACTION_EDITOR_TOOL_EDIT_ENTITY,
    INPUT_ACTION_EDITOR_DELETE,
    INPUT_ACTION_EDITOR_TOOL_SQUADS,
#endif
    INPUT_HOTKEY_NONE,
    INPUT_HOTKEY_ATTACK,
    INPUT_HOTKEY_STOP,
    INPUT_HOTKEY_DEFEND,
    INPUT_HOTKEY_BUILD,
    INPUT_HOTKEY_BUILD2,
    INPUT_HOTKEY_REPAIR,
    INPUT_HOTKEY_UNLOAD,
    INPUT_HOTKEY_CANCEL,
    INPUT_HOTKEY_HALL,
    INPUT_HOTKEY_HOUSE,
    INPUT_HOTKEY_SALOON,
    INPUT_HOTKEY_WORKSHOP,
    INPUT_HOTKEY_SMITH,
    INPUT_HOTKEY_COOP,
    INPUT_HOTKEY_BARRACKS,
    INPUT_HOTKEY_SHERIFFS,
    INPUT_HOTKEY_BUNKER,
    INPUT_HOTKEY_MINER,
    INPUT_HOTKEY_COWBOY,
    INPUT_HOTKEY_BANDIT,
    INPUT_HOTKEY_SAPPER,
    INPUT_HOTKEY_PYRO,
    INPUT_HOTKEY_WAGON,
    INPUT_HOTKEY_JOCKEY,
    INPUT_HOTKEY_SOLDIER,
    INPUT_HOTKEY_CANNON,
    INPUT_HOTKEY_DETECTIVE,
    INPUT_HOTKEY_BALLOON,
    INPUT_HOTKEY_MOLOTOV,
    INPUT_HOTKEY_CAMO,
    INPUT_HOTKEY_LANDMINE,
    INPUT_HOTKEY_RESEARCH_LANDMINES,
    INPUT_HOTKEY_RESEARCH_WAGON_ARMOR,
    INPUT_HOTKEY_RESEARCH_BAYONETS,
    INPUT_HOTKEY_RESEARCH_PRIVATE_EYE,
    INPUT_HOTKEY_RESEARCH_STAKEOUT,
    INPUT_HOTKEY_RESEARCH_SERRATED_KNIVES,
    INPUT_HOTKEY_RESEARCH_TAILWIND,
    INPUT_HOTKEY_IDLE_MINER,
    INPUT_ACTION_COUNT
};

void input_init(SDL_Window* window);
void input_set_mouse_capture_enabled(bool value);
void input_update_screen_scale();
void input_poll_events();

void input_start_text_input(std::string* str, size_t max_length);
void input_stop_text_input();
bool input_is_text_input_active();

ivec2 input_get_mouse_position();
bool input_user_requests_exit();

int input_get_mouse_scroll();

bool input_is_action_pressed(InputAction action);
bool input_is_action_just_pressed(InputAction action);
bool input_is_action_just_released(InputAction action);

int input_sprintf_sdl_scancode_str(char* str_ptr, SDL_Scancode scancode);
SDL_Scancode input_get_hotkey_mapping(InputAction hotkey);
void input_set_hotkey_mapping(InputAction hotkey, SDL_Scancode key);
void input_set_hotkey_mapping_to_default(SDL_Scancode* hotkey_mapping);

SDL_Scancode input_get_key_just_pressed();
bool input_is_key_valid_hotkey_mapping(SDL_Scancode key);