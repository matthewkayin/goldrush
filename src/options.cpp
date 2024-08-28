#include "options.h"

#include "input.h"
#include "logger.h"
#include <cstdio>

static const char* OPTIONS_FILE_VERSION = "1";

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
    bool success = options_load_from_file();
    if (!success) {
        options = options_create_from_defaults();
    }
}

std::unordered_map<Option, int> options_create_from_defaults() {
    std::unordered_map<Option, int> result;
    for (int i = 0; i < OPTION_COUNT; i++) {
        result[(Option)i] = OPTION_DATA.at((Option)i).default_value;
    }
    return result;
}

void options_save_to_file() {
    FILE* options_file = fopen("./options.ini", "w");
    if (options_file == NULL) {
        log_error("Unable to open options file for writing");
        return;
    }

    fprintf(options_file, "version=%s\n", OPTIONS_FILE_VERSION);
    for (auto it : options) {
        fprintf(options_file, "%s=%s\n", option_string((Option)it.first), option_value_string((Option)it.first, it.second));
    }

    fclose(options_file);
    log_info("Options file saved");
}

bool options_load_from_file() {
    FILE* options_file = fopen("./options.ini", "r");
    if (options_file == NULL) {
        return false;
    }

    char key[128];
    uint32_t key_size = 0;
    char value[128];
    uint32_t value_size = 0;
    bool found_equals = false;
    char c;
    while ((c = fgetc(options_file)) != EOF) {
        if (c == '\n') {
            if (key_size == 0 || value_size == 0 || !found_equals) {
                continue;
            }

            key[key_size] = '\0';
            key_size++;
            value[value_size] = '\0';
            value_size++;
            log_trace("Option line read. key %s value %s", key, value);

            if (strcmp(key, "version") == 0) {
                if (strcmp(value, OPTIONS_FILE_VERSION) != 0) {
                    log_error("Unexpected options file version %s.");
                    fclose(options_file);
                    return false;
                }

                log_trace("Detected options file version %s", value);
                key_size = 0;
                value_size = 0;
                found_equals = false;
                continue;
            }

            int option;
            for (option = 0; option < OPTION_COUNT; option++) {
                if (strcmp(key, option_string((Option)option)) == 0) {
                    break;
                }
            }
            if (option == OPTION_COUNT) {
                log_error("Unrecognized option %s.", key);
                fclose(options_file);
                return false;
            }

            int option_value;
            for (option_value = 0; option_value < OPTION_DATA.at((Option)option).max_value; option_value++) {
                if (strcmp(value, option_value_string((Option)option, option_value)) == 0) {
                    break;
                }
            }
            if (option_value == OPTION_DATA.at((Option)option).max_value) {
                log_error("Unrecognized value %s for option %s.", value, key);
                fclose(options_file);
                return false;
            }

            options[(Option)option] = option_value;
            key_size = 0;
            value_size = 0;
            found_equals = false;
        } else if (c == '=') {
            found_equals = true;
        } else {
            if (!found_equals) {
                key[key_size] = c;
                key_size++;
            } else {
                value[value_size] = c;
                value_size++;
            }
            if (key_size == 127 || value_size == 127) {
                log_error("Option data too long for key or value buffer.");
                fclose(options_file);
                return false;
            }
        }
    }

    fclose(options_file);

    for (int i = 0; i < OPTION_COUNT; i++) {
        auto it = options.find((Option)i);
        if (it == options.end()) {
            log_warn("Option %s is not defined in log file. Filling with default.", option_string((Option)i));
            options[(Option)i] = OPTION_DATA.at((Option)i).default_value;
        }
    }

    return true;
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