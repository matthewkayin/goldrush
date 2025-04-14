#pragma once

#include "render/sprite.h"
#include <cstdint>

const uint32_t UPGRADE_WAR_WAGON = 1;
const uint32_t UPGRADE_LANDMINES = 2;
const uint32_t UPGRADE_BAYONETS = 1 << 2;

struct UpgradeData {
    const char* name;
    SpriteName icon;
    uint32_t gold_cost;
    uint32_t research_duration;
};

const UpgradeData& upgrade_get_data(uint32_t upgrade);