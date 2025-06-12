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
    }},
    { UPGRADE_PRIVATE_EYE, (UpgradeData) {
        .name = "Private Eye",
        .desc = "Grants detection to detectives",
        .icon = SPRITE_BUTTON_ICON_PRIVATE_EYE,
        .gold_cost = 200,
        .research_duration = 50
    }},
    { UPGRADE_STAKEOUT, (UpgradeData) {
        .name = "Stakeout",
        .desc = "Increases detective energy",
        .icon = SPRITE_BUTTON_ICON_STAKEOUT,
        .gold_cost = 250,
        .research_duration = 60
    }},
    { UPGRADE_SERRATED_KNIVES, (UpgradeData) {
        .name = "Serrated Knives",
        .desc = "Makes bandit attacks inflict bleed",
        .icon = SPRITE_BUTTON_ICON_SERRATED_KNIVES,
        .gold_cost = 250,
        .research_duration = 60
    }},
    { UPGRADE_BLACK_POWDER, (UpgradeData) {
        .name = "Black Powder",
        .desc = "Grants ranged units +1 damage",
        .icon = SPRITE_BUTTON_ICON_BLACK_POWDER,
        .gold_cost = 250,
        .research_duration = 60
    }}
};

const UpgradeData& upgrade_get_data(uint32_t upgrade) {
    return UPGRADE_DATA.at(upgrade);
}