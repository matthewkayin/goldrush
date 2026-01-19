#include "config.h"

#include "match/upgrade.h"
#include "util/bitflag.h"

BotConfig bot_config_init() {
    BotConfig config;

    config.flags = 0;
    config.macro_cycle_cooldown = 0;
    config.target_base_count = 0;
    config.allowed_upgrades = 0;
    memset(config.is_entity_allowed, 0, sizeof(config.is_entity_allowed));

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
                BOT_CONFIG_SHOULD_SCOUT;
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
                BOT_CONFIG_SHOULD_SCOUT;
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
                BOT_CONFIG_SHOULD_SCOUT;
            config.macro_cycle_cooldown = 0;
            config.target_base_count = BOT_TARGET_BASE_COUNT_MATCH_ENEMY;
            break;
        }
        case DIFFICULTY_COUNT:
            GOLD_ASSERT(false);
    }

    return config;
}