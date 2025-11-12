#include "match_setting.h"

#include "core/asserts.h"
#include <unordered_map>

static std::unordered_map<MatchSetting, MatchSettingData> MATCH_SETTING_DATA; 

void match_setting_init() {
    MATCH_SETTING_DATA[MATCH_SETTING_TEAMS] = (MatchSettingData) {
        .name = "Teams",
        .values = { "Disabled", "Enabled" },
        .value_count = MATCH_SETTING_TEAMS_COUNT
    };
    MATCH_SETTING_DATA[MATCH_SETTING_MAP_SIZE] = (MatchSettingData) {
        .name = "Map Size",
        .values = { "Small", "Medium", "Large" },
        .value_count = MATCH_SETTING_TEAMS_COUNT
    };
    MATCH_SETTING_DATA[MATCH_SETTING_DIFFICULTY] = (MatchSettingData) {
        .name = "Difficulty",
        .values = { "Easy", "Moderate", "Hard" },
        .value_count = MATCH_SETTING_DIFFICULTY_COUNT
    };
}

const MatchSettingData& match_setting_data(MatchSetting setting) {
    static bool initialized = false;
    if (!initialized) {
        match_setting_init();
        initialized = true;
    }
    return MATCH_SETTING_DATA.at(setting);
}

uint32_t match_setting_get_map_size(MatchSettingMapSizeValue value) {
    switch (value) {
        case MATCH_SETTING_MAP_SIZE_SMALL:
            return 96;
        case MATCH_SETTING_MAP_SIZE_MEDIUM:
            return 128;
        case MATCH_SETTING_MAP_SIZE_LARGE:
            return 160;
        case MATCH_SETTING_MAP_SIZE_COUNT: {
            GOLD_ASSERT(false);
            return 0;
        }
    }
}