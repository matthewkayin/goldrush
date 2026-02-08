#include "options.h"

#include "logger.h"
#include "sound.h"
#include "input.h"
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
        .max_value = 32,
        .default_value = 16
    }}
};

const OptionData& option_get_data(OptionName name) {
    return OPTION_DATA.at(name);
}
static std::unordered_map<OptionName, uint32_t> options;

void options_load() {
    for (int option = 0; option < OPTION_COUNT; option++) {
        options[(OptionName)option] = OPTION_DATA.at((OptionName)option).default_value;
    }

    std::string options_file_path = filesystem_get_data_path() + OPTIONS_FILE_NAME;
    std::ifstream options_file(options_file_path);
    if (options_file.is_open()) {
        std::string line;
        while (std::getline(options_file, line)) {
            size_t equals_index = line.find('=');
            std::string key = line.substr(0, equals_index);
            std::string value = line.substr(equals_index + 1);

            int option;
            for (option = 0; option < OPTION_COUNT; option++) {
                const OptionData& option_data = OPTION_DATA.at((OptionName)option);

                if (key == std::string(option_data.name)) {
                    uint32_t option_value = (uint32_t)std::stoi(value);
                    if (option_value < option_data.min_value || option_value > option_data.max_value) {
                        log_warn("Option value of %u for %s is out of range.", option_value, option_data.name);
                        continue;
                    }

                    options[(OptionName)option] = option_value;
                    break;
                }
            }
            if (option == OPTION_COUNT) {
                log_warn("Unrecognized option key %s with value %s", key.c_str(), value.c_str());
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
    std::string options_file_path = filesystem_get_data_path() + OPTIONS_FILE_NAME;
    FILE* options_file = fopen(options_file_path.c_str(), "w");
    if (options_file == NULL) {
        log_error("Could not open options file for writing.");
        return;
    }

    for (int option = 0; option < OPTION_COUNT; option++) {
        fprintf(options_file, "%s=%i\n", OPTION_DATA.at((OptionName)option).name, options[(OptionName)option]);
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