#pragma once

#include "defines.h"

#include "match/entity.h"

enum BotOpener {
    BOT_OPENER_BANDIT_RUSH,
    BOT_OPENER_BUNKER,
    BOT_OPENER_EXPAND_FIRST,
    BOT_OPENER_TECH_FIRST,
    BOT_OPENER_COUNT
};

enum BotUnitComp {
    BOT_UNIT_COMP_NONE,
    BOT_UNIT_COMP_COWBOY_BANDIT,
    BOT_UNIT_COMP_COWBOY_BANDIT_PYRO,
    BOT_UNIT_COMP_COWBOY_SAPPER_PYRO,
    BOT_UNIT_COMP_COWBOY_WAGON,
    BOT_UNIT_COMP_SOLDIER_BANDIT,
    BOT_UNIT_COMP_SOLDIER_CANNON,
    BOT_UNIT_COMP_COUNT
};

const uint32_t BOT_CONFIG_SHOULD_ATTACK_FIRST = 1U << 0U;
const uint32_t BOT_CONFIG_SHOULD_ATTACK = 1U << 1U;
const uint32_t BOT_CONFIG_SHOULD_HARASS = 1U << 2U;
const uint32_t BOT_CONFIG_SHOULD_RETREAT = 1U << 3U;
const uint32_t BOT_CONFIG_SHOULD_SCOUT = 1U << 4U;
const uint32_t BOT_CONFIG_SHOULD_SURRENDER = 1U << 5U;
const uint32_t BOT_CONFIG_FLAG_COUNT = 6;

const uint32_t BOT_TARGET_BASE_COUNT_MATCH_ENEMY = UINT32_MAX;

struct BotConfig {
    uint32_t flags;
    uint32_t target_base_count;
    uint32_t macro_cycle_cooldown;
    uint32_t allowed_upgrades;
    bool is_entity_allowed[ENTITY_TYPE_COUNT];
    uint8_t padding1 = 0;
    uint8_t padding2 = 0;
    BotOpener opener;
    BotUnitComp preferred_unit_comp;
};
STATIC_ASSERT(sizeof(BotConfig) == 48ULL);

BotConfig bot_config_init();
BotConfig bot_config_init_from_difficulty(Difficulty difficulty);
BotOpener bot_config_roll_opener(int* lcg_seed, Difficulty difficulty);
BotUnitComp bot_config_roll_preferred_unit_comp(int* lcg_seed);

const char* bot_config_flag_str(uint32_t flag);
uint32_t bot_config_flag_from_str(const char* str);
const char* bot_config_opener_str(BotOpener opener);
BotOpener bot_config_opener_from_str(const char* str);
const char* bot_config_unit_comp_str(BotUnitComp unit_comp);
BotUnitComp bot_config_unit_comp_from_str(const char* str);