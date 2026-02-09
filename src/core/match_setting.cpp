#include "match_setting.h"

#include "core/asserts.h"
#include <unordered_map>

static std::unordered_map<MatchSetting, MatchSettingData> MATCH_SETTING_DATA; 

void match_setting_init() {
    MATCH_SETTING_DATA[MATCH_SETTING_TEAMS] = (MatchSettingData) {
        .name = "Teams",
        .values = { "Disabled", "Enabled" },
        .value_count = TEAMS_COUNT
    };
    MATCH_SETTING_DATA[MATCH_SETTING_MAP_TYPE] = (MatchSettingData) {
        .name = "Map Type",
        .values = { "Tombstone", "Klondike" },
        .value_count = MAP_TYPE_COUNT
    };
    MATCH_SETTING_DATA[MATCH_SETTING_MAP_SIZE] = (MatchSettingData) {
        .name = "Map Size",
        .values = { "Small", "Medium", "Large" },
        .value_count = TEAMS_COUNT
    };
    MATCH_SETTING_DATA[MATCH_SETTING_DIFFICULTY] = (MatchSettingData) {
        .name = "Difficulty",
        .values = { "Easy", "Moderate", "Hard" },
        .value_count = DIFFICULTY_COUNT
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

int match_setting_get_map_size(MapSize value) {
    switch (value) {
        case MAP_SIZE_SMALL:
            return 96;
        case MAP_SIZE_MEDIUM:
            return 128;
        case MAP_SIZE_LARGE:
            return MAX_MAP_SIZE;
        case MAP_SIZE_COUNT: {
            GOLD_ASSERT(false);
            return 0;
        }
    }
}