#include "match_setting.h"

#include "core/asserts.h"

static const char* MATCH_SETTING_TEAMS_STRING[] = {
    "Disabled", "Enabled"
};
static const char* MATCH_SETTING_MAP_SIZE_STRING[MATCH_SETTING_MAP_SIZE_COUNT] = {
    "Small", "Medium", "Large"
};

MatchSettingData match_setting_data(MatchSetting setting) {
    switch (setting) {
        case MATCH_SETTING_TEAMS:
            return (MatchSettingData) {
                .name = "Teams",
                .values = MATCH_SETTING_TEAMS_STRING,
                .value_count = MATCH_SETTING_TEAMS_COUNT
            };
        case MATCH_SETTING_MAP_SIZE:
            return (MatchSettingData) {
                .name = "Map Size",
                .values = MATCH_SETTING_MAP_SIZE_STRING,
                .value_count = MATCH_SETTING_MAP_SIZE_COUNT
            };
        case MATCH_SETTING_COUNT:
            GOLD_ASSERT(false);
            const char* silence_warnings[] = { "" };
            return (MatchSettingData) {
                .name = "",
                .values = silence_warnings,
                .value_count = 1
            };
            break;
    }
}