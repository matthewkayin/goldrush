#pragma once

#include <cstdint>

enum OptionName {
    OPTION_DISPLAY,
    OPTION_VSYNC,
    OPTION_SFX_VOLUME,
    OPTION_MUS_VOLUME,
    OPTION_UNIT_VOICES,
    OPTION_CAMERA_SPEED,
    OPTION_COUNT
};

enum OptionValueUnitVoices {
    OPTION_UNIT_VOICES_OFF,
    OPTION_UNIT_VOICES_ON,
    OPTION_UNIT_VOICES_COUNT,
};

enum OptionType {
    OPTION_TYPE_DROPDOWN,
    OPTION_TYPE_PERCENT_SLIDER,
    OPTION_TYPE_SLIDER
};

struct OptionData {
    const char* name;
    OptionType type;
    uint32_t min_value;
    uint32_t max_value;
    uint32_t default_value;
};

const OptionData& option_get_data(OptionName name);

void options_load();
void options_save();
uint32_t option_get_value(OptionName name);
void option_set_value(OptionName name, uint32_t value);
void option_apply(OptionName name);