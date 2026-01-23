#pragma once

#include "defines.h"

#include "match/entity.h"

const uint32_t BOT_CONFIG_SHOULD_ATTACK_FIRST = 1U << 0U;
const uint32_t BOT_CONFIG_SHOULD_ATTACK = 1U << 1U;
const uint32_t BOT_CONFIG_SHOULD_HARASS = 1U << 2U;
const uint32_t BOT_CONFIG_SHOULD_RETREAT = 1U << 3U;
const uint32_t BOT_CONFIG_SHOULD_SCOUT = 1U << 4U;
const uint32_t BOT_CONFIG_FLAG_COUNT = 5;

const uint32_t BOT_TARGET_BASE_COUNT_MATCH_ENEMY = UINT32_MAX;

struct BotConfig {
    uint32_t flags;
    uint32_t target_base_count;
    uint32_t macro_cycle_cooldown;
    uint32_t allowed_upgrades;
    bool is_entity_allowed[ENTITY_TYPE_COUNT];
    uint8_t padding1 = 0;
    uint8_t padding2 = 0;
};
STATIC_ASSERT(sizeof(BotConfig) == 40ULL);

BotConfig bot_config_init();
BotConfig bot_config_init_from_difficulty(Difficulty difficulty);

#ifdef GOLD_DEBUG
const char* bot_config_flag_str(uint32_t flag);
uint32_t bot_config_flag_from_str(const char* str);
#endif