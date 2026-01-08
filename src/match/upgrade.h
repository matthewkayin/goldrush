#pragma once

#include "defines.h"
#include "render/sprite.h"

const uint32_t UPGRADE_WAR_WAGON = 1 << 0;
const uint32_t UPGRADE_LANDMINES = 1 << 1;
const uint32_t UPGRADE_BAYONETS = 1 << 2;
const uint32_t UPGRADE_PRIVATE_EYE = 1 << 3;
const uint32_t UPGRADE_STAKEOUT = 1 << 4;
const uint32_t UPGRADE_SERRATED_KNIVES = 1 << 5;
const uint32_t UPGRADE_TAILWIND = 1 << 6;
const uint32_t UPGRADE_IRON_SIGHTS = 1 << 7;

struct UpgradeData {
    const char* name;
    const char* desc;
    SpriteName icon;
    uint32_t gold_cost;
    uint32_t research_duration;
};

const UpgradeData& upgrade_get_data(uint32_t upgrade);