#pragma once

#include "defines.h"
#include "util.h"
#include <unordered_map>

const xy OPTIONS_MENU_SIZE = xy(250, 300);
const rect_t OPTIONS_FRAME_RECT = rect_t(xy((SCREEN_WIDTH / 2) - (OPTIONS_MENU_SIZE.x / 2), (SCREEN_HEIGHT / 2) - (OPTIONS_MENU_SIZE.y / 2)), OPTIONS_MENU_SIZE);
const xy BACK_BUTTON_SIZE = xy(63, 21);
const xy APPLY_BUTTON_POSITION = OPTIONS_FRAME_RECT.position + OPTIONS_FRAME_RECT.size - BACK_BUTTON_SIZE - xy(14, 6);
const xy BACK_BUTTON_POSITION = APPLY_BUTTON_POSITION - xy(BACK_BUTTON_SIZE.x + 4, 0);
const rect_t APPLY_BUTTON_RECT = rect_t(APPLY_BUTTON_POSITION, BACK_BUTTON_SIZE);
const rect_t BACK_BUTTON_RECT = rect_t(BACK_BUTTON_POSITION, BACK_BUTTON_SIZE);
const xy OPTIONS_DROPDOWN_SIZE = xy(112, 21);
const xy OPTIONS_DROPDOWN_POSITION = OPTIONS_FRAME_RECT.position + xy(OPTIONS_FRAME_RECT.size.x - OPTIONS_DROPDOWN_SIZE.x - 14, 12);
const int OPTIONS_DROPDOWN_PADDING = 4;

enum Option {
    OPTION_DISPLAY,
    OPTION_VSYNC,
    OPTION_COUNT
};

enum OptionDisplayValue {
    DISPLAY_WINDOWED,
    DISPLAY_FULLSCREEN,
    DISPLAY_BORDERLESS,
    DISPLAY_COUNT
};

enum OptionVsyncValue {
    VSYNC_ENABLED,
    VSYNC_DISABLED,
    VSYNC_COUNT
};

inline const char* option_string(Option option) {
    switch (option) {
        case OPTION_DISPLAY:
            return "Display";
        case OPTION_VSYNC:
            return "VSync";
        case OPTION_COUNT:
            return "";
    }
}

inline const char* option_value_string(Option option, int value) {
    switch (option) {
        case OPTION_DISPLAY: {
            switch ((OptionDisplayValue)value) {
                case DISPLAY_WINDOWED:
                    return "Windowed";
                case DISPLAY_FULLSCREEN:
                    return "Fullscreen";
                case DISPLAY_BORDERLESS:
                    return "Borderless";
                case DISPLAY_COUNT:
                    return "";
            }
        }
        case OPTION_VSYNC: {
            switch ((OptionVsyncValue)value) {
                case VSYNC_ENABLED:
                    return "Enabled";
                case VSYNC_DISABLED:
                    return "Disabled";
                case VSYNC_COUNT:
                    return "";
            }
        }
        case OPTION_COUNT:
            return "";
    }
}

struct option_data_t {
    int max_value;
    int default_value;
    bool confirm_required;
};

enum OptionMenuMode {
    OPTION_MENU_OPEN,
    OPTION_MENU_CLOSED,
    OPTION_MENU_DROPDOWN,
    OPTION_MENU_REQUEST_CHANGES
};

enum OptionMenuHover {
    OPTION_HOVER_NONE,
    OPTION_HOVER_BACK,
    OPTION_HOVER_APPLY,
    OPTION_HOVER_DROPDOWN,
    OPTION_HOVER_DROPDOWN_ITEM
};

const uint32_t REQUEST_REINIT_RENDERER = 1;
const uint32_t REQUEST_UPDATE_FULLSCREEN = 2;

struct option_menu_state_t {
    OptionMenuMode mode;
    OptionMenuHover hover;
    int hover_subindex;
    Option dropdown_chosen;
    std::unordered_map<Option, int> pending_changes;
    uint32_t requested_changes;
};

void options_init();
std::unordered_map<Option, int> options_create_from_defaults();
void options_save_to_file();
bool options_load_from_file();

option_menu_state_t option_menu_init();
void option_menu_update(option_menu_state_t& state);

extern std::unordered_map<Option, option_data_t> OPTION_DATA;
extern std::unordered_map<Option, int> options;