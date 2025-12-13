#pragma once

#include <cstddef>
#include <vector> 
#include <string>
#include <cstdint>

enum MatchSetting {
    MATCH_SETTING_TEAMS,
    MATCH_SETTING_MAP_TYPE,
    MATCH_SETTING_MAP_SIZE,
    MATCH_SETTING_DIFFICULTY,
    MATCH_SETTING_COUNT
};

struct MatchSettingData {
    const char* name;
    std::vector<std::string> values;
    size_t value_count;
};

enum Teams {
    TEAMS_DISABLED,
    TEAMS_ENABLED,
    TEAMS_COUNT
};

enum MapType {
    MAP_TYPE_ARIZONA,
    MAP_TYPE_KLONDIKE,
    MAP_TYPE_COUNT
};

enum MapSize {
    MAP_SIZE_SMALL,
    MAP_SIZE_MEDIUM,
    MAP_SIZE_LARGE,
    MAP_SIZE_COUNT
};

enum Difficulty {
    DIFFICULTY_EASY,
    DIFFICULTY_MODERATE,
    DIFFICULTY_HARD,
    DIFFICULTY_COUNT
};

const MatchSettingData& match_setting_data(MatchSetting setting);
uint32_t match_setting_get_map_size(MapSize value);