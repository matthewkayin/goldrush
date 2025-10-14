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
};

const MatchSettingData& match_setting_data(MatchSetting setting) {
    return MATCH_SETTING_DATA.at(setting);
}