#include "config.h"

#include "match/upgrade.h"
#include "util/bitflag.h"
#include "util/util.h"
#include "match/lcg.h"

BotConfig bot_config_init() {
    BotConfig config;

    config.flags = 0;
    config.macro_cycle_cooldown = 0;
    config.target_base_count = 0;
    config.allowed_upgrades = 0;
    memset(config.is_entity_allowed, 0, sizeof(config.is_entity_allowed));
    config.opener = BOT_OPENER_TECH_FIRST;
    config.preferred_unit_comp = BOT_UNIT_COMP_COWBOY_BANDIT;

    return config;
}

BotConfig bot_config_init_from_difficulty(Difficulty difficulty) {
    BotConfig config = bot_config_init();

    for (uint32_t upgrade_index = 0; upgrade_index < UPGRADE_COUNT; upgrade_index++) {
        config.allowed_upgrades |= (1U << upgrade_index);
    }
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        config.is_entity_allowed[entity_type] = true;
    }

    switch (difficulty) {
        case DIFFICULTY_EASY: {
            config.flags = 
                BOT_CONFIG_SHOULD_ATTACK |
                BOT_CONFIG_SHOULD_SCOUT |
                BOT_CONFIG_SHOULD_SURRENDER |
                BOT_CONFIG_SHOULD_CANCEL_BUILDINGS;
            config.macro_cycle_cooldown = 1U * 60U * UPDATES_PER_SECOND;
            config.target_base_count = 1U;
            config.allowed_upgrades = 0;
            config.allowed_upgrades = UPGRADE_LANDMINES;
            break;
        }
        case DIFFICULTY_MODERATE: {
            config.flags =
                BOT_CONFIG_SHOULD_ATTACK |
                BOT_CONFIG_SHOULD_ATTACK_FIRST |
                BOT_CONFIG_SHOULD_RETREAT | 
                BOT_CONFIG_SHOULD_SCOUT |
                BOT_CONFIG_SHOULD_SURRENDER | 
                BOT_CONFIG_SHOULD_CANCEL_BUILDINGS;
            config.macro_cycle_cooldown = 1U * 30U * UPDATES_PER_SECOND;
            config.target_base_count = 2U;
            break;
        }
        case DIFFICULTY_HARD: {
            config.flags =
                BOT_CONFIG_SHOULD_ATTACK |
                BOT_CONFIG_SHOULD_ATTACK_FIRST |
                BOT_CONFIG_SHOULD_HARASS |
                BOT_CONFIG_SHOULD_RETREAT |
                BOT_CONFIG_SHOULD_SCOUT | 
                BOT_CONFIG_SHOULD_SURRENDER |
                BOT_CONFIG_SHOULD_CANCEL_BUILDINGS;
            config.macro_cycle_cooldown = 0;
            config.target_base_count = BOT_TARGET_BASE_COUNT_MATCH_ENEMY;
            break;
        }
        case DIFFICULTY_COUNT:
            GOLD_ASSERT(false);
    }

    return config;
}

BotOpener bot_config_roll_opener(int* lcg_seed, Difficulty difficulty) {
    if (difficulty == DIFFICULTY_EASY) {
        return BOT_OPENER_BUNKER;
    }

    const BotOpener valid_openers[] = { BOT_OPENER_BUNKER, BOT_OPENER_BANDIT_RUSH, BOT_OPENER_EXPAND_FIRST };
    const int valid_openers_count = 3;

    int opener_roll = lcg_rand(lcg_seed) % valid_openers_count;
    return valid_openers[opener_roll];
}

BotUnitComp bot_config_roll_preferred_unit_comp(int* lcg_seed) {
    const int UNIT_COMP_COUNT = 5;
    const BotUnitComp unit_comps[5] = {
        BOT_UNIT_COMP_COWBOY_BANDIT_PYRO,
        BOT_UNIT_COMP_COWBOY_SAPPER_PYRO,
        BOT_UNIT_COMP_COWBOY_WAGON,
        BOT_UNIT_COMP_SOLDIER_BANDIT,
        BOT_UNIT_COMP_SOLDIER_CANNON
    };
    int unit_comp_roll = lcg_rand(lcg_seed) % UNIT_COMP_COUNT;

    return unit_comps[unit_comp_roll];
}

const char* bot_config_flag_str(uint32_t flag) {
    switch (flag) {
        case BOT_CONFIG_SHOULD_ATTACK_FIRST:
            return "Attack First";
        case BOT_CONFIG_SHOULD_ATTACK:
            return "Attack";
        case BOT_CONFIG_SHOULD_HARASS:
            return "Harass";
        case BOT_CONFIG_SHOULD_RETREAT:
            return "Retreat";
        case BOT_CONFIG_SHOULD_SCOUT:
            return "Scout";
        case BOT_CONFIG_SHOULD_SURRENDER:
            return "Surrender";
        case BOT_CONFIG_SHOULD_PAUSE:
            return "Pause";
        case BOT_CONFIG_SHOULD_CANCEL_BUILDINGS:
            return "Cancel Buildings";
    }

    GOLD_ASSERT(false);
    return "";
}

uint32_t bot_config_flag_from_str(const char* str) {
    for (uint32_t index = 0; index < BOT_CONFIG_FLAG_COUNT; index++) {
        uint32_t flag = 1U << index;
        if (strcmp(bot_config_flag_str(flag), str) == 0) {
            return flag;
        }
    }

    return BOT_CONFIG_FLAG_COUNT;
}

const char* bot_config_opener_str(BotOpener opener) {
    switch (opener) {
        case BOT_OPENER_BANDIT_RUSH:
            return "Bandit Rush";
        case BOT_OPENER_BUNKER:
            return "Bunker";
        case BOT_OPENER_EXPAND_FIRST:
            return "Expand First";
        case BOT_OPENER_TECH_FIRST:
            return "Tech First";
        case BOT_OPENER_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

BotOpener bot_config_opener_from_str(const char* str) {
    return (BotOpener)enum_from_str(str, (EnumToStrFn)bot_config_opener_str, BOT_OPENER_COUNT, BOT_OPENER_TECH_FIRST);
}

const char* bot_config_unit_comp_str(BotUnitComp unit_comp) {
    switch (unit_comp) {
        case BOT_UNIT_COMP_NONE:
            return "None";
        case BOT_UNIT_COMP_COWBOY_BANDIT:
            return "Cwby/Bndt";
        case BOT_UNIT_COMP_COWBOY_BANDIT_PYRO:
            return "Cwby/Bndt/Pyro";
        case BOT_UNIT_COMP_COWBOY_SAPPER_PYRO:
            return "Cwby/Sppr/Pyro";
        case BOT_UNIT_COMP_COWBOY_WAGON:
            return "Cwby/Wgn";
        case BOT_UNIT_COMP_SOLDIER_BANDIT:
            return "Sldr/Bndt";
        case BOT_UNIT_COMP_SOLDIER_CANNON:
            return "Sldr/Cnn";
        case BOT_UNIT_COMP_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

BotUnitComp bot_config_unit_comp_from_str(const char* str) {
    return (BotUnitComp)enum_from_str(str, (EnumToStrFn)bot_config_unit_comp_str, BOT_UNIT_COMP_COUNT, BOT_UNIT_COMP_NONE);
}