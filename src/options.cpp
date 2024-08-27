#include "options.h"

#include "input.h"
#include "logger.h"

std::unordered_map<Option, int> options;
std::unordered_map<Option, option_data_t> OPTION_DATA = {
    { OPTION_DISPLAY, (option_data_t) {
        .max_value = DISPLAY_COUNT,
        .default_value = DISPLAY_WINDOWED,
        .confirm_required = true
    }},
    { OPTION_VSYNC, (option_data_t) {
        .max_value = VSYNC_COUNT,
        .default_value = VSYNC_ENABLED,
        .confirm_required = true
    }}
};

void options_init() {
    options = options_create_from_defaults();
}

std::unordered_map<Option, int> options_create_from_defaults() {
    std::unordered_map<Option, int> result;
    for (int i = 0; i < OPTION_COUNT; i++) {
        result[(Option)i] = OPTION_DATA.at((Option)i).default_value;
    }
    return result;
}

option_menu_state_t option_menu_init() {
    option_menu_state_t state;
    state.mode = OPTION_MENU_OPEN;
    state.pending_changes = options;
    state.requested_changes = 0;
    return state;
}

void option_menu_update(option_menu_state_t& state) {
    state.hover = OPTION_HOVER_NONE;
    if (state.mode == OPTION_MENU_OPEN) {
        if (BACK_BUTTON_RECT.has_point(input_get_mouse_position())) {
            state.hover = OPTION_HOVER_BACK;
        } else if (APPLY_BUTTON_RECT.has_point(input_get_mouse_position())) {
            state.hover = OPTION_HOVER_APPLY;
        } else {
            for (int i = 0; i < OPTION_COUNT; i++) {
                rect_t dropdown_rect = rect_t(OPTIONS_DROPDOWN_POSITION + xy(0, i * (OPTIONS_DROPDOWN_SIZE.y + OPTIONS_DROPDOWN_PADDING)), OPTIONS_DROPDOWN_SIZE);
                if (dropdown_rect.has_point(input_get_mouse_position())) {
                    state.hover = OPTION_HOVER_DROPDOWN;
                    state.hover_subindex = i;
                    break;
                }
            }
        }
    } else if (state.mode == OPTION_MENU_DROPDOWN) {
        for (int i = 0; i < OPTION_DATA.at(state.dropdown_chosen).max_value; i++) {
            rect_t dropdown_item_rect = rect_t(
                OPTIONS_DROPDOWN_POSITION + 
                xy(0, (int)state.dropdown_chosen * (OPTIONS_DROPDOWN_SIZE.y + OPTIONS_DROPDOWN_PADDING)) +
                xy(0, (i + 1) * OPTIONS_DROPDOWN_SIZE.y), 
                OPTIONS_DROPDOWN_SIZE);
            if (dropdown_item_rect.has_point(input_get_mouse_position())) {
                state.hover = OPTION_HOVER_DROPDOWN_ITEM;
                state.hover_subindex = i;
            }
        }
    }

    if (input_is_mouse_button_just_pressed(MOUSE_BUTTON_LEFT)) {
        if (state.hover == OPTION_HOVER_BACK) {
            state.mode = OPTION_MENU_CLOSED;
        } else if (state.hover == OPTION_HOVER_APPLY) {
            state.requested_changes = 0;
            for (auto it : state.pending_changes) {
                if (options[it.first] != it.second) {
                    if (it.first == OPTION_VSYNC) {
                        state.mode = OPTION_MENU_REQUEST_CHANGES;
                        state.requested_changes |= REQUEST_REINIT_RENDERER;
                    } else if (it.first == OPTION_DISPLAY) {
                        state.mode = OPTION_MENU_REQUEST_CHANGES;
                        state.requested_changes |= REQUEST_UPDATE_FULLSCREEN;
                    }
                }
            }
            options = state.pending_changes;
        } else if (state.hover == OPTION_HOVER_DROPDOWN) {
            state.mode = OPTION_MENU_DROPDOWN;
            state.hover = OPTION_HOVER_NONE;
            state.dropdown_chosen = (Option)state.hover_subindex;
        } else if (state.hover == OPTION_HOVER_DROPDOWN_ITEM) {
            state.pending_changes[state.dropdown_chosen] = state.hover_subindex;
            state.mode = OPTION_MENU_OPEN;
            state.hover = OPTION_HOVER_NONE;
        }
    }
}