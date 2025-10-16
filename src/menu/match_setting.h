#pragma once

#include <cstddef>
#include <vector> 
#include <string>

enum MatchSetting {
    MATCH_SETTING_TEAMS,
    MATCH_SETTING_MAP_SIZE,
    MATCH_SETTING_COUNT
};

struct MatchSettingData {
    const char* name;
    std::vector<std::string> values;
    size_t value_count;
};

enum MatchSettingTeamsValue {
    MATCH_SETTING_TEAMS_DISABLED,
    MATCH_SETTING_TEAMS_ENABLED,
    MATCH_SETTING_TEAMS_COUNT
};

enum MatchSettingMapSizeValue {
    MATCH_SETTING_MAP_SIZE_SMALL,
    MATCH_SETTING_MAP_SIZE_MEDIUM,
    MATCH_SETTING_MAP_SIZE_LARGE,
    MATCH_SETTING_MAP_SIZE_GIANT,
    MATCH_SETTING_MAP_SIZE_COUNT
};

void match_setting_init();
const MatchSettingData& match_setting_data(MatchSetting setting);

uint32_t match_setting_get_map_size(MatchSettingMapSizeValue value);