#include "upgrade.h"

#include <unordered_map>

static const std::unordered_map<uint32_t, UpgradeData> UPGRADE_DATA = {
    { UPGRADE_WAR_WAGON, (UpgradeData) {
        .name = "Wagon Armor",
        .desc = "Allows units to fire from inside wagons",
        .icon = SPRITE_BUTTON_ICON_WAGON_ARMOR,
        .gold_cost = 300,
        .research_duration = 50
    }},
    { UPGRADE_LANDMINES, (UpgradeData) {
        .name = "Land Mines",
        .desc = "Allows Pyros to place Land Mines",
        .icon = SPRITE_BUTTON_ICON_LANDMINE,
        .gold_cost = 250,
        .research_duration = 60
    }},
    { UPGRADE_BAYONETS, (UpgradeData) {
        .name = "Bayonets",
        .desc = "Grants melee attack to soldiers",
        .icon = SPRITE_BUTTON_ICON_BAYONETS,
        .gold_cost = 300,
        .research_duration = 50
    }}
};

const UpgradeData& upgrade_get_data(uint32_t upgrade) {
    return UPGRADE_DATA.at(upgrade);
}