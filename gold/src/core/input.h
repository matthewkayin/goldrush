#pragma once

#include "math/gmath.h"
#include <SDL2/SDL.h>
#include <string>

enum InputAction {
    INPUT_ACTION_LEFT_CLICK,
    INPUT_ACTION_RIGHT_CLICK,
    INPUT_ACTION_SHIFT,
    INPUT_ACTION_CTRL,
    INPUT_ACTION_SPACE,
    INPUT_ACTION_F3,
    INPUT_ACTION_ENTER,
    INPUT_ACTION_NUM0,
    INPUT_ACTION_NUM1,
    INPUT_ACTION_NUM2,
    INPUT_ACTION_NUM3,
    INPUT_ACTION_NUM4,
    INPUT_ACTION_NUM5,
    INPUT_ACTION_NUM6,
    INPUT_ACTION_NUM7,
    INPUT_ACTION_NUM8,
    INPUT_ACTION_NUM9,
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
    INPUT_HOTKEY_BUNKER,
    INPUT_HOTKEY_MINER,
    INPUT_HOTKEY_COWBOY,
    INPUT_HOTKEY_BANDIT,
    INPUT_HOTKEY_SAPPER,
    INPUT_HOTKEY_PYRO,
    INPUT_HOTKEY_MOLOTOV,
    INPUT_HOTKEY_LANDMINE,
    INPUT_HOTKEY_RESEARCH_LANDMINES,
    INPUT_ACTION_COUNT
};

const uint32_t INPUT_HOTKEY_GROUP_EMPTY = 0;
const uint32_t INPUT_HOTKEY_GROUP_UNIT = 1;
const uint32_t INPUT_HOTKEY_GROUP_MINER = 2;
const uint32_t INPUT_HOTKEY_GROUP_UNLOAD = 2 << 1;
const uint32_t INPUT_HOTKEY_GROUP_CANCEL = 2 << 2;
const uint32_t INPUT_HOTKEY_GROUP_EXPLODE = 2 << 3;
const uint32_t INPUT_HOTKEY_GROUP_PYRO = 2 << 4;
const uint32_t INPUT_HOTKEY_GROUP_HALL = 2 << 5;
const uint32_t INPUT_HOTKEY_GROUP_SALOON = 2 << 6;
const uint32_t INPUT_HOTKEY_GROUP_BARRACKS = 2 << 7;
const uint32_t INPUT_HOTKEY_GROUP_SMITH = 2 << 8;
const uint32_t INPUT_HOTKEY_GROUP_BUILD = 2 << 9;
const uint32_t INPUT_HOTKEY_GROUP_BUILD2 = 2 << 10;
const uint32_t INPUT_HOTKEY_GROUP_SHERIFFS = 2 << 11;
const uint32_t INPUT_HOTKEY_GROUP_COOP = 2 << 12;
const uint32_t INPUT_HOTKEY_GROUP_WORKSHOP = 2 << 13;
const uint32_t INPUT_HOTKEY_GROUP_RESEARCH_LANDMINES = 2 << 14;

void input_init(SDL_Window* window);
void input_poll_events();

void input_start_text_input(std::string* str, size_t max_length);
void input_stop_text_input();
bool input_is_text_input_active();

ivec2 input_get_mouse_position();
bool input_user_requests_exit();
bool input_is_action_pressed(InputAction action);
bool input_is_action_just_pressed(InputAction action);
bool input_is_action_just_released(InputAction action);

void input_set_hotkey_group(uint32_t group);
InputAction input_get_hotkey(uint32_t index);
int input_sprintf_hotkey_str(char* str_ptr, InputAction hotkey);
void input_use_hotkey_mapping_default();