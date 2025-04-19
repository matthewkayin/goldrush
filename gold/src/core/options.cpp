#include "options.h"

#include "logger.h"
#include "sound.h"
#include "input.h"
#include "render/render.h"
#include <SDL2/SDL_mixer.h>
#include <unordered_map>
#include <fstream>
#include <string>

static const char* OPTIONS_FILE_PATH = "./options.ini";

static const std::unordered_map<OptionName, OptionData> OPTION_DATA = {
    { OPTION_DISPLAY, (OptionData) {
        .name = "Display",
        .type = OPTION_TYPE_DROPDOWN,
        .min_value = 0,
        .max_value = RENDER_DISPLAY_COUNT,
        .default_value = RENDER_DISPLAY_BORDERLESS
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
        .max_value = MIX_MAX_VOLUME,
        .default_value = MIX_MAX_VOLUME
    }},
    { OPTION_MUS_VOLUME, (OptionData) {
        .name = "Music Volume",
        .type = OPTION_TYPE_PERCENT_SLIDER,
        .min_value = 0,
        .max_value = MIX_MAX_VOLUME,
        .default_value = MIX_MAX_VOLUME
    }},
    { OPTION_UNIT_VOICES, (OptionData) {
        .name = "Unit Voices",
        .type = OPTION_TYPE_DROPDOWN,
        .min_value = 0,
        .max_value = OPTION_UNIT_VOICES_COUNT,
        .default_value = OPTION_UNIT_VOICES_ON
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

    std::ifstream options_file(OPTIONS_FILE_PATH);
    if (!options_file.is_open()) {
        log_info("No options file found.");
        return;
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
        } 
    }

    for (int option = 0; option < OPTION_COUNT; option++) {
        option_apply((OptionName)option);
    }
}

void options_save() {
    FILE* options_file = fopen(OPTIONS_FILE_PATH, "w");
    if (options_file == NULL) {
        log_error("Could not open options file for writing.");
        return;
    }

    for (int option = 0; option < OPTION_COUNT; option++) {
        fprintf(options_file, "%s=%i\n", OPTION_DATA.at((OptionName)option).name, options[(OptionName)option]);
    }

    fprintf(options_file, "[hotkeys]\n");

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
            input_update_window_size();
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