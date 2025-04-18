#pragma once

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
    int min_value;
    int max_value;
    int default_value;
};

const OptionData& option_get_data(OptionName name);

void options_load();
void options_save();
int option_get_value(OptionName name);
void option_set_value(OptionName name, int value);
void option_apply(OptionName name);