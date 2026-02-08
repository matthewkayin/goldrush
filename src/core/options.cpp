#include "options.h"

#include "logger.h"
#include "sound.h"
#include "input.h"
#include "core/filesystem.h"
#include "render/render.h"
#include "util/json.h"
#include <unordered_map>
#include <fstream>
#include <string>

static const std::unordered_map<OptionName, OptionData> OPTION_DATA = {
    { OPTION_DISPLAY, (OptionData) {
        .name = "Display",
        .type = OPTION_TYPE_DROPDOWN,
        .min_value = 0,
        .max_value = RENDER_DISPLAY_COUNT - 1,
        #ifdef GOLD_DEBUG
        .default_value = RENDER_DISPLAY_WINDOWED
        #else
        .default_value = RENDER_DISPLAY_FULLSCREEN
        #endif
    }},
    { OPTION_VSYNC, (OptionData) {
        .name = "Vsync",
        .type = OPTION_TYPE_DROPDOWN,
        .min_value = 0,
        .max_value = RENDER_VSYNC_COUNT - 1,
        .default_value = RENDER_VSYNC_ENABLED
    }},
    { OPTION_SFX_VOLUME, (OptionData) {
        .name = "Sound Volume",
        .type = OPTION_TYPE_PERCENT_SLIDER,
        .min_value = 0,
        .max_value = 100,
        .default_value = 100
    }},
    { OPTION_MUSIC_VOLUME, (OptionData) {
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
        .max_value = 32,
        .default_value = 16
    }}
};

const OptionData& option_get_data(OptionName name) {
    return OPTION_DATA.at(name);
}
static std::unordered_map<OptionName, uint32_t> options;

const std::string options_get_file_path() {
    return filesystem_get_data_path() + "options.json";
}

void options_load() {
    for (int option = 0; option < OPTION_COUNT; option++) {
        options[(OptionName)option] = OPTION_DATA.at((OptionName)option).default_value;
    }

    const std::string options_file_path = options_get_file_path();
    Json* options_json = json_read(options_file_path.c_str());
    if (options_json != NULL) {
        for (size_t index = 0; index < options_json->object.length; index++) {
            const char* key = options_json->object.keys[index];

            int option;
            for (option = 0; option < OPTION_COUNT; option++) {
                if (strcmp(option_get_data((OptionName)option).name, key) == 0) {
                    break;
                }
            }
            if (option == OPTION_COUNT) {
                log_warn("Unrecognized option %s.", key);
                continue;
            }

            const OptionData& option_data = option_get_data((OptionName)option);
            uint32_t option_value = (uint32_t)json_object_get_number(options_json, key);
            if (option_value < option_data.min_value || option_value > option_data.max_value) {
                log_warn("Option value of %u for %s is out of range.", option_value, key);
                continue;
            }

            options[(OptionName)option] = option_value;
        }
    } else {
        log_info("No options file found. Saving defaults.");
        options_save();
    }
}

void options_save() {
    Json* options_json = json_object();
    for (int option = 0; option < OPTION_COUNT; option++) {
        json_object_set_number(options_json, OPTION_DATA.at((OptionName)option).name, options[(OptionName)option]);
    }

    const std::string options_file_path = options_get_file_path();
    if (!json_write(options_json, options_file_path.c_str())) {
        log_error("Could not save options json.");
    }
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
        case OPTION_MUSIC_VOLUME: {
            sound_set_music_volume(options[name]);
            break;
        }
        default:
            break;
    }
}