#include "match_setting.h"

#include "core/asserts.h"
#include <unordered_map>

static const char* const MATCH_SETTING_TEAMS_VALUES[] = { "Disabled", "Enabled" };
static const char* const MATCH_SETTING_MAP_SIZE_VALUES[] = { "Small", "Medium", "Large" };

static std::unordered_map<MatchSetting, MatchSettingData> MATCH_SETTING_DATA = {
    { MATCH_SETTING_TEAMS, (MatchSettingData) {
        .name = "Teams",
        .values = MATCH_SETTING_TEAMS_VALUES,
        .value_count = MATCH_SETTING_TEAMS_COUNT
    }},
    { MATCH_SETTING_MAP_SIZE, (MatchSettingData) {
        .name = "Map Size",
        .values = MATCH_SETTING_MAP_SIZE_VALUES,
        .value_count = MATCH_SETTING_MAP_SIZE_COUNT
    }}
};

const MatchSettingData& match_setting_data(MatchSetting setting) {
    return MATCH_SETTING_DATA.at(setting);
}