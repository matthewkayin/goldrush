#include "options.h"

#include "logger.h"
#include "sound.h"
#include "input.h"
#include "match/hotkey.h"
#include "core/filesystem.h"
#include "render/render.h"
#include <unordered_map>
#include <fstream>
#include <string>

static const char* OPTIONS_FILE_NAME = "options.ini";

static const std::unordered_map<OptionName, OptionData> OPTION_DATA = {
    { OPTION_DISPLAY, (OptionData) {
        .name = "Display",
        .type = OPTION_TYPE_DROPDOWN,
        .min_value = 0,
        .max_value = RENDER_DISPLAY_COUNT,
        .default_value = RENDER_DISPLAY_FULLSCREEN
    }},
    { OPTION_VSYNC, (OptionData) {
        .name = "Vsync",
        .type = OPTION_TYPE_DROPDOWN,
        .min_value = 0,
        .max_value = RENDER_VSYNC_COUNT,
        .default_value = RENDER_VSYNC_ENABLED
    }},
    { OPTION_SFX_VOLUME, (OptionData) {
        .name = "Sound Volume",
        .type = OPTION_TYPE_PERCENT_SLIDER,
        .min_value = 0,
        .max_value = 100,
        .default_value = 100
    }},
    { OPTION_MUS_VOLUME, (OptionData) {
        .name = "Music Volume",
        .type = OPTION_TYPE_PERCENT_SLIDER,
        .min_value = 0,
        .max_value = 100,
        .default_value = 100
    }},
    { OPTION_CAMERA_SPEED, (OptionData) {
        .name = "Camera Speed",
        .type = OPTION_TYPE_SLIDER,
        .min_value = 1,
        .max_value = 31,
        .default_value = 16
    }}
};

const OptionData& option_get_data(OptionName name) {
    return OPTION_DATA.at(name);
}
static std::unordered_map<OptionName, int> options;

void options_load() {
    for (int option = 0; option < OPTION_COUNT; option++) {
        options[(OptionName)option] = OPTION_DATA.at((OptionName)option).default_value;
    }

    char options_file_path[256];
    filesystem_get_data_path(options_file_path, OPTIONS_FILE_NAME);
    std::ifstream options_file(options_file_path);
    if (options_file.is_open()) {
        std::string hotkey_names[INPUT_ACTION_COUNT];
        char hotkey_name_buffer[128];
        for (int hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
            hotkey_get_name(hotkey_name_buffer, (InputAction)hotkey);
            hotkey_names[hotkey] = std::string(hotkey_name_buffer);
        }

        std::string line;
        bool options_mode = true;
        while (std::getline(options_file, line)) {
            if (line == "[hotkeys]") {
                options_mode = false;
                continue;
            }

            size_t equals_index = line.find('=');
            std::string key = line.substr(0, equals_index);
            std::string value = line.substr(equals_index + 1);

            if (options_mode) {
                int option;
                for (option = 0; option < OPTION_COUNT; option++) {
                    if (key == std::string(OPTION_DATA.at((OptionName)option).name)) {
                        options[(OptionName)option] = std::stoi(value);
                        break;
                    }
                }
                if (option == OPTION_COUNT) {
                    log_warn("Unrecognized option key %s with value %s", key.c_str(), value.c_str());
                }
            } else {
                int hotkey;
                for (hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
                    if (key == hotkey_names[hotkey]) {
                        input_set_hotkey_mapping((InputAction)hotkey, (SDL_Keycode)std::stoi(value));
                        break;
                    }
                }
                if (hotkey == INPUT_ACTION_COUNT) {
                    log_warn("Unrecognized hotkey mapping key %s with value %s", key.c_str(), value.c_str());
                }
            }
        }
    } else {
        log_info("No options file found.");
    }

    for (int option = 0; option < OPTION_COUNT; option++) {
        option_apply((OptionName)option);
    }
}

void options_save() {
    char options_file_path[256];
    filesystem_get_data_path(options_file_path, OPTIONS_FILE_NAME);
    FILE* options_file = fopen(options_file_path, "w");
    if (options_file == NULL) {
        log_error("Could not open options file for writing.");
        return;
    }

    for (int option = 0; option < OPTION_COUNT; option++) {
        fprintf(options_file, "%s=%i\n", OPTION_DATA.at((OptionName)option).name, options[(OptionName)option]);
    }

    fprintf(options_file, "[hotkeys]\n");

    for (int hotkey = INPUT_HOTKEY_NONE + 1; hotkey < INPUT_ACTION_COUNT; hotkey++) {
        char hotkey_name[128];
        hotkey_get_name(hotkey_name, (InputAction)hotkey);
        fprintf(options_file, "%s=%i\n", hotkey_name, input_get_hotkey_mapping((InputAction)hotkey));
    }

    fclose(options_file);
}

uint32_t option_get_value(OptionName name) {
    return options[name];
}

void option_set_value(OptionName name, uint32_t value) {
    if (options[name] == value) {
        return;
    }

    options[name] = value;
    option_apply(name);
}

void option_apply(OptionName name) {
    switch (name) {
        case OPTION_DISPLAY:
            render_set_display((RenderDisplay)options[name]);
            input_update_screen_scale();
            break;
        case OPTION_VSYNC:
            render_set_vsync((RenderVsync)options[name]);
            break;
        case OPTION_SFX_VOLUME: {
            sound_set_sfx_volume(options[name]);
            break;
        }
        case OPTION_MUS_VOLUME: {
            sound_set_mus_volume(options[name]);
            break;
        }
        default:
            break;
    }
}