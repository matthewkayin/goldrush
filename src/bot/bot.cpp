#include "bot.h"

#include "core/logger.h"
#include "match/upgrade.h"
#include "match/lcg.h"
#include "profile/profile.h"
#include "util/bitflag.h"
#include "util/util.h"

// Scout info
static const uint32_t BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES = 1;
static const uint32_t BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES = 1 << 1;
static const uint32_t BOT_SCOUT_INFO_ENEMY_HAS_ATTACKED = 1 << 2;

// Unit scores
static const int BOT_UNIT_SCORE = 4;
static const int BOT_UNIT_SCORE_IN_BUNKER = 6;

// Near dist
static const int BOT_NEAR_DISTANCE = 16;
static const int BOT_MEDIUM_DISTANCE = 32;

// Defend options
static const uint32_t BOT_DEFEND_COUNTERATTACK = 1;
static const uint32_t BOT_DEFEND_WITH_WORKERS = 1 << 1;

// Attack thresholds
static const uint32_t BOT_HARASS_SQUAD_MAX_SIZE = 8U;

// Landmines
static const uint32_t LANDMINE_MAX_PER_BASE = 6U;

// This function zeroes out everything that doesn't have a default constructor
// It is used to initialize unused bots, i.e. bots that will not be initialized with bot_init()
// This is so that the desync detector does not yell at us when these unused bots are out of sync
Bot bot_empty() {
    Bot bot;

    memset(&bot.config, 0, sizeof(bot.config));
    bot.player_id = PLAYER_NONE;
    bot.padding = 0;

    bot.unit_comp = BOT_UNIT_COMP_NONE;
    bot.macro_cycle_timer = 0;
    bot.macro_cycle_count = 0;

    bot.next_squad_id = 0;
    bot.next_landmine_time = 0;

    bot.scout_id = ID_NULL;
    bot.scout_info = 0;
    bot.last_scout_time = 0;

    return bot;
}

Bot bot_init(const MatchState* state, uint8_t player_id, BotConfig config) {
    Bot bot = bot_empty();

    bot.player_id = player_id;
    bot.config = config;

    switch (config.opener) {
        case BOT_OPENER_BANDIT_RUSH: {
            BotDesiredSquad desired_squad;
            desired_squad.type = BOT_SQUAD_TYPE_ATTACK;
            desired_squad.entity_count[ENTITY_WAGON] = 1;
            desired_squad.entity_count[ENTITY_BANDIT] = 4;

            bot.desired_squads.push_back(desired_squad);

            break;
        }
        case BOT_OPENER_BUNKER: {
            BotDesiredSquad desired_squad;
            desired_squad.type = BOT_SQUAD_TYPE_DEFEND;
            desired_squad.entity_count[ENTITY_BUNKER] = 1;
            desired_squad.entity_count[ENTITY_COWBOY] = 4;

            bot.desired_squads.push_back(desired_squad);

            break;
        }
        case BOT_OPENER_EXPAND_FIRST: {
            bot.unit_comp = BOT_UNIT_COMP_COWBOY_BANDIT;

            break;
        }
        case BOT_OPENER_TECH_FIRST: {
            bot.unit_comp = bot.config.preferred_unit_comp;

            break;
        }
        case BOT_OPENER_COUNT: {
            GOLD_ASSERT(false);
            break;
        }
    }

    // Init base info
    EntityList goldmine_ids = match_find_entities(state, [](const Entity& goldmine, EntityId /*goldmine_id*/) {
        return goldmine.type == ENTITY_GOLDMINE;
    });
    GOLD_ASSERT(goldmine_ids.size() < BOT_MAX_BASE_INFO);
    for (uint32_t goldmine_id_index = 0; goldmine_id_index < goldmine_ids.size(); goldmine_id_index++) {
        BotBaseInfo info;
        bot_clear_base_info(info);
        info.goldmine_id = goldmine_ids[goldmine_id_index];
        info.retreat_count = 0;
        info.retreat_time = 0;

        bot.base_info.push_back(info);
    }

    log_info("BOT with player id %u initialized with opener %u preferred comp %u", bot.player_id, bot.config.opener, bot.config.preferred_unit_comp);

    return bot;
}

MatchInput bot_get_turn_input(const MatchState* state, Bot& bot, uint32_t match_timer) {
    ZoneScoped;

    GOLD_ASSERT_MESSAGE(state->players[bot.player_id].active, "bot_get_turn_input should not be called after bot has surrendered.");

    // Gather info

    bot_scout_gather_info(state, bot);

    // Surrender

    if (bot_should_surrender(state, bot, match_timer)) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Strategy

    bot_strategy_update(state, bot);

    // Production

    if (bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_PRODUCE)) {
        MatchInput production_input = bot_get_production_input(state, bot, match_timer);
        if (production_input.type != MATCH_INPUT_NONE) {
            return production_input;
        }
    }

    // Squads

    uint32_t squad_index = 0;
    while (squad_index < bot.squads.size()) {
        MatchInput input = bot_squad_update(state, bot, bot.squads[squad_index], match_timer);
        if (bot.squads[squad_index].entity_list.empty()) {
            log_debug("BOT %u, removing squad %u", bot.player_id, bot.squads[squad_index].id);
            bot.squads[squad_index] = bot.squads.back();
            bot.squads.pop_back();
        } else {
            squad_index++;
        }

        if (input.type != MATCH_INPUT_NONE) {
            return input;
        }
    }

    // Scout

    MatchInput scout_input = bot_scout(state, bot, match_timer);
    if (scout_input.type != MATCH_INPUT_NONE) {
        return scout_input;
    }

    // Misc

    // Cancel in-progress buildings
    if (bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_CANCEL_BUILDINGS)) {
        EntityId threatened_in_progress_building_id = bot_find_threatened_in_progress_building(state, bot);
        if (threatened_in_progress_building_id != ID_NULL) {
            MatchInput input;
            input.type = MATCH_INPUT_BUILD_CANCEL;
            input.build_cancel.building_id = threatened_in_progress_building_id;

            log_debug("BOT %u get_turn_input, cancel threatened building %u", bot.player_id, threatened_in_progress_building_id);
            return input;
        }
    }

    // Repair burning buildings
    EntityId building_to_repair_id = bot_find_building_in_need_of_repair(state, bot);
    if (building_to_repair_id != ID_NULL) {
        MatchInput repair_input = bot_repair_building(state, bot, building_to_repair_id);
        if (repair_input.type != MATCH_INPUT_NONE) {
            return repair_input;
        }
    }

    // Rein in stray units
    MatchInput rein_input = bot_rein_in_stray_units(state, bot);
    if (rein_input.type != MATCH_INPUT_NONE) {
        return rein_input;
    }

    // Set building rally points
    while (!bot.buildings_to_set_rally_points.empty()) {
        EntityId building_id = bot.buildings_to_set_rally_points.front();
        bot.buildings_to_set_rally_points.pop();

        MatchInput rally_input = bot_update_building_rally_point(state, bot, building_id);
        if (rally_input.type != MATCH_INPUT_NONE) {
            return rally_input;
        }
    }

    // Unload unreserved carriers
    MatchInput unload_input = bot_unload_unreserved_carriers(state, bot);
    if (unload_input.type != MATCH_INPUT_NONE) {
        return unload_input;
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

// STRATEGY

void bot_strategy_update(const MatchState* state, Bot& bot) {
    ZoneScoped;
    
    // Handle base under attack
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player == bot.player_id && base_info.is_under_attack) {
            bitflag_set(&bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_ATTACKED, true);
            ivec2 location = state->entities.get_by_id(base_info.goldmine_id).cell;
            bot_defend_location(state, bot, location, BOT_DEFEND_COUNTERATTACK | BOT_DEFEND_WITH_WORKERS);
        }
    }

    // Create squads from desired squads
    {
        EntityCount unreserved_entity_count = bot_count_unreserved_entities(state, bot);

        uint32_t desired_squad_index = 0;
        while (desired_squad_index < bot.desired_squads.size()) {
            if (unreserved_entity_count.is_gte_to(bot.desired_squads[desired_squad_index].entity_count)) {
                // Create desired squad
                EntityList entity_list = bot_create_entity_list_from_entity_count(state, bot, bot.desired_squads[desired_squad_index].entity_count);
                BotSquadType type = bot.desired_squads[desired_squad_index].type;
                ivec2 target_cell = bot_squad_choose_target_cell(state, bot, type, entity_list);
                bot_add_squad(bot, {
                    .type = type,
                    .target_cell = target_cell,
                    .entity_list = entity_list
                });

                // Subtract squad units from unreserved entity count
                unreserved_entity_count = unreserved_entity_count.subtract(bot.desired_squads[desired_squad_index].entity_count);

                // Remove desired squad from list
                bot.desired_squads[desired_squad_index] = bot.desired_squads.back();
                bot.desired_squads.pop_back();
            } else {
                desired_squad_index++;
            }
        }
    }

    if (bot_is_bandit_rushing(bot)) {
        // Abandon rush if wagon has died
        EntityId wagon_id = match_find_entity(state, [&bot](const Entity& wagon, EntityId /*wagon_id*/) {
            return wagon.type == ENTITY_WAGON &&
                wagon.player_id == bot.player_id &&
                entity_is_selectable(wagon);
        });
        if (wagon_id == ID_NULL) {
            bot.desired_squads.clear();
            bot_set_unit_comp(bot, BOT_UNIT_COMP_COWBOY_BANDIT);
        }
    }

    // If doing an opener, switch unit comps once the opener squad is finished
    if (bot.unit_comp == BOT_UNIT_COMP_NONE && bot.desired_squads.empty()) {
        bot_set_unit_comp(bot, BOT_UNIT_COMP_COWBOY_BANDIT);
    }

    // If expanding, secure the expansion
    if (bot_should_expand(state, bot)) {
        EntityId goldmine_id = bot_find_goldmine_for_next_expansion(state, bot);
        ivec2 goldmine_cell = state->entities.get_by_id(goldmine_id).cell;
        if (!bot_is_area_safe(state, bot, goldmine_cell)) {
            bot_defend_location(state, bot, goldmine_cell, 0);
        } 
    }

    // Tech up
    if (bot.unit_comp != bot.config.preferred_unit_comp && 
            bot_should_tech_into_preferred_unit_comp(state, bot)) {
        bot_set_unit_comp(bot, bot.config.preferred_unit_comp);
    }

    // Request landmines squad
    if (bot.desired_army_ratio[ENTITY_PYRO] != 0 &&
            !bot_has_squad_of_type(bot, BOT_SQUAD_TYPE_LANDMINES) &&
            !bot_has_desired_squad_of_type(bot, BOT_SQUAD_TYPE_LANDMINES)) {
        BotDesiredSquad squad;
        squad.type = BOT_SQUAD_TYPE_LANDMINES;
        squad.entity_count[ENTITY_PYRO] = 1;
        bot.desired_squads.push_back(squad);
    }

    // Determine unreserved army count
    EntityCount unreserved_army_count = bot_count_unreserved_army(state, bot);

    // Reinforce attack squads
    // We can assume that this squad is not retreating because a returning squad would no longer be in ATTACK mode
    uint32_t attack_squad_index = bot_get_index_of_squad_of_type(bot, BOT_SQUAD_TYPE_ATTACK);
    if (attack_squad_index < bot.squads.size() &&
            !unreserved_army_count.is_empty() &&
            bot.squads[attack_squad_index].entity_list.size() > BOT_HARASS_SQUAD_MAX_SIZE) {
        // Gather reinforcements list
        EntityList reinforcements = bot_create_entity_list_from_entity_count(state, bot, unreserved_army_count);

        // Filter out low-energy pyros
        bot_entity_list_filter(state, reinforcements, [](const Entity& entity, EntityId /*entity_id*/) {
            return entity.type == ENTITY_PYRO && entity.energy < MOLOTOV_ENERGY_COST;
        });

        // Add reinforcements to squad
        if (!reinforcements.empty()) {
            log_debug("BOT %u strategy_update, reinforcing existing squad", bot.player_id);
            for (uint32_t reinforcements_index = 0; reinforcements_index < reinforcements.size(); reinforcements_index++) {
                EntityId entity_id = reinforcements[reinforcements_index];
                if (bot_reserve_entity(bot, entity_id)) {
                    bot.squads[attack_squad_index].entity_list.push_back(entity_id);
                }
            }

            return;
        }
    }

    // Attack
    if (bot_should_attack(state, bot)) {
        EntityList army = bot_create_entity_list_from_entity_count(state, bot, unreserved_army_count);
        ivec2 target_cell = bot_squad_get_attack_target_cell(state, bot, army);
        bot_add_squad(bot, {
            .type = BOT_SQUAD_TYPE_ATTACK,
            .target_cell = target_cell,
            .entity_list = army
        });
    }
}

bool bot_should_surrender(const MatchState* state, const Bot& bot, uint32_t match_timer) {
    if (!bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_SURRENDER)) {
        return false;
    }

    if (bot_has_non_miner_army(state, bot)) {
        return false;
    }

    EntityCount entity_count;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id == bot.player_id) {
            entity_count[entity.type]++;
        }
    }

    // If we have no units and are on only one base, then surrender
    if (entity_count[ENTITY_HALL] <= 1 && entity_count.unit_count() == 0) {
        return true;
    }

    // If we have no mining bases and no army and no money to get another base, then surrender
    EntityCount unreserved_army_count = bot_count_unreserved_army(state, bot);
    if (match_timer >= 5U * 60U * UPDATES_PER_SECOND &&
            unreserved_army_count.count() == 0 &&
            !bot_is_mining(state, bot) &&
            state->players[bot.player_id].gold <= entity_get_data(ENTITY_HALL).gold_cost) {
        return true;
    }

    return false;
}

bool bot_has_non_miner_army(const MatchState* state, const Bot& bot) {
    for (uint32_t squad_index = 0; squad_index < bot.squads.size(); squad_index++) {
        const BotSquad& squad = bot.squads[squad_index];
        if (squad.type == BOT_SQUAD_TYPE_ATTACK) {
            return true;
        }
        if (squad.type == BOT_SQUAD_TYPE_RESERVES) {
            bool is_miner_squad = true;
            for (uint32_t index = 0; index < squad.entity_list.size(); index++) {
                EntityId entity_id = squad.entity_list[index];
                // It's not safe to assume that these IDs are valid because the squads
                // might not have all been updated this turn
                uint32_t entity_index = state->entities.get_index_of(entity_id);
                if (entity_index != INDEX_INVALID && state->entities[entity_index].type != ENTITY_MINER) {
                    is_miner_squad = false;
                    break;
                }
            }
            if (!is_miner_squad) {
                return true;
            }
        }
    }

    return false;
}

bool bot_is_mining(const MatchState* state, const Bot& bot) {
    EntityId miner_id = match_find_entity(state, [&state, &bot](const Entity& miner, EntityId /*miner_id*/) {
        return miner.type == ENTITY_MINER &&
            miner.health != 0 &&
            miner.player_id == bot.player_id &&
            entity_is_mining(state, miner);
    });
    return miner_id != ID_NULL;
}

bool bot_should_expand(const MatchState* state, const Bot& bot) {
    // If unit comp is none, then we're doing an opener, so we should not expand yet
    if (bot.unit_comp == BOT_UNIT_COMP_NONE) {
        return false;
    }

    if (bot_has_base_that_is_missing_a_hall(bot)) {
        return true;
    }
    if (!bot_is_unoccupied_goldmine_available(bot)) {
        return false;
    }
    if (!bot_are_bases_fully_saturated(bot)) {
        return false;
    }

    uint32_t target_base_count = bot.config.target_base_count == BOT_TARGET_BASE_COUNT_MATCH_ENEMY
        ? std::max(2U, bot_get_max_enemy_mining_base_count(state, bot))
        : bot.config.target_base_count;

    // Replace bases if one of them is low on gold
    target_base_count += bot_get_low_on_gold_base_count(bot);

    return bot_get_player_mining_base_count(bot, bot.player_id) < target_base_count;
}

bool bot_should_tech_into_preferred_unit_comp(const MatchState* state, const Bot& bot) {
    return bot_get_player_mining_base_count(bot, bot.player_id) > 0 &&
        bot_are_bases_fully_saturated(bot) &&
        bot_score_allied_army(state, bot) > 7 * BOT_UNIT_SCORE;
}

uint32_t bot_get_player_mining_base_count(const Bot& bot, uint8_t player_id) {
    uint32_t count = 0;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player != player_id) {
            continue;
        }
        if (base_info.has_surrounding_hall && base_info.has_gold) {
            count++;
        }
    }

    return count;
}

bool bot_are_bases_fully_saturated(const Bot& bot) {
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player == bot.player_id && base_info.has_gold &&
                !base_info.is_saturated) {
            return false;
        }
    }

    return true;
}

uint32_t bot_get_low_on_gold_base_count(const Bot& bot) {
    uint32_t count = 0;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player == bot.player_id && base_info.is_low_on_gold) {
            count++;
        }
    }

    return count;
}

uint32_t bot_get_max_enemy_mining_base_count(const MatchState* state, const Bot& bot) {
    uint32_t max_enemy_mining_base_count = 0;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->players[player_id].active && state->players[player_id].team != state->players[bot.player_id].team) {
            max_enemy_mining_base_count = std::max(max_enemy_mining_base_count, bot_get_player_mining_base_count(bot, player_id));
        }
    }

    return max_enemy_mining_base_count;
}

bool bot_has_base_that_is_missing_a_hall(const Bot& bot, EntityId* goldmine_id) {
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player == bot.player_id && base_info.has_gold && !base_info.has_surrounding_hall) {
            if (goldmine_id != NULL) {
                *goldmine_id = base_info.goldmine_id;
            }
            return true;
        }
    }

    if (goldmine_id != NULL) {
        *goldmine_id = ID_NULL;
    }
    return false;
}

bool bot_is_unoccupied_goldmine_available(const Bot& bot) {
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player == PLAYER_NONE && base_info.has_gold) {
            return true;
        }
    }

    return false;
}

uint32_t bot_get_least_defended_enemy_base_info_index(const MatchState* state, const Bot& bot) {
    uint32_t least_defended_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player == PLAYER_NONE ||
                state->players[base_info.controlling_player].team == state->players[bot.player_id].team) {
            continue;
        }

        if (least_defended_base_info_index == INDEX_INVALID ||
                base_info.defense_score < bot.base_info[least_defended_base_info_index].defense_score) {
            least_defended_base_info_index = base_info_index;
        }
    }

    return least_defended_base_info_index;
}

bool bot_is_under_attack(const Bot& bot) {
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player == bot.player_id && base_info.is_under_attack) {
            return true;
        }
    }

    return false;
}

void bot_defend_location(const MatchState* state, Bot& bot, ivec2 location, uint32_t options) {
    bool should_counterattack = (options & BOT_DEFEND_COUNTERATTACK) == BOT_DEFEND_COUNTERATTACK;
    bool should_defend_with_workers = (options & BOT_DEFEND_WITH_WORKERS) == BOT_DEFEND_WITH_WORKERS;

    // Determine enemy score
    int enemy_score = bot_score_entities_at_location(state, bot, location, [&state, &bot](const Entity& enemy, EntityId /*enemy_id*/) {
        return entity_is_unit(enemy.type) && entity_is_selectable(enemy) &&
            state->players[enemy.player_id].team != state->players[bot.player_id].team;
    });

    // Determine defending score
    int defending_score = 0;
    for (uint32_t squad_index = 0; squad_index < bot.squads.size(); squad_index++) {
        const BotSquad& squad = bot.squads[squad_index];
        if (!(squad.type == BOT_SQUAD_TYPE_DEFEND || squad.type == BOT_SQUAD_TYPE_RESERVES)) {
            continue;
        }

        // Ignore this squad if it's far away
        if (ivec2::manhattan_distance(squad.target_cell, location) > BOT_MEDIUM_DISTANCE) {
            continue;
        }

        defending_score += bot_score_entity_list(state, bot, squad.entity_list);
    }

    // If this location is already well defended, then don't do anything
    if (defending_score > enemy_score + (2 * BOT_UNIT_SCORE)) {
        log_debug("BOT %u defend_location, defense is fine. do nothing.", bot.player_id);
        return;
    }

    // Otherwise the area is undefended, so let's see what we can do

    // Check to see if we have any unreserved units ot create a defense squad with
    EntityList unreserved_army = match_find_entities(state, [&bot](const Entity& entity, EntityId /*entity_id*/) {
        return entity_is_unit(entity.type) &&
            entity.type != ENTITY_MINER &&
            entity.type != ENTITY_WAGON &&
            entity_is_selectable(entity) &&
            entity.player_id == bot.player_id &&
            !entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED);
    });
    int unreserved_army_score = bot_score_entity_list(state, bot, unreserved_army);
    if (!unreserved_army.empty()) {
        ivec2 unreserved_army_center = bot_entity_list_get_center(state, unreserved_army);
        if (defending_score + unreserved_army_score >= enemy_score &&
                ivec2::manhattan_distance(unreserved_army_center, location) < BOT_MEDIUM_DISTANCE) {
            int squad_id = bot_add_squad(bot, {
                .type = BOT_SQUAD_TYPE_RESERVES,
                .target_cell = location,
                .entity_list = unreserved_army
            });
            log_debug("BOT %u defend_location, send in unreserved army %u, squad %u", bot.player_id, unreserved_army.size(), squad_id);
            return;
        }
    }

    // Next, check attacking squads to see if they should turn around and defend
    for (uint32_t squad_index = 0; squad_index < bot.squads.size(); squad_index++) {
        BotSquad& squad = bot.squads[squad_index];
        if (squad.type != BOT_SQUAD_TYPE_ATTACK) {
            continue;
        }
        // Don't send in a squad unless we feel pretty sure that this squad could cleanup the attack
        if (defending_score + bot_score_entity_list(state, bot, squad.entity_list) < enemy_score + (4 * BOT_UNIT_SCORE)) {
            continue;
        }

        // Count how many units are closer to the target than they are to the under-attack base
        uint32_t squad_units_closer_to_target = 0;
        uint32_t squad_units_closer_to_base = 0;
        for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
            EntityId entity_id = squad.entity_list[entity_list_index];
            uint32_t entity_index = state->entities.get_index_of(entity_id);
            if (entity_index == INDEX_INVALID) {
                continue;
            }

            if (ivec2::manhattan_distance(state->entities[entity_index].cell, squad.target_cell) <
                    ivec2::manhattan_distance(state->entities[entity_index].cell, location)) {
                squad_units_closer_to_target++;
            } else {
                squad_units_closer_to_base++;
            }
        }

        // If they are closer to the target, keep attacking the target
        if (squad_units_closer_to_target > squad_units_closer_to_base) {
            continue;
        }

        // Otherwise, switch targets to defend the base
        squad.type = BOT_SQUAD_TYPE_RESERVES;
        squad.target_cell = location;
        log_debug("BOT %u defend_location, turn around attacking squad", bot.player_id);
        return;
    }

    // Next, see if we can use unreserved units to mount a counterattack
    uint32_t least_defended_enemy_base_info_index = bot_get_least_defended_enemy_base_info_index(state, bot);
    if (should_counterattack &&
            least_defended_enemy_base_info_index != INDEX_INVALID &&
            !unreserved_army.empty() &&
            unreserved_army_score > bot.base_info[least_defended_enemy_base_info_index].defense_score) {
        ivec2 counterattack_squad_target_cell = bot_squad_get_attack_target_cell(state, bot, unreserved_army);
        int squad_id = bot_add_squad(bot, {
            .type = BOT_SQUAD_TYPE_ATTACK,
            .target_cell = counterattack_squad_target_cell,
            .entity_list = unreserved_army
        });
        log_debug("BOT %u defend_location, counterattack %u with squad %u", bot.player_id, unreserved_army.size(), squad_id);
        return;
    }

    // Finally, if we're really desperate, use workers to attack
    EntityList workers = match_find_entities(state, [&state, &bot, &location](const Entity& entity, EntityId /*entity_id*/) {
        return entity.type == ENTITY_MINER &&
            entity_can_be_given_orders(state, entity) &&
            entity.player_id == bot.player_id &&
            !entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED) &&
            ivec2::manhattan_distance(entity.cell, location) < BOT_NEAR_DISTANCE;
    });
    if (should_defend_with_workers && !workers.empty()) {
        int squad_id = bot_add_squad(bot, {
            .type = BOT_SQUAD_TYPE_RESERVES,
            .target_cell = location,
            .entity_list = workers
        });
        log_debug("BOT %u defend_location, worker defense, squad %u", bot.player_id, squad_id);
        return;
    }
}

int bot_score_entities_at_location(const MatchState* state, const Bot& bot, ivec2 location, std::function<bool(const Entity& entity, EntityId entity_id)> filter) {
    int score = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        EntityId entity_id = state->entities.get_id_of(entity_index);

        if (!filter(entity, entity_id)) {
            continue;
        }

        ivec2 entity_cell = entity.cell;
        if (!match_is_target_invalid(state, entity.target, entity.player_id)) {
            entity_cell = entity_get_target_cell(state, entity);
        }
        if (ivec2::manhattan_distance(entity_cell, location) > BOT_MEDIUM_DISTANCE) {
            continue;
        }

        score += bot_score_entity(state, bot, entity);
    }

    return score;
}

bool bot_should_attack(const MatchState* state, const Bot& bot) {
    // If we're not configured to attack, then don't
    if (!bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_ATTACK)) {
        return false;
    }

    // If there are no active opponents, then don't attack
    bool has_active_opponents = false;
    for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
        if (state->players[player_id].active && 
                state->players[player_id].team !=
                state->players[bot.player_id].team) {
            has_active_opponents = true;
        }
    }
    if (!has_active_opponents) {
        return false;
    }

    // Don't attack unless player has attacked us first
    if (!bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_ATTACK_FIRST) &&
            !bitflag_check(bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_ATTACKED)) {
        return false;
    }

    // Don't attack if we have no units to attack with
    int unreserved_army_score = bot_score_unreserved_army(state, bot);
    if (unreserved_army_score == 0) {
        return false;
    }

    // Don't attack if there are no bases to attack
    uint32_t least_defended_enemy_base_info_index = bot_get_least_defended_enemy_base_info_index(state, bot);
    if (least_defended_enemy_base_info_index == INDEX_INVALID) {
        return false;
    }

    // Attack if we are maxed out
    if (match_get_player_population(state, bot.player_id) >= MATCH_MAX_POPULATION - 2) {
        return true;
    }

    // Attack if there are no more bases left to take
    if (bot_should_all_in(bot)) {
        return true;
    }

    // Attack if we have a bigger army than our opponent
    const int least_defended_base_score = bot.base_info[least_defended_enemy_base_info_index].defense_score;
    const int minimum_attack_threshold = 
        least_defended_base_score < 4 * BOT_UNIT_SCORE_IN_BUNKER && 
        bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_HARASS)
            ? 4 * BOT_UNIT_SCORE_IN_BUNKER
            : 32 * BOT_UNIT_SCORE;
    const int attack_threshold = std::max(least_defended_base_score + (BOT_UNIT_SCORE * 4), minimum_attack_threshold);
    return unreserved_army_score > attack_threshold && 
            bot_score_allied_army(state, bot) > bot_score_enemy_army(state, bot);
}

bool bot_should_all_in(const Bot& bot) {
    return bot_get_player_mining_base_count(bot, bot.player_id) == 0 &&
        !bot_is_unoccupied_goldmine_available(bot);
}

// PRODUCTION

MatchInput bot_get_production_input(const MatchState* state, Bot& bot, uint32_t match_timer) {
    ZoneScoped;
    
    // Saturate bases
    MatchInput saturate_bases_input = bot_saturate_bases(state, bot);
    if (saturate_bases_input.type != MATCH_INPUT_NONE) {
        return saturate_bases_input;
    }

    // Build house
    if (bot_should_build_house(state, bot) && !bot_is_under_attack(bot)) {
        return bot_build_building(state, bot, ENTITY_HOUSE);
    }

    // Count entities
    EntityCount in_progress_entity_count = bot_count_in_progress_entities(state, bot);
    EntityCount available_building_count = bot_count_available_production_buildings(state, bot);
    EntityCount unreserved_and_in_progress_entity_count = bot_count_unreserved_entities(state, bot).add(in_progress_entity_count);

    // Expand
    if (bot_should_expand(state, bot) && 
            !bot_is_under_attack(bot) &&
            in_progress_entity_count[ENTITY_HALL] == 0) {
        EntityId goldmine_id = bot_find_goldmine_for_next_expansion(state, bot);
        const Entity& goldmine = state->entities.get_by_id(goldmine_id);
        if (bot_is_area_safe(state, bot, goldmine.cell)) {
            return bot_build_building(state, bot, ENTITY_HALL);
        } 
    } 

    // Research upgrades
    const uint32_t desired_upgrade = bot_get_desired_upgrade(state, bot, unreserved_and_in_progress_entity_count);
    if (desired_upgrade != 0) {
        EntityType building_type = bot_get_building_which_researches(desired_upgrade);
        if (available_building_count[building_type] != 0) {
            log_debug("BOT production returned research upgrade input");
            return bot_research_upgrade(state, bot, desired_upgrade);
        }
    }

    // Get desired production
    EntityCount desired_entities = bot.desired_buildings;
    for (uint32_t desired_squad_index = 0; desired_squad_index < bot.desired_squads.size(); desired_squad_index++) {
        const BotDesiredSquad& desired_squad = bot.desired_squads[desired_squad_index];
        desired_entities = desired_entities.add(desired_squad.entity_count);
    }

    // Train units
    const bool bot_desires_tech = !unreserved_and_in_progress_entity_count.is_gte_to(bot.desired_buildings);
    const bool bot_prefer_tech_over_units = bot.unit_comp != BOT_UNIT_COMP_COWBOY_BANDIT &&
            bot_desires_tech && in_progress_entity_count.select(bot_entity_types_production_buildings()).is_empty();
    if (bot_has_building_available_to_train_units(bot, desired_entities, available_building_count, unreserved_and_in_progress_entity_count) &&
            !bot_prefer_tech_over_units && 
            match_timer > bot.macro_cycle_timer) {
        return bot_train_unit(state, bot, bot_get_unit_type_to_train(bot, desired_entities, available_building_count, unreserved_and_in_progress_entity_count), match_timer);
    }

    // Determine other buildings based on upgrade pre-reqs
    if (desired_upgrade != 0) {
        EntityType building_type = bot_get_building_which_researches(desired_upgrade);
        if (unreserved_and_in_progress_entity_count[building_type] == 0 && desired_entities[building_type] == 0) {
            desired_entities[building_type] = 1;
        }
    }

    // Determine any other desired buildings based on building pre-reqs
    for (uint32_t entity_type = ENTITY_HALL; entity_type < ENTITY_LANDMINE; entity_type++) {
        if (desired_entities[entity_type] == 0) {
            continue;
        }

        EntityType prereq = bot_get_building_prereq((EntityType)entity_type);
        while (prereq != ENTITY_TYPE_COUNT && desired_entities[prereq] == 0) {
            desired_entities[prereq] = 1;
            prereq = bot_get_building_prereq((EntityType)prereq);
        }
    }

    // Disable any buildings that aren't allowed
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (!bot.config.is_entity_allowed[entity_type]) {
            desired_entities[entity_type] = 0;
        }
    }

    // Build buildings
    if (!bot_is_under_attack(bot)) {
        for (uint32_t building_type = ENTITY_HALL; building_type < ENTITY_TYPE_COUNT; building_type++) {
            if (desired_entities[building_type] > unreserved_and_in_progress_entity_count[building_type]) {
                return bot_build_building(state, bot, (EntityType)building_type);
            }
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

void bot_set_unit_comp(Bot& bot, BotUnitComp unit_comp) {
    bot.unit_comp = unit_comp;
    bot_update_desired_production(bot);
    log_debug("BOT %u set_unit_comp %u", bot.player_id, bot.unit_comp);
}

void bot_update_desired_production(Bot& bot) {
    const uint32_t mining_base_count = bot_get_player_mining_base_count(bot, bot.player_id) - bot_get_low_on_gold_base_count(bot);
    bot.desired_buildings.clear();
    bot.desired_army_ratio.clear();

    switch (bot.unit_comp) {
        case BOT_UNIT_COMP_NONE: {
            // We still want 2 buildings for the opener
            bot.desired_buildings[ENTITY_SALOON] = 2;
            
            break;
        }
        case BOT_UNIT_COMP_COWBOY_BANDIT: {
            bot.desired_buildings[ENTITY_SALOON] = 2 * mining_base_count;

            bot.desired_army_ratio[ENTITY_COWBOY] = 2;
            bot.desired_army_ratio[ENTITY_BANDIT] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_BANDIT_PYRO: {
            bot.desired_buildings[ENTITY_SALOON] = 2 * mining_base_count;
            bot.desired_buildings[ENTITY_WORKSHOP] = 1;

            bot.desired_army_ratio[ENTITY_COWBOY] = 4;
            bot.desired_army_ratio[ENTITY_BANDIT] = 4;
            bot.desired_army_ratio[ENTITY_PYRO] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_SAPPER_PYRO: {
            bot.desired_buildings[ENTITY_SALOON] = mining_base_count + 1;
            bot.desired_buildings[ENTITY_WORKSHOP] = mining_base_count;

            bot.desired_army_ratio[ENTITY_COWBOY] = 3;
            bot.desired_army_ratio[ENTITY_SAPPER] = 2;
            bot.desired_army_ratio[ENTITY_PYRO] = 1;
            break;
        }
        case BOT_UNIT_COMP_COWBOY_WAGON: {
            bot.desired_buildings[ENTITY_SALOON] = 2 * mining_base_count;
            bot.desired_buildings[ENTITY_COOP] = mining_base_count;

            bot.desired_army_ratio[ENTITY_COWBOY] = 4;
            bot.desired_army_ratio[ENTITY_WAGON] = 1;
            break;
        }
        case BOT_UNIT_COMP_SOLDIER_BANDIT: {
            bot.desired_buildings[ENTITY_SALOON] = mining_base_count;
            bot.desired_buildings[ENTITY_BARRACKS] = mining_base_count;

            bot.desired_army_ratio[ENTITY_SOLDIER] = 1;
            bot.desired_army_ratio[ENTITY_BANDIT] = 1;
            break;
        }
        case BOT_UNIT_COMP_SOLDIER_CANNON: {
            bot.desired_buildings[ENTITY_BARRACKS] = 2 * mining_base_count;

            bot.desired_army_ratio[ENTITY_SOLDIER] = 8;
            bot.desired_army_ratio[ENTITY_CANNON] = 1;
            break;
        }
        case BOT_UNIT_COMP_COUNT: {
            GOLD_ASSERT(false);
            break;
        }
    }

    // If enemy has invisible units and we're not using a pyro-based unit comp, then get balloons!
    const bool enemy_has_invisible_units = 
        bitflag_check(bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES) ||
        bitflag_check(bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES);
    if (enemy_has_invisible_units && bot.desired_army_ratio[ENTITY_PYRO] == 0) {
        bot.desired_buildings[ENTITY_WORKSHOP] = 1;
        bot.desired_army_ratio[ENTITY_BALLOON] = 1;
    }

    // Disable anything that isn't allowed
    for (uint32_t entity_type = 0; entity_type < ENTITY_TYPE_COUNT; entity_type++) {
        if (!bot.config.is_entity_allowed[entity_type]) {
            bot.desired_buildings[entity_type] = 0;
            bot.desired_army_ratio[entity_type] = 0;
        }
    }
}

// SATURATE BASES

MatchInput bot_saturate_bases(const MatchState* state, Bot& bot) {
    for (uint32_t goldmine_index = 0; goldmine_index < state->entities.size(); goldmine_index++) {
        const Entity& goldmine = state->entities[goldmine_index];
        EntityId goldmine_id = state->entities.get_id_of(goldmine_index);
        if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
            continue;
        }

        EntityId hall_id = match_find_entity(state, [&goldmine, &bot](const Entity& hall, EntityId /*hall_id*/) {
            return hall.player_id == bot.player_id &&
                    hall.type == ENTITY_HALL &&
                    hall.mode == MODE_BUILDING_FINISHED &&
                    bot_does_entity_surround_goldmine(hall, goldmine.cell);
        });

        // If there is not hall, tell any miners to stop
        // This handles the case that the town hall is destroyed
        if (hall_id == ID_NULL) {
            MatchInput input;
            input.type = MATCH_INPUT_STOP;
            input.stop.entity_count = 0;

            for (uint32_t miner_index = 0; miner_index < state->entities.size(); miner_index++) {
                const Entity& miner = state->entities[miner_index];
                EntityId miner_id = state->entities.get_id_of(miner_index);
                if (miner.player_id == bot.player_id && 
                        miner.goldmine_id == goldmine_id && 
                        miner_id != bot.scout_id &&
                        entity_is_mining(state, miner)) {
                    input.stop.entity_ids[input.stop.entity_count] = state->entities.get_id_of(miner_index);
                    input.stop.entity_count++;
                }
            }

            if (input.stop.entity_count != 0) {
                GOLD_ASSERT(input.stop.entity_count <= SELECTION_LIMIT);
                log_debug("BOT %u saturate_bases, tell miners to stop mining.", bot.player_id);
                return input;
            }

            continue;
        }

        // Check the miner count
        const Entity& hall = state->entities.get_by_id(hall_id);
        uint32_t miner_count = match_get_miners_on_gold(state, goldmine_id, bot.player_id);

        // If oversaturated, pull workers off gold
        if (miner_count > MATCH_MAX_MINERS_ON_GOLD) {
            EntityId miner_id = match_find_entity(state, [&state, &bot, &goldmine_id](const Entity& entity, EntityId /*entity_id*/) {
                return entity.player_id == bot.player_id &&
                        entity_is_selectable(entity) &&
                        entity_is_mining(state, entity) &&
                        entity.goldmine_id == goldmine_id;
            });
            if (miner_id == ID_NULL) {
                log_warn("BOT %u saturate_bases, tried to pull workers off gold but no worker found.", bot.player_id);
                continue; // continue to the next goldmine
            }

            ivec2 goldmine_cell = state->entities.get_by_id(goldmine_id).cell;
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_CELL;
            input.move.shift_command = 0;
            input.move.target_cell = bot_get_position_near_hall_away_from_miners(state, hall.cell, goldmine_cell);
            input.move.target_id = ID_NULL;
            input.move.entity_count = 1;
            input.move.entity_ids[0] = miner_id;

            log_debug("BOT %u saturate_bases, pull worker off gold.", bot.player_id);
            return input;
        }

        // If undersaturated and we have an idle worker, put the worker on gold
        EntityId idle_worker_id = bot_find_nearest_idle_worker(state, bot, hall.cell);
        if (miner_count < MATCH_MAX_MINERS_ON_GOLD && idle_worker_id != ID_NULL) {
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_ENTITY;
            input.move.shift_command = 0;
            input.move.target_cell = ivec2(0, 0);
            input.move.target_id = goldmine_id;
            input.move.entity_count = 1;
            input.move.entity_ids[0] = idle_worker_id;

            log_debug("BOT %u saturate_bases, put worker on gold.", bot.player_id);
            return input;
        }

        // If undersatured and no idle worker, make a new worker
        if (miner_count < MATCH_MAX_MINERS_ON_GOLD && 
                state->players[bot.player_id].gold >= entity_get_data(ENTITY_MINER).gold_cost &&
                hall.queue.empty()) {
            if (hall.rally_point.x == -1) {
                bot_queue_set_building_rally_point(bot, hall_id);
            }

            MatchInput input;
            input.type = MATCH_INPUT_BUILDING_ENQUEUE;
            input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
            input.building_enqueue.item_subtype = (uint32_t)ENTITY_MINER;
            input.building_enqueue.building_count = 1;
            input.building_enqueue.building_ids[0] = hall_id;

            log_debug("BOT %u saturate_bases, train new worker.", bot.player_id);
            return input;
        }

        // If saturated, cancel any in-progress miners
        if (miner_count == MATCH_MAX_MINERS_ON_GOLD && !hall.queue.empty()) {
            MatchInput input;
            input.type = MATCH_INPUT_BUILDING_DEQUEUE;
            input.building_dequeue.building_id = hall_id;
            input.building_dequeue.index = BUILDING_DEQUEUE_POP_FRONT;

            log_debug("BOT %u saturate_bases, cancel in-progress worker.", bot.player_id);
            return input;
        }
    
        // Scramble stuck workers
        EntityId stuck_worker_id = match_find_entity(state, [&state, &bot, &goldmine_id](const Entity& entity, EntityId /*entity_id*/) {
            if (entity.player_id != bot.player_id ||
                    !entity_is_mining(state, entity) ||
                    entity.goldmine_id != goldmine_id ||
                    entity.mode != MODE_UNIT_BLOCKED) {
                return false;
            }
            ivec2 facing_cell = entity.cell + DIRECTION_IVEC2[entity.direction];
            if (!map_is_cell_in_bounds(state->map, facing_cell)) {
                return false;
            }
            Cell map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, facing_cell);
            if (!(map_cell.type == CELL_MINER || map_cell.type == CELL_UNIT)) {
                return false;
            }
            const Entity& other_miner = state->entities.get_by_id(map_cell.id);
            return other_miner.mode == MODE_UNIT_BLOCKED &&
                    other_miner.direction == (entity.direction + 4) % DIRECTION_COUNT;
        });
        if (stuck_worker_id != ID_NULL) {
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_CELL;
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;
            input.move.target_cell = bot_get_position_near_hall_away_from_miners(state, hall.cell, goldmine.cell);
            input.move.entity_count = 1;
            input.move.entity_ids[0] = stuck_worker_id;

            log_debug("BOT %u saturate bases, scramble stuck worker.", bot.player_id);
            return input;
        }

    } // End for each goldmine

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

ivec2 bot_get_position_near_hall_away_from_miners(const MatchState* state, ivec2 hall_cell, ivec2 goldmine_cell) {
    // Determine adjacent direction from hall to landmine
    // If direction is diagonal, prefer north / south because enum_direction_to_rect 
    // returns those whenever the y distance is bigger
    Direction direction_from_hall_to_mine = enum_direction_to_rect(hall_cell + ivec2(1, 1), goldmine_cell, 3);
    if (direction_from_hall_to_mine == DIRECTION_SOUTHWEST || direction_from_hall_to_mine == DIRECTION_SOUTHEAST) {
        direction_from_hall_to_mine = DIRECTION_SOUTH;
    } else if (direction_from_hall_to_mine == DIRECTION_NORTHWEST || direction_from_hall_to_mine == DIRECTION_NORTHEAST) {
        direction_from_hall_to_mine = DIRECTION_NORTH;
    }
    GOLD_ASSERT(direction_from_hall_to_mine % 2 == 0);
    
    // Determine direction that is adjacent to the goldmine path
    Direction direction_adjacent_to_goldmine_path = (Direction)((direction_from_hall_to_mine + 2) % DIRECTION_COUNT);

    // If direction is taking us close to the edge of the map, reverse it
    static const int HALL_MAP_EDGE_MARGIN = 32;
    if (direction_adjacent_to_goldmine_path == DIRECTION_SOUTH && hall_cell.y >= state->map.height - HALL_MAP_EDGE_MARGIN) {
        direction_adjacent_to_goldmine_path = DIRECTION_NORTH;
    } else if (direction_adjacent_to_goldmine_path == DIRECTION_NORTH && hall_cell.y < HALL_MAP_EDGE_MARGIN) {
        direction_adjacent_to_goldmine_path = DIRECTION_SOUTH;
    } else if (direction_adjacent_to_goldmine_path == DIRECTION_EAST && hall_cell.x >= state->map.width - HALL_MAP_EDGE_MARGIN) {
        direction_adjacent_to_goldmine_path = DIRECTION_WEST;
    } else if (direction_adjacent_to_goldmine_path == DIRECTION_WEST && hall_cell.x < HALL_MAP_EDGE_MARGIN) {
        direction_adjacent_to_goldmine_path = DIRECTION_EAST;
    }

    return map_clamp_cell(state->map, hall_cell + (DIRECTION_IVEC2[direction_adjacent_to_goldmine_path] * 8));
}

EntityId bot_find_nearest_idle_worker(const MatchState* state, const Bot& bot, ivec2 cell) {
    return match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [&bot](const Entity& entity, EntityId entity_id) {
            return entity.type == ENTITY_MINER && 
                    entity.player_id == bot.player_id &&
                    ((entity.mode == MODE_UNIT_IDLE &&
                        entity.target.type == TARGET_NONE) ||
                     (entity.mode == MODE_UNIT_MOVE &&
                        entity.target.type == TARGET_CELL)) &&
                    entity_id != bot.scout_id &&
                    !entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED);
        },
        .compare = match_compare_closest_manhattan_distance_to(cell)
    });
}

// BUILD BUILDINGS

bool bot_should_build_house(const MatchState* state, const Bot& bot) {
    uint32_t future_max_population = match_get_player_max_population(state, bot.player_id);
    uint32_t future_population = match_get_player_population(state, bot.player_id);
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id != bot.player_id || !entity_is_selectable(entity)) {
            continue;
        }

        if ((entity.type == ENTITY_HALL || entity.type == ENTITY_HOUSE) && entity.mode == MODE_BUILDING_IN_PROGRESS) {
            future_max_population += 10;
        } else if (entity.mode == MODE_BUILDING_FINISHED && !entity.queue.empty() && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) {
            future_population += entity_get_data(entity.queue[0].unit_type).unit_data.population_cost;
        } else if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD && 
                        (entity.target.build.building_type == ENTITY_HOUSE || entity.target.build.building_type == ENTITY_HALL)) {
            future_max_population += 10;
        }
    }

    return future_max_population < MATCH_MAX_POPULATION && (int)future_max_population - (int)future_population <= 1;
}

MatchInput bot_build_building(const MatchState* state, Bot& bot, EntityType building_type) {
    GOLD_ASSERT(building_type != ENTITY_TYPE_COUNT && entity_is_building(building_type));

    // Check pre-req
    EntityType prereq_type = bot_get_building_prereq(building_type);
    if (prereq_type != ENTITY_TYPE_COUNT) {
        EntityId prereq_id = match_find_entity(state, [&bot, &prereq_type](const Entity& entity, EntityId /*entity_id*/) {
            return entity.player_id == bot.player_id && entity.mode == MODE_BUILDING_FINISHED && entity.type == prereq_type;
        });
        if (prereq_id == ID_NULL) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
    }

    // Check gold cost
    if (state->players[bot.player_id].gold < entity_get_data(building_type).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id, building_type == ENTITY_BUNKER);
    log_debug("BOT %u build_building, hall index with least nearby buildings %u", bot.player_id, hall_index);
    if (hall_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId builder_id = bot_find_builder(state, bot, state->entities[hall_index].cell);
    if (builder_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 building_location; 
    if (building_type == ENTITY_HALL) {
        building_location = bot_find_hall_location(state, bot);
    } else if (building_type == ENTITY_BUNKER) {
        building_location = bot_find_bunker_location(state, bot, hall_index);
    } else {
        building_location = bot_find_building_location(state, state->entities[hall_index].cell + ivec2(1, 1), entity_get_data(building_type).cell_size);
    }
    if (building_location.x == -1) {
        log_warn("BOT %u: building location not found for building type %s", bot.player_id, entity_get_data(building_type).name);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    log_debug("BOT %u get_production_input, build building type %s location <%i, %i>.", bot.player_id, entity_get_data(building_type).name, building_location.x, building_location.y);
    MatchInput input;
    input.type = MATCH_INPUT_BUILD;
    input.build.shift_command = 0;
    input.build.target_cell = building_location;
    input.build.building_type = building_type;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = builder_id;
    return input;
}

bool bot_is_building_location_valid(const MatchState* state, ivec2 cell, int size) {
    GOLD_ASSERT(map_is_cell_rect_in_bounds(state->map, cell, size));

    const int MARGIN = 1;
    uint32_t cell_elevation = map_get_tile(state->map, cell).elevation;
    for (int y = cell.y - MARGIN; y < cell.y + size + MARGIN; y++) {
        for (int x = cell.x - MARGIN; x < cell.x + + size + MARGIN; x++) {
            ivec2 neighbor = ivec2(x, y);
            if (!map_is_cell_in_bounds(state->map, neighbor)) {
                continue;
            }

            if (map_get_cell(state->map, CELL_LAYER_GROUND, neighbor).type != CELL_EMPTY) {
                return false;
            }
            if (map_get_tile(state->map, neighbor).elevation != cell_elevation) {
                return false;
            }
            if (map_is_tile_ramp(state->map, neighbor)) {
                return false;
            }
        }
    }

    return true;
}

ivec2 bot_find_building_location(const MatchState* state, ivec2 start_cell, int size) {
    std::vector<ivec2> frontier = { start_cell };
    std::vector<bool> is_explored = std::vector<bool>(state->map.width * state->map.height, false);

    while (!frontier.empty()) {
        uint32_t nearest_index = 0;
        for (uint32_t frontier_index = 1; frontier_index < frontier.size(); frontier_index++) {
            if (ivec2::manhattan_distance(frontier[frontier_index], start_cell) < 
                    ivec2::manhattan_distance(frontier[nearest_index], start_cell)) {
                nearest_index = frontier_index;
            }
        }
        ivec2 nearest = frontier[nearest_index];
        frontier[nearest_index] = frontier.back();
        frontier.pop_back();

        if (bot_is_building_location_valid(state, nearest, size)) {
            return nearest;
        }

        is_explored[nearest.x + (nearest.y * state->map.width)] = true;

        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = nearest + DIRECTION_IVEC2[direction];
            if (!map_is_cell_rect_in_bounds(state->map, child, size)) {
                continue;
            }

            if (is_explored[child.x + (child.y * state->map.width)]) {
                continue;
            }

            bool is_in_frontier = false;
            for (ivec2 cell : frontier) {
                if (cell == child) {
                    is_in_frontier = true;
                    break;
                }
            }
            if (is_in_frontier) {
                continue;
            }

            frontier.push_back(child);
        } // End for each direction
    } // End while not frontier empty

    // If we got here it means we didn't find anywhere to build
    // Would be crazy if that happened
    return ivec2(-1, -1);
}

uint32_t bot_find_hall_index_with_least_nearby_buildings(const MatchState* state, uint8_t bot_player_id, bool count_bunkers_only) {
    // First find all the town halls for this bot
    std::vector<uint32_t> hall_indices;
    std::vector<uint32_t> buildings_near_hall;
    for (uint32_t hall_index = 0; hall_index < state->entities.size(); hall_index++) {
        const Entity& hall = state->entities[hall_index];
        if (hall.player_id != bot_player_id || hall.type != ENTITY_HALL || hall.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        hall_indices.push_back(hall_index);
        buildings_near_hall.push_back(0);
    }

    // Return INDEX_INVALID if the bot has no halls
    if (hall_indices.empty()) {
        return INDEX_INVALID;
    }

    // Then find all the houses for this bot and determine which hall they are closest to
    for (uint32_t building_index = 0; building_index < state->entities.size(); building_index++) {
        const Entity& building = state->entities[building_index];
        if (building.player_id != bot_player_id || !entity_is_building(building.type) || !entity_is_selectable(building)) {
            continue;
        }
        if (count_bunkers_only && building.type != ENTITY_BUNKER) {
            continue;
        }

        uint32_t nearest_hall_indicies_index = 0;
        for (uint32_t hall_indices_index = 1; hall_indices_index < hall_indices.size(); hall_indices_index++) {
            if (ivec2::manhattan_distance(building.cell, state->entities[hall_indices[hall_indices_index]].cell) <
                    ivec2::manhattan_distance(building.cell, state->entities[hall_indices[nearest_hall_indicies_index]].cell)) {
                nearest_hall_indicies_index = hall_indices_index;
            }
        }

        buildings_near_hall[nearest_hall_indicies_index]++;
    }

    // Finally choose the hall with the least houses
    uint32_t least_buildings_hall_indices_index = 0;
    for (uint32_t hall_indices_index = 1; hall_indices_index < hall_indices.size(); hall_indices_index++) {
        if (buildings_near_hall[hall_indices_index] < buildings_near_hall[least_buildings_hall_indices_index]) {
            least_buildings_hall_indices_index = hall_indices_index;
        }
    }

    return hall_indices[least_buildings_hall_indices_index];
}

ivec2 bot_find_hall_location(const MatchState* state, const Bot& bot) {
    EntityId goldmine_id = bot_find_goldmine_for_next_expansion(state, bot);
    const Entity& goldmine = state->entities.get_by_id(goldmine_id);

    // To find the hall location, we will search around the perimeter
    // of the building_block_rect for this goldmine and evaluate each 
    // placement, choosing the one with the least obstacles nearby
    Rect building_block_rect = entity_goldmine_get_block_building_rect(goldmine.cell);
    Rect goldmine_rect = (Rect) {
        .x = goldmine.cell.x,
        .y = goldmine.cell.y,
        .w = entity_get_data(ENTITY_GOLDMINE).cell_size,
        .h = entity_get_data(ENTITY_GOLDMINE).cell_size
    };
    const int HALL_SIZE = entity_get_data(ENTITY_HALL).cell_size;
    ivec2 corners[4] = { 
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(-HALL_SIZE, -HALL_SIZE),
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(-HALL_SIZE, building_block_rect.h), 
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(building_block_rect.w, building_block_rect.h), 
        ivec2(building_block_rect.x, building_block_rect.y) + ivec2(building_block_rect.w, -HALL_SIZE) 
    };
    ivec2 directions[4] = {
        DIRECTION_IVEC2[DIRECTION_SOUTH],
        DIRECTION_IVEC2[DIRECTION_EAST],
        DIRECTION_IVEC2[DIRECTION_NORTH],
        DIRECTION_IVEC2[DIRECTION_WEST]
    };
    ivec2 best_hall_cell = ivec2(-1, -1);
    int best_hall_score = -1;

    ivec2 hall_cell = corners[0];
    ivec2 hall_step = directions[0];
    while (true) {
        // Evaluate hall cell score
        int hall_score = 0;
        // Add the hall distance to the score so that we prefer less diagonal base placement
        Rect hall_rect = (Rect) {
            .x = hall_cell.x, .y = hall_cell.y,
            .w = HALL_SIZE, .h = HALL_SIZE
        };
        // If the area is blocked (by a cactus, for example) then don't build there
        if (!map_is_cell_rect_in_bounds(state->map, hall_cell, HALL_SIZE) || map_is_cell_rect_occupied(state->map, CELL_LAYER_GROUND, hall_cell, HALL_SIZE) ||
                map_get_tile(state->map, hall_cell).elevation != map_get_tile(state->map, goldmine.cell).elevation) {
            hall_score = -1;
        } else {
            // Check for obstacles (including stairs) in a rect around where the hall would be
            // The more obstacles there are, the higher the score
            hall_score = 0;
            Rect hall_surround_rect = (Rect) {
                .x = hall_cell.x - 2,
                .y = hall_cell.y - 2,
                .w = HALL_SIZE + 4,
                .h = HALL_SIZE + 4
            };
            for (int y = hall_surround_rect.y; y < hall_surround_rect.y + hall_surround_rect.h; y++) {
                for (int x = hall_surround_rect.x; x < hall_surround_rect.x + hall_surround_rect.w; x++) {
                    if (!map_is_cell_in_bounds(state->map, ivec2(x, y))) {
                        continue;
                    }
                    if (map_is_tile_ramp(state->map, ivec2(x, y))) {
                        hall_score++;
                    } 
                }
            }

            MapPath path;
            map_get_ideal_mine_exit_path(state->map, goldmine.cell, hall_cell, &path);
            hall_score += path.size();
            hall_score += Rect::euclidean_distance_squared_between(hall_rect, goldmine_rect);
        }

        // Compare hall score to best hall score
        if (hall_score != -1 && (best_hall_score == -1 || hall_score < best_hall_score))  {
            best_hall_cell = hall_cell;
            best_hall_score = hall_score;
        }

        // Increment perimeter search
        hall_cell += hall_step;
        int corner = 0;
        while (corner < 4) {
            if (hall_cell == corners[corner]) {
                break;
            } 
            corner++;
        }

        // Determine whether to end the search or change the search direction
        if (corner == 0) {
            break;
        } else if (corner < 4) {
            hall_step = directions[corner];
        }
    } // End for each cell in perimeter

    // Implicit, we will return ivec2(-1, -1) if there was no good hall cell
    // found in the search above, but the only reason that would happen is 
    // if we had a really bad map gen bug or the player surrounded the
    // goldmine with units 
    return best_hall_cell;
}

EntityId bot_find_goldmine_for_next_expansion(const MatchState* state, const Bot& bot) {
    // If we have a base that is missing a hall, rebuild the hall there
    EntityId goldmine_missing_hall_id;
    if (bot_has_base_that_is_missing_a_hall(bot, &goldmine_missing_hall_id)) {
        return goldmine_missing_hall_id;
    }

    // Choose the reference entity
    // This will either be the hall with the least nearby buildings
    // Or, if no hall is found, any random bot-controlled building 
    uint32_t hall_index = bot_find_hall_index_with_least_nearby_buildings(state, bot.player_id, false);
    EntityId reference_entity_id = hall_index != INDEX_INVALID
        ? state->entities.get_id_of(hall_index)
        : match_find_entity(state, [&bot](const Entity& building, EntityId /*building_id*/) {
            return building.player_id == bot.player_id;
        });
    GOLD_ASSERT(reference_entity_id != ID_NULL);

    return bot_find_unoccupied_goldmine_nearest_to_entity(state, bot, reference_entity_id);
}

EntityId bot_find_unoccupied_goldmine_nearest_to_entity(const MatchState* state, const Bot& bot, EntityId reference_entity_id) {
    const Entity& reference_entity = state->entities.get_by_id(reference_entity_id);

    // Find the unoccupied goldmine closest to this allied building
    uint32_t nearest_goldmine_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player != PLAYER_NONE || !base_info.has_gold) {
            continue;
        }

        if (nearest_goldmine_base_info_index == INDEX_INVALID ||
                ivec2::manhattan_distance(state->entities.get_by_id(base_info.goldmine_id).cell, reference_entity.cell) <
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_goldmine_base_info_index].goldmine_id).cell, reference_entity.cell)) {
            nearest_goldmine_base_info_index = base_info_index;
        }
    }

    return nearest_goldmine_base_info_index;
}

EntityId bot_find_builder(const MatchState* state, const Bot& bot, ivec2 near_cell) {
    // First try an idle worker
    EntityId builder = bot_find_nearest_idle_worker(state, bot, near_cell);
    if (builder != ID_NULL) {
        return builder;
    }

    // Then try a worker from the gold mine
    return match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [&state, &bot](const Entity& entity, EntityId /*entity_id*/) {
            return entity.player_id == bot.player_id &&
                    entity_is_selectable(entity) &&
                    entity_is_mining(state, entity);
        },
        .compare = match_compare_closest_manhattan_distance_to(near_cell)
    });
}

ivec2 bot_find_bunker_location(const MatchState* state, const Bot& bot, uint32_t nearby_hall_index) {
    const Entity& nearby_hall = state->entities[nearby_hall_index];
    ivec2 nearby_hall_cell = nearby_hall.cell;
    EntityId nearest_enemy_building_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [&state, &bot](const Entity& building, EntityId /*entity_id*/) {
            return entity_is_building(building.type) &&
                    entity_is_selectable(building) &&
                    state->players[building.player_id].team != state->players[bot.player_id].team;
        },
        .compare = [&nearby_hall_cell](const Entity& a, const Entity& b) {
            if (a.type == ENTITY_HALL && b.type != ENTITY_HALL) {
                return true;
            }
            if (a.type != ENTITY_HALL && b.type == ENTITY_HALL) {
                return false;
            }
            return ivec2::manhattan_distance(a.cell, nearby_hall_cell) < ivec2::manhattan_distance(b.cell, nearby_hall_cell);
        }
    });

    // If there are no enemy buildings, then it doesn't matter where we put the bunker
    if (nearest_enemy_building_id == ID_NULL) {
        return bot_find_building_location(state, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }

    const Entity& nearest_enemy_building = state->entities.get_by_id(nearest_enemy_building_id);
    ivec2 path_start_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, nearby_hall_cell, entity_get_data(ENTITY_HALL).cell_size, 1, nearest_enemy_building.cell, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_building.cell, entity_get_data(nearest_enemy_building.type).cell_size, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    if (path_start_cell.x == -1 || path_end_cell.x == -1) {
        return bot_find_building_location(state, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }
    MapPath path;
    map_pathfind(state->map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS, NULL, &path);

    if (path.empty()) {
        return bot_find_building_location(state, nearby_hall_cell, entity_get_data(ENTITY_BUNKER).cell_size);
    }

    ivec2 search_start_cell = path_start_cell;
    uint32_t path_index = 0;
    while (path_index < path.size() &&
            ivec2::manhattan_distance(path.get_from_top(path_index), path_start_cell) < 17 &&
            map_get_tile(state->map, path.get_from_top(path_index)).elevation == map_get_tile(state->map, path_start_cell).elevation) {
        path_index++;
    }
    if (path_index > 1 && path_index < path.size() && path_index < path.size()) {
        search_start_cell = path.get_from_top(path_index - 1);
    }

    return bot_find_building_location(state, search_start_cell, entity_get_data(ENTITY_BUNKER).cell_size);
}

// RESEARCH UPGRADES

uint32_t bot_get_desired_upgrade(const MatchState* state, const Bot& bot, EntityCount unreserved_and_in_progress_entity_count) {
    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_LANDMINES) &&
            bot_has_squad_of_type(bot, BOT_SQUAD_TYPE_LANDMINES) && 
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_LANDMINES)) {
        return UPGRADE_LANDMINES;
    }

    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_IRON_SIGHTS) &&
            bot.desired_army_ratio[ENTITY_COWBOY] != 0 &&
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_IRON_SIGHTS)) {
        return UPGRADE_IRON_SIGHTS;
    }

    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_PRIVATE_EYE) &&
            unreserved_and_in_progress_entity_count[ENTITY_SHERIFFS] &&
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_PRIVATE_EYE)) {
        return UPGRADE_PRIVATE_EYE;
    }

    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_BAYONETS) &&
            bot.unit_comp == BOT_UNIT_COMP_SOLDIER_CANNON &&
            unreserved_and_in_progress_entity_count[ENTITY_BARRACKS] != 0 &&
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_BAYONETS)) {
        return UPGRADE_BAYONETS;
    }

    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_WAR_WAGON) &&
            bot.desired_army_ratio[ENTITY_WAGON] != 0 &&
            unreserved_and_in_progress_entity_count[ENTITY_COOP] != 0 &&
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_WAR_WAGON)) {
        return UPGRADE_WAR_WAGON;
    }

    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_SERRATED_KNIVES) &&
            bot.desired_army_ratio[ENTITY_BANDIT] != 0 && 
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_SERRATED_KNIVES)) {
        return UPGRADE_SERRATED_KNIVES;
    }

    if (bitflag_check(bot.config.allowed_upgrades, UPGRADE_TAILWIND) &&
            bot.desired_army_ratio[ENTITY_BALLOON] != 0 && 
            unreserved_and_in_progress_entity_count[ENTITY_WORKSHOP] != 0 &&
            match_player_upgrade_is_available(state, bot.player_id, UPGRADE_TAILWIND)) {
        return UPGRADE_TAILWIND;
    }

    return 0;
}

MatchInput bot_research_upgrade(const MatchState* state, Bot& bot, uint32_t upgrade) {
    GOLD_ASSERT(match_player_upgrade_is_available(state, bot.player_id, upgrade));

    EntityType building_type = bot_get_building_which_researches(upgrade);
    EntityId building_id = match_find_entity(state, [&bot, &building_type](const Entity& entity, EntityId /*entity_id*/) {
        return entity.player_id == bot.player_id && 
                entity.type == building_type &&
                entity.mode == MODE_BUILDING_FINISHED &&
                entity.queue.empty();
    });
    if (building_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Check gold
    if (state->players[bot.player_id].gold < upgrade_get_data(upgrade).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    log_debug("BOT %u get_production_input, research upgrade.", bot.player_id);
    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UPGRADE;
    input.building_enqueue.item_subtype = upgrade;
    input.building_enqueue.building_count = 1;
    input.building_enqueue.building_ids[0] = building_id;
    return input;
}

// TRAIN UNITS

bool bot_has_building_available_to_train_units(const Bot& bot, EntityCount desired_entities, EntityCount available_building_count, EntityCount unreserved_and_in_progress_entity_count) {
    for (uint32_t unit_type = ENTITY_MINER + 1; unit_type < ENTITY_HALL; unit_type++) {
        const bool bot_desires_unit_type = 
            desired_entities[unit_type] > unreserved_and_in_progress_entity_count[unit_type] ||
            (bot.unit_comp != BOT_UNIT_COMP_NONE && bot.desired_army_ratio[unit_type] != 0);
        if (!bot_desires_unit_type) {
            continue;
        }

        EntityType building_type = bot_get_building_which_trains((EntityType)unit_type);
        if (available_building_count[building_type] != 0) {
            return true;
        }
    }

    return false;
}

EntityType bot_get_unit_type_to_train(Bot& bot, EntityCount desired_entities, EntityCount available_building_count, EntityCount unreserved_and_in_progress_entity_count) {
    while (true) {
        for (uint32_t unit_type = ENTITY_MINER + 1; unit_type < ENTITY_HALL; unit_type++) {
            if (desired_entities[unit_type] <= unreserved_and_in_progress_entity_count[unit_type]) {
                continue;
            }

            EntityType building_type = bot_get_building_which_trains((EntityType)unit_type);
            if (available_building_count[building_type] == 0) {
                continue;
            }

            return (EntityType)unit_type;
        }

        // If we didn't return an entity, then we should be using army ratio
        GOLD_ASSERT(!bot.desired_army_ratio.is_empty());

        desired_entities = desired_entities.add(bot.desired_army_ratio);
    }

    return ENTITY_TYPE_COUNT;
}

MatchInput bot_train_unit(const MatchState* state, Bot& bot, EntityType unit_type, uint32_t match_timer) {
    GOLD_ASSERT(unit_type != ENTITY_TYPE_COUNT && entity_is_unit(unit_type));

    // Find building to train unit
    EntityType building_type = bot_get_building_which_trains(unit_type);
    EntityId building_id = match_find_entity(state, [&bot, &building_type](const Entity& entity, EntityId /*entity_id*/) {
        return entity.player_id == bot.player_id && 
                entity.type == building_type &&
                entity.mode == MODE_BUILDING_FINISHED &&
                entity.queue.empty();
    });
    if (building_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Check gold
    if (state->players[bot.player_id].gold < entity_get_data(unit_type).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    bot_queue_set_building_rally_point(bot, building_id);

    if (bot.config.macro_cycle_cooldown != 0) {
        bot.macro_cycle_count++;
        if (bot.macro_cycle_count >= bot_get_player_mining_base_count(bot, bot.player_id) * 2) {
            bot.macro_cycle_timer = match_timer + bot.config.macro_cycle_cooldown;
            bot.macro_cycle_count = 0;
        }
    } 

    MatchInput input;
    input.type = MATCH_INPUT_BUILDING_ENQUEUE;
    input.building_enqueue.item_type = (uint8_t)BUILDING_QUEUE_ITEM_UNIT;
    input.building_enqueue.item_subtype = (uint32_t)unit_type;
    input.building_enqueue.building_count = 1;
    input.building_enqueue.building_ids[0] = building_id;
    return input;
}

// SQUADS

const char* bot_squad_type_str(BotSquadType type) {
    switch (type) {
        case BOT_SQUAD_TYPE_ATTACK:
            return "Attack";
        case BOT_SQUAD_TYPE_DEFEND:
            return "Defend";
        case BOT_SQUAD_TYPE_RESERVES:
            return "Reserves";
        case BOT_SQUAD_TYPE_LANDMINES:
            return "Landmines";
        case BOT_SQUAD_TYPE_RETURN:
            return "Return";
        case BOT_SQUAD_TYPE_PATROL:
            return "Patrol";
        case BOT_SQUAD_TYPE_HOLD_POSITION:
            return "Hold Position";
        case BOT_SQUAD_TYPE_COUNT:
            GOLD_ASSERT(false);
            return "";
    }
}

BotSquadType bot_squad_type_from_str(const char* str) {
    return (BotSquadType)enum_from_str(str, (EnumToStrFn)bot_squad_type_str, BOT_SQUAD_TYPE_COUNT);
}

int bot_add_squad(Bot& bot, BotAddSquadParams params) {
    GOLD_ASSERT(!params.entity_list.empty());
    if (params.entity_list.empty()) {
        log_warn("BOT %u squad_create, entity_list is empty.", bot.player_id);
        return BOT_SQUAD_ID_NULL;
    }

    if (bot.squads.is_full()) {
        log_warn("BOT %u squad_create, squads list is full.", bot.player_id);
        return BOT_SQUAD_ID_NULL;
    }

    BotSquad squad;
    squad.id = bot.next_squad_id;
    squad.type = params.type;
    squad.target_cell = params.target_cell;
    squad.patrol_cell = squad.type == BOT_SQUAD_TYPE_PATROL
        ? params.patrol_cell : ivec2(-1, -1);

    {
        char debug_buffer[1024];
        char* debug_ptr = debug_buffer;
        debug_ptr += sprintf(debug_ptr, "BOT %u squad_create, id %u type %s target cell <%i, %i> patrol cell <%i, %i> entities [", 
            bot.player_id, 
            squad.id,
            bot_squad_type_str(squad.type),
            squad.target_cell.x, squad.target_cell.y,
            squad.patrol_cell.x, squad.patrol_cell.y);
        for (uint32_t entity_list_index = 0; entity_list_index < params.entity_list.size(); entity_list_index++) {
            debug_ptr += sprintf(debug_ptr, "%u,", params.entity_list[entity_list_index]);
        }
        debug_ptr += sprintf(debug_ptr, "]");
        log_debug(debug_buffer);
    }

    for (uint32_t entity_list_index = 0; entity_list_index < params.entity_list.size(); entity_list_index++) {
        EntityId entity_id = params.entity_list[entity_list_index];
        if (bot_reserve_entity(bot, entity_id)) {
            squad.entity_list.push_back(entity_id);
        }
    }
    if (squad.entity_list.empty()) {
        log_warn("BOT %u no entities could be reserved. Skipping squad creation.", bot.player_id);
        return BOT_SQUAD_ID_NULL;
    }
    log_debug("BOT %u --- end squad create ---");

    bot.squads.push_back(squad);
    bot.next_squad_id++;

    return squad.id;
}

void bot_squad_dissolve(Bot& bot, BotSquad& squad) {
    {
        char debug_buffer[512];
        char* debug_ptr = debug_buffer;
        debug_ptr += sprintf(debug_ptr, "BOT %u squad_dissolve, squad_id %u type %u entities [", bot.player_id, squad.id, squad.type);
        for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
            debug_ptr += sprintf(debug_ptr, "%u,", squad.entity_list[entity_list_index]);
        }
        debug_ptr += sprintf(debug_ptr, "]");
        log_debug(debug_buffer);
    }

    while (!squad.entity_list.empty()) {
        if (bot_release_entity(bot, squad.entity_list.back())) {
            squad.entity_list.pop_back();
        } else {
            log_warn("BOT %u could not dissolve squad %u completely", bot.player_id, squad.id);
            break;
        }
    }
}

bool bot_squad_remove_entity_by_id(Bot& bot, BotSquad& squad, EntityId entity_id) {
    // Determine entity index in squad array
    uint32_t index;
    for (index = 0; index < squad.entity_list.size(); index++) {
        if (squad.entity_list[index] == entity_id) {
            break;
        }
    }

    if (!bot_release_entity(bot, entity_id)) {
        log_warn("BOT %u squad %u remove_entity_by_id, not enough reservation space to release entity.", bot.player_id, squad.id);
        return false;
    }

    squad.entity_list.remove_at_unordered(index);

    return true;
}

MatchInput bot_squad_update(const MatchState* state, Bot& bot, BotSquad& squad, uint32_t match_timer) {
    ZoneScoped;

    // Remove dead units
    bot_squad_remove_dead_units(state, squad);
    if (squad.entity_list.empty()) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Score enemies
    EntityList nearby_enemy_list = bot_squad_get_nearby_enemy_list(state, bot, squad);
    int nearby_enemy_score = bot_score_entity_list(state, bot, nearby_enemy_list);

    // Retreat
    if (squad.type == BOT_SQUAD_TYPE_ATTACK && bot_squad_should_retreat(state, bot, squad, nearby_enemy_score)) {
        /* 
         * The retreat memory determines the score-to-beat when we want to attack
         * this location again.
         * 
         * If there was an existing retreat memory and this new enemy list is bigger
         * than the old one, then the bigger score will become the new score and we'll
         * have a retreat_count of 1. 
         * 
         * If the new enemy list is less than or equal to the old one, then that means
         * there are probably environmental factors causing us to lose in this position
         * so we will increase the retreat count, which will increase how many units
         * we need to attack this location next time
        */

        Cell target_cell_map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, squad.target_cell);
        if (target_cell_map_cell.type == CELL_GOLDMINE) {
            EntityId target_goldmine_id = target_cell_map_cell.id;

            // Retreat memory

            // Find base info index
            uint32_t base_info_index;
            for (base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
                if (bot.base_info[base_info_index].goldmine_id == target_goldmine_id) {
                    break;
                }
            }
            GOLD_ASSERT(base_info_index < bot.base_info.size());

            // Check for existing retreat memory
            const bool bot_has_retreated_here_before = 
                bot.base_info[base_info_index].retreat_count != 0 &&
                // Only increase retreat count against losses to smaller or equal armies than the last retreat memory
                nearby_enemy_score <= bot_score_entity_list(state, bot, bot.base_info[base_info_index].retreat_entity_list) &&
                // Only increase retreat count if it has been (literally) a minute since the last retreat.
                // This shouldn't be that necessary, but it prevents the retreat counter from getting crazy high due to jank bot retreat behavior
                bot.base_info[base_info_index].retreat_time + (60U * UPDATES_PER_SECOND) >= match_timer;

            // Create retreat memory
            bot.base_info[base_info_index].retreat_count = bot_has_retreated_here_before
                ? bot.base_info[base_info_index].retreat_count + 1
                : 1;
            bot.base_info[base_info_index].retreat_time = match_timer;
            bot.base_info[base_info_index].retreat_entity_list = nearby_enemy_list;
        }

        // Set squad to return
        squad.type = BOT_SQUAD_TYPE_RETURN;
        log_debug("BOT %u squad_update, retreat. nearby enemy score %u", bot.player_id, nearby_enemy_score);
    }

    // Bunker squads
    if (bot_squad_has_bunker(state, squad)) {
        return bot_squad_bunker_micro(state, bot, squad);
    }

    // Engaged unit micro
    EntityList unengaged_units;
    if (squad.type != BOT_SQUAD_TYPE_RETURN) {
        for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
            EntityId unit_id = squad.entity_list[squad_entity_index];
            const Entity& unit = state->entities.get_by_id(unit_id);

            // Filer down only to units
            if (!entity_is_unit(unit.type)) {
                continue;
            }

            // Filter down only to ungarrisoned units and non-carriers
            const EntityData& unit_data = entity_get_data(unit.type);
            if (unit.garrison_id != ID_NULL || unit_data.garrison_capacity != 0) {
                continue;
            }

            // Check if this unit already has a good target nearby
            bool unit_is_engaged = 
                ((unit.target.type == TARGET_ATTACK_ENTITY && state->entities.get_index_of(unit.target.id) != INDEX_INVALID)
                    || unit.target.type == TARGET_MOLOTOV) &&
                    ivec2::manhattan_distance(unit.cell, entity_get_target_cell(state, unit)) < BOT_MEDIUM_DISTANCE;

            // Check if there is an enemy nearby
            EntityId nearby_enemy_id = unit_is_engaged
                ? ID_NULL
                : match_find_best_entity(state, (MatchFindBestEntityParams) {
                    .filter = [&state, &unit](const Entity& enemy, EntityId /*enemy_id*/) {
                        return enemy.type != ENTITY_GOLDMINE &&
                                state->players[enemy.player_id].team != state->players[unit.player_id].team &&
                                entity_is_selectable(enemy) &&
                                ivec2::manhattan_distance(enemy.cell, unit.cell) < BOT_NEAR_DISTANCE &&
                                entity_is_visible_to_player(state, enemy, unit.player_id);
                    },
                    .compare = match_compare_closest_manhattan_distance_to(unit.cell)
                });
            if (nearby_enemy_id != ID_NULL) {
                unit_is_engaged = true;
            }

            // If this is none, then the entity is considered unengaged
            if (!unit_is_engaged ||
                    unit.type == ENTITY_BALLOON || 
                    unit.type == ENTITY_WAGON) {
                unengaged_units.push_back(unit_id);
                continue;
            }

            // If unit is pyro, then throw molotov
            if (unit.type == ENTITY_PYRO && nearby_enemy_id != ID_NULL) {
                MatchInput pyro_input = bot_squad_pyro_micro(state, bot, squad, unit, unit_id, state->entities.get_by_id(nearby_enemy_id).cell);
                if (pyro_input.type != MATCH_INPUT_NONE) {
                    return pyro_input;
                }
            }

            // If unit is a detective, then activate camo
            if (unit.type == ENTITY_DETECTIVE && !entity_check_flag(unit, ENTITY_FLAG_INVISIBLE)) {
                MatchInput detective_input = bot_squad_detective_micro(state, bot, squad, unit, unit_id);
                if (detective_input.type != MATCH_INPUT_NONE) {
                    return detective_input;
                }
            }

            // If unit is a miner, tell it to stop mining
            if (unit.type == ENTITY_MINER && entity_is_mining(state, unit) && nearby_enemy_id != ID_NULL) {
                return bot_squad_a_move_miners(state, squad, unit, unit_id, state->entities.get_by_id(nearby_enemy_id).cell);
            }
        } // End for each engaged unit
    }
    // End engaged unit micro

    // Landmine Squad Micro
    if (squad.type == BOT_SQUAD_TYPE_LANDMINES) {
        return bot_squad_landmines_micro(state, bot, squad, match_timer);
    }

    // Patrol squad micro
    if (squad.type == BOT_SQUAD_TYPE_PATROL) {
        MatchInput input;
        input.type = MATCH_INPUT_PATROL;
        input.patrol.target_cell_a = squad.patrol_cell;
        input.patrol.target_cell_b = squad.target_cell;
        input.patrol.unit_count = 0;

        for (uint32_t unengaged_units_index = 0; unengaged_units_index < unengaged_units.size(); unengaged_units_index++) {
            EntityId entity_id = unengaged_units[unengaged_units_index];
            const Entity& entity = state->entities.get_by_id(entity_id);

            // Don't interrupt a unit that is already patrolling
            // Also don't interrupt a unit that has a target
            if (entity.target.type == TARGET_PATROL || entity.target.type == TARGET_ATTACK_ENTITY) {
                continue;
            }

            input.patrol.unit_ids[input.patrol.unit_count] = entity_id;
            input.patrol.unit_count++;
            if (input.patrol.unit_count == SELECTION_LIMIT) {
                break;
            }
        }

        if (input.patrol.unit_count == 0) {
            return (MatchInput) {
                .type = MATCH_INPUT_NONE
            };
        }

        return input;
    }

    // Hold position squad micro
    if (squad.type == BOT_SQUAD_TYPE_HOLD_POSITION) {
        MatchInput input;
        input.type = MATCH_INPUT_DEFEND;
        input.stop.entity_count = 0;

        for (uint32_t unengaged_units_index = 0; unengaged_units_index < unengaged_units.size(); unengaged_units_index++) {
            EntityId entity_id = unengaged_units[unengaged_units_index];
            const Entity& entity = state->entities.get_by_id(entity_id);

            // If it's already holding position, then no need to give any orders
            if (entity_check_flag(entity, ENTITY_FLAG_HOLD_POSITION)) {
                continue;
            }

            input.stop.entity_ids[input.stop.entity_count] = entity_id;
            input.stop.entity_count++;
            if (input.stop.entity_count == SELECTION_LIMIT) {
                break;
            }
        }

        if (input.stop.entity_count == 0) {
            return (MatchInput) {
                .type = MATCH_INPUT_NONE
            };
        }

        return input;
    }

    // Divide unengaged units into distant infantry (can be garrisoned) and distant cavalry (cannot be garrisoned)
    EntityList distant_infantry;
    EntityList distant_cavalry;
    for (uint32_t unengaged_units_index = 0; unengaged_units_index < unengaged_units.size(); unengaged_units_index++) {
        EntityId entity_id = unengaged_units[unengaged_units_index];
        const Entity& entity = state->entities.get_by_id(entity_id);
        const EntityData& entity_data = entity_get_data(entity.type);

        // If units are already close to target cell, don't move them
        if (ivec2::manhattan_distance(entity.cell, squad.target_cell) < BOT_NEAR_DISTANCE / 2) {
            continue;
        }

        // If units cannot garrison, they are cavalry
        if (entity_data.garrison_size == ENTITY_CANNOT_GARRISON) {
            distant_cavalry.push_back(entity_id);
            continue;
        }

        // If units are close enough that ferrying them isn't worth it, 
        // then just place them straight into cavalary
        if (ivec2::manhattan_distance(entity.cell, squad.target_cell) < BOT_MEDIUM_DISTANCE) {
            distant_cavalry.push_back(entity_id);
            continue;
        }

        // If the units are in the process of being ferried, don't add them to the infantry list
        // (If infantry are unable to garrison, they will be added to the cavalry list and be A-moved
        // so filtering them out of the list here prevents that from happening)
        if (entity.target.type == TARGET_ENTITY) {
            continue;
        }

        distant_infantry.push_back(entity_id);
    }

    // Garrison distant infantry if there are any carriers
    for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
        EntityId carrier_id = squad.entity_list[squad_entity_index];
        const Entity& carrier = state->entities.get_by_id(carrier_id);
        const EntityData& carrier_data = entity_get_data(carrier.type);

        // Filter out non-carriers
        if (carrier_data.garrison_capacity == 0) {
            continue;
        }

        // Filter out full carriers
        if (!bot_squad_carrier_has_capacity(state, squad, carrier, carrier_id)) {
            continue;
        }

        MatchInput garrison_input = bot_squad_garrison_into_carrier(state, squad, carrier, carrier_id, distant_infantry);
        if (garrison_input.type != MATCH_INPUT_NONE) {
            return garrison_input;
        }
    }

    // Carrier micro (move carriers closer to units)
    for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
        EntityId carrier_id = squad.entity_list[squad_entity_index];
        const Entity& carrier = state->entities.get_by_id(carrier_id);
        const EntityData& carrier_data = entity_get_data(carrier.type);

        // Filter out non-carriers
        if (!entity_is_unit(carrier.type) || carrier_data.garrison_capacity == 0) {
            continue;
        }

        // Move carrier toward en route infantry
        ivec2 en_route_infantry_center;
        if (bot_squad_carrier_has_en_route_infantry(state, squad, carrier, carrier_id, &en_route_infantry_center)) {
            MatchInput move_carrier_input = bot_squad_move_carrier_toward_en_route_infantry(state, carrier, carrier_id, en_route_infantry_center);
            if (move_carrier_input.type != MATCH_INPUT_NONE) {
                return move_carrier_input;
            }

            continue;
        }
        
        // From here on, the carrier does not have any en route infantry

        // If the carrier is empty, release it from the squad
        if (carrier.garrisoned_units.empty()) {
            if (!bot_squad_remove_entity_by_id(bot, squad, carrier_id)) {
                return (MatchInput) { .type = MATCH_INPUT_NONE };
            }

            log_debug("BOT %u, squad_update, return empty carrier to nearest hall.", bot.player_id);
            return bot_return_entity_to_nearest_hall(state, bot, carrier_id);
        }

        // Unload units
        if (bot_squad_should_carrier_unload_garrisoned_units(state, squad, carrier)) {
            MatchInput input;
            input.type = MATCH_INPUT_MOVE_UNLOAD;
            input.move.shift_command = 0;
            input.move.target_id = ID_NULL;
            input.move.target_cell = carrier.cell;
            input.move.entity_count = 1;
            input.move.entity_ids[0] = carrier_id;

            log_debug("BOT %u, squad_update, unload carrier units.", bot.player_id);
            return input;
        }

        distant_cavalry.push_back(carrier_id);
    }

    // At this point, any distant infantry have not been able to garrison, so move them
    // into the cavalry list so that they can be A-moved
    for (uint32_t index = 0; index < distant_infantry.size(); index++) {
        EntityId entity_id = distant_infantry[index];
        distant_cavalry.push_back(entity_id);
    }

    MatchInput move_input = bot_squad_move_distant_units_to_target(state, squad, distant_cavalry);
    if (move_input.type != MATCH_INPUT_NONE) {
        return move_input;
    }

    // Squad state changes
    bool is_enemy_near_squad = !nearby_enemy_list.empty();
    bool is_enemy_near_target = false;
    if (squad.type == BOT_SQUAD_TYPE_RESERVES || squad.type == BOT_SQUAD_TYPE_ATTACK) {
        EntityId enemy_near_target_id = match_find_entity(state, [&state, &bot, &squad](const Entity& entity, EntityId /*entity_id*/) {
            return entity.type != ENTITY_GOLDMINE &&
                state->players[entity.player_id].team != state->players[bot.player_id].team &&
                entity.health != 0 &&
                ivec2::manhattan_distance(entity.cell, squad.target_cell) < BOT_MEDIUM_DISTANCE;
        });
        is_enemy_near_target = enemy_near_target_id != ID_NULL;
    }
    if (squad.type == BOT_SQUAD_TYPE_RESERVES && !is_enemy_near_target) {
        log_debug("BOT %u squad_update, dissolve reserves squad because no enemy nearby", bot.player_id);
        squad.type = BOT_SQUAD_TYPE_RETURN;
    }
    if (squad.type == BOT_SQUAD_TYPE_ATTACK && !(is_enemy_near_squad || is_enemy_near_target)) {
        log_debug("BOT %u squad_update, return attack squad because no enemy nearby", bot.player_id);
        squad.type = BOT_SQUAD_TYPE_RETURN;
    }

    // Return squad to base
    if (squad.type == BOT_SQUAD_TYPE_RETURN) {
        return bot_squad_return_to_nearest_base(state, bot, squad);
    }

    return (MatchInput) {.type = MATCH_INPUT_NONE };
}

void bot_squad_remove_dead_units(const MatchState* state, BotSquad& squad) {
    uint32_t squad_entity_index = 0;
    while (squad_entity_index < squad.entity_list.size()) {
        uint32_t entity_index = state->entities.get_index_of(squad.entity_list[squad_entity_index]);
        if (entity_index == INDEX_INVALID || state->entities[entity_index].health == 0) {
            // Remove the entity
            squad.entity_list.remove_at_unordered(squad_entity_index);
        } else {
            squad_entity_index++;
        }
    }
}

bool bot_squad_is_entity_near_squad(const MatchState* state, const BotSquad& squad, const Entity& entity) {
    for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
        const Entity& squad_entity = state->entities.get_by_id(squad.entity_list[squad_entity_index]);
        if (ivec2::manhattan_distance(entity.cell, squad_entity.cell) < BOT_NEAR_DISTANCE) {
            return true;
        }
    }

    return false;
}

EntityList bot_squad_get_nearby_enemy_list(const MatchState* state, const Bot& bot, const BotSquad& squad) {
    EntityList enemy_list;

    for (uint32_t enemy_index = 0; enemy_index < state->entities.size(); enemy_index++) {
        const Entity& enemy = state->entities[enemy_index];
        EntityId enemy_id = state->entities.get_id_of(enemy_index);

        // Filter down to enemy units
        if (enemy.type == ENTITY_GOLDMINE || 
                (entity_is_building(enemy.type) && enemy.type != ENTITY_BUNKER) ||
                state->players[enemy.player_id].team == state->players[bot.player_id].team ||
                !entity_is_selectable(enemy)) {
            continue;
        }

        // Filter out any units that are far away from the squad
        if (!bot_squad_is_entity_near_squad(state, squad, enemy)) {
            continue;
        }

        if (enemy_list.is_full()) {
            log_warn("BOT %u squad_get_nearby_enemy_list, squad_id %u, enemy list is full.", bot.player_id, squad.id);
            break;
        }
        enemy_list.push_back(enemy_id);
    }

    return enemy_list;
}

bool bot_squad_should_retreat(const MatchState* state, const Bot& bot, const BotSquad& squad, int nearby_enemy_score) {
    // Don't retreat if not configured to
    if (!bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_RETREAT)) {
        return false;
    }

    // Don't retreat if all-in
    if (bot_should_all_in(bot)) {
        return false;
    }

    // Score allied army
    int squad_score = bot_score_entity_list(state, bot, squad.entity_list);

    return squad_score < nearby_enemy_score;
}

MatchInput bot_squad_bunker_micro(const MatchState* state, const Bot& bot, const BotSquad& squad) {
    EntityId bunker_id = bot_squad_get_bunker_id(state, squad);
    const Entity& bunker = state->entities.get_by_id(bunker_id);

    // Check if a miner is under attack
    EntityId ally_under_attack_id = match_find_entity(state, [&bunker](const Entity& ally, EntityId /*miner_id*/) {
        return ally.player_id == bunker.player_id &&
                entity_is_selectable(ally) &&
                (ally.type == ENTITY_MINER || entity_is_building(ally.type)) &&
                ally.type != ENTITY_BUNKER &&
                ally.taking_damage_timer != 0 &&
                ivec2::manhattan_distance(ally.cell, bunker.cell) < BOT_MEDIUM_DISTANCE;
    });

    // Check if the units in the bunker can hit anything
    const int BUNKER_RANGE = entity_get_data(ENTITY_COWBOY).unit_data.range_squared;
    EntityId enemy_in_range_of_bunker_id = match_find_entity(state, [&state, &bunker, &BUNKER_RANGE](const Entity& enemy, EntityId /*enemy_id*/) {
        return entity_is_unit(enemy.type) &&
                state->players[enemy.player_id].team != state->players[bunker.player_id].team &&
                entity_is_selectable(enemy) &&
                ivec2::euclidean_distance_squared(bunker.cell, enemy.cell) <= BUNKER_RANGE;
    });

    // If a miner is under attack and they can't hit anything, tell them to exit the bunker
    if (ally_under_attack_id != ID_NULL && 
            enemy_in_range_of_bunker_id == ID_NULL && 
            !bunker.garrisoned_units.empty()) {

        MatchInput unload_input;
        unload_input.type = MATCH_INPUT_UNLOAD;
        unload_input.unload.carrier_count = 1;
        unload_input.unload.carrier_ids[0] = bunker_id;

        log_debug("BOT %u squad_update, unload bunker because miner is under attack.", bot.player_id);
        return unload_input;
    }

    // Otherwise, tell them to enter the bunker
    if (ally_under_attack_id == ID_NULL && enemy_in_range_of_bunker_id == ID_NULL && 
            !bot_squad_bunker_units_are_engaged(state, squad, bunker, bunker_id) &&
            bunker.garrisoned_units.size() < squad.entity_list.size() - 1 &&
            bot_squad_carrier_has_capacity(state, squad, bunker, bunker_id)) {
        MatchInput garrison_input = bot_squad_garrison_into_carrier(state, squad, bunker, bunker_id, squad.entity_list);
        if (garrison_input.type != MATCH_INPUT_NONE) {
            std::vector<EntityId> infantry;
            for (uint32_t squad_entity_index = 0; squad_entity_index < squad.entity_list.size(); squad_entity_index++) {
                EntityId entity_id = squad.entity_list[squad_entity_index];
                if (entity_id == bunker_id) {
                    continue;
                }
                infantry.push_back(entity_id);
            }

            log_debug("BOT %u squad %u squad_update, bunker squad enter bunker.", bot.player_id, squad.id);
            return garrison_input;
        }
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

EntityId bot_squad_get_bunker_id(const MatchState* state, const BotSquad& squad) {
    for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
        EntityId entity_id = squad.entity_list[entity_list_index];
        if (state->entities.get_by_id(entity_id).type == ENTITY_BUNKER) {
            return entity_id;
        }
    }

    return ID_NULL;
}

bool bot_squad_has_bunker(const MatchState* state, const BotSquad& squad) {
    return bot_squad_get_bunker_id(state, squad) != ID_NULL;
}

bool bot_squad_bunker_units_are_engaged(const MatchState* state, const BotSquad& squad, const Entity& bunker, EntityId bunker_id) {
    for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
        EntityId entity_id = squad.entity_list[entity_list_index];
        if (entity_id == bunker_id) {
            continue;
        }

        const Entity& entity = state->entities.get_by_id(entity_id);
        if (entity.target.type != TARGET_ATTACK_ENTITY) {
            continue;
        }
        
        uint32_t target_index = state->entities.get_index_of(entity.target.id);
        if (target_index == INDEX_INVALID) {
            continue;
        }

        const Entity& target = state->entities[target_index];
        if (ivec2::manhattan_distance(target.cell, bunker.cell) < BOT_MEDIUM_DISTANCE) {
            return true;
        }
    }

    return false;
}

uint32_t bot_squad_get_carrier_capacity(const MatchState* state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id) {
    const uint32_t GARRISON_CAPACITY = entity_get_data(carrier.type).garrison_capacity;

    uint32_t garrison_size = carrier.garrisoned_units.size();
    for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
        EntityId entity_id = squad.entity_list[entity_list_index];
        const Entity& entity = state->entities.get_by_id(entity_id);
        if (entity.target.type == TARGET_ENTITY && entity.target.id == carrier_id) {
            garrison_size++;
        }
    }

    if (garrison_size > GARRISON_CAPACITY) {
        return 0;
    }
    return GARRISON_CAPACITY - garrison_size;
}

bool bot_squad_carrier_has_capacity(const MatchState* state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id) {
    return bot_squad_get_carrier_capacity(state, squad, carrier, carrier_id) != 0;
}

MatchInput bot_squad_garrison_into_carrier(const MatchState* state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id, const EntityList& entity_list) {
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_ENTITY;
    input.move.shift_command = 0;
    input.move.target_id = carrier_id;
    input.move.target_cell = ivec2(0, 0);
    input.move.entity_count = 0;

    uint32_t carrier_capacity = bot_squad_get_carrier_capacity(state, squad, carrier, carrier_id);

    GOLD_ASSERT(carrier_capacity != 0);

    for (uint32_t entity_list_index = 0; entity_list_index < entity_list.size(); entity_list_index++) {
        EntityId entity_id = entity_list[entity_list_index];
        const Entity& entity = state->entities.get_by_id(entity_id);
        const uint32_t entity_garrison_size = entity_get_data(entity.type).garrison_size;
        
        // Don't garrison units which cannot garrison
        if (entity_garrison_size == ENTITY_CANNOT_GARRISON) {
            if (entity_is_unit(entity.type)) {
                log_warn("BOT %u squad %u squad_garrison_into_carrier, entity_id %u has garrison ENTITY_CANNOT_GARRISON. We should not be adding ungarrisonable entitites to the infantry list.",
                    entity.player_id, squad.id, entity_id);
            }
            continue;
        }

        // Don't garrison units which are already garrisoning
        if (entity.target.type == TARGET_ENTITY) {
            continue;
        }

        // If there is space to garrison, then garrison the unit
        if (input.move.entity_count + entity_garrison_size <= carrier_capacity) {
            input.move.entity_ids[input.move.entity_count] = entity_id;
            input.move.entity_count++;
            continue;
        }

        // If there is no space, then check to see if this entity is closer 
        // than one of the other entities in the input
        uint32_t furthest_unit_index = 0;
        for (uint32_t unit_index = 1; unit_index < input.move.entity_count; unit_index++) {
            ivec2 unit_cell = state->entities.get_by_id(input.move.entity_ids[unit_index]).cell;
            ivec2 furthest_unit_cell = state->entities.get_by_id(input.move.entity_ids[furthest_unit_index]).cell;
            if (ivec2::manhattan_distance(unit_cell, carrier.cell) >
                ivec2::manhattan_distance(furthest_unit_cell, carrier.cell)) {
                furthest_unit_cell = unit_cell;
            }
        }

        // If the entity is closer than the furthest unit, 
        // then replace the furthest with this infantry
        ivec2 furthest_unit_cell = state->entities.get_by_id(input.move.entity_ids[furthest_unit_index]).cell;
        if (ivec2::manhattan_distance(furthest_unit_cell, carrier.cell) >
            ivec2::manhattan_distance(entity.cell, carrier.cell)) {
            input.move.entity_ids[furthest_unit_index] = entity_id;
        }
    } // End for each entity in entity_list

    // If the input entity count is 0, it should mean that all the entities in the
    // entity_list were already garrisoning somewhere
    if (input.move.entity_count == 0) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    return input;
}

bool bot_squad_carrier_has_en_route_infantry(const MatchState* state, const BotSquad& squad, const Entity& carrier, EntityId carrier_id, ivec2* en_route_infantry_center) {
    const uint32_t GARRISON_CAPACITY = entity_get_data(carrier.type).garrison_capacity;

    if (en_route_infantry_center != NULL) {
        *en_route_infantry_center = ivec2(0, 0);
    }

    if (carrier.garrisoned_units.size() == GARRISON_CAPACITY) {
        return false;
    }

    uint32_t en_route_infantry_count = 0;
    for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
        EntityId entity_id = squad.entity_list[entity_list_index];
        const Entity& entity = state->entities.get_by_id(entity_id);
        if (entity.target.type == TARGET_ENTITY && entity.target.id == carrier_id) {
            *en_route_infantry_center += entity.cell;
            en_route_infantry_count++;
        }
    }

    if (en_route_infantry_count != 0) {
        *en_route_infantry_center = *en_route_infantry_center / en_route_infantry_count;
    }

    return en_route_infantry_count != 0;
}

MatchInput bot_squad_move_carrier_toward_en_route_infantry(const MatchState* state, const Entity& carrier, EntityId carrier_id, ivec2 en_route_infantry_center) {
    // If the carrier is already close to the infantry, then don't worry about moving
    ivec2 carrier_cell = carrier.target.type == TARGET_CELL ? carrier.target.cell : carrier.cell;
    if (ivec2::manhattan_distance(carrier_cell, en_route_infantry_center) < BOT_NEAR_DISTANCE) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Long pathing might be a performance issue here
    MapPath path_to_infantry_center;
    map_pathfind(state->map, CELL_LAYER_GROUND, carrier.cell, en_route_infantry_center, 2, MAP_OPTION_IGNORE_UNITS, NULL, &path_to_infantry_center);

    // If the path is small, then don't bother moving
    if (path_to_infantry_center.size() < BOT_NEAR_DISTANCE) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 path_midpoint = path_to_infantry_center.get_from_top(path_to_infantry_center.size() / 2);

    // If the carrier is already walking close to the path midpoint, then don't bother moving
    if (ivec2::manhattan_distance(carrier_cell, path_midpoint) < BOT_NEAR_DISTANCE) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Move carrier to the midpoint
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = path_midpoint;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = carrier_id;

    log_debug("BOT %u, squad_move_carrier_toward_en_route_infantry. carrier.cell <%i, %i> carrier_cell <%i, %i> infantry center <%i, %i>", carrier.player_id, carrier.cell.x, carrier.cell.y, carrier_cell.x, carrier_cell.y, en_route_infantry_center.x, en_route_infantry_center.y);
    return input;
}

bool bot_squad_should_carrier_unload_garrisoned_units(const MatchState* state, const BotSquad& squad, const Entity& carrier) {
    // Unload units if near target cell or near an enemy,
    // but don't unload units if we are already unloading units

    if (carrier.target.type == TARGET_UNLOAD && ivec2::manhattan_distance(carrier.cell, carrier.target.cell) < 4) {
        return false;
    }
    if (ivec2::manhattan_distance(carrier.cell, squad.target_cell) < BOT_NEAR_DISTANCE - 4) {
        return true;
    }
    EntityId nearby_enemy_id = match_find_entity(state, [&state, &carrier](const Entity& enemy, EntityId /*enemy_id*/) {
        return enemy.type != ENTITY_GOLDMINE &&
                state->players[enemy.player_id].team != state->players[carrier.player_id].team &&
                entity_is_selectable(enemy) &&
                ivec2::manhattan_distance(enemy.cell, carrier.cell) < BOT_NEAR_DISTANCE &&
                entity_is_visible_to_player(state, enemy, carrier.player_id);
    });
    return nearby_enemy_id != ID_NULL;
}

MatchInput bot_squad_pyro_micro(const MatchState* state, Bot& bot, BotSquad& squad, const Entity& pyro, EntityId pyro_id, ivec2 nearby_enemy_cell) {
    // If pyro is out of energy, run away
    if (pyro.energy < MOLOTOV_ENERGY_COST) {
        if (squad.type != BOT_SQUAD_TYPE_LANDMINES) {
            if (bot_squad_remove_entity_by_id(bot, squad, pyro_id)) {
                return (MatchInput) { .type = MATCH_INPUT_NONE };
            }
        }
        log_debug("BOT %u squad_pyro_micro, out of energy. return to nearest hall.", bot.player_id);
        return bot_return_entity_to_nearest_hall(state, bot, pyro_id);
    }

    ivec2 molotov_cell = bot_squad_find_best_molotov_cell(state, pyro, nearby_enemy_cell);

    // Don't throw if no valid cell was found
    if (molotov_cell.x == -1) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Don't interrupt a throw in-progress, unless it is far away
    if (pyro.target.type == TARGET_MOLOTOV &&
            ivec2::manhattan_distance(pyro.target.cell, molotov_cell) < BOT_NEAR_DISTANCE) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    log_debug("BOT %u squad_pyro_micro, throw molotov.", bot.player_id);
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_MOLOTOV;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = molotov_cell;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = pyro_id;
    return input;
}

int bot_squad_get_molotov_cell_score(const MatchState* state, const Entity& pyro, ivec2 cell) {
    if (!map_is_cell_in_bounds(state->map, cell) ||
            map_get_cell(state->map, CELL_LAYER_GROUND, cell).type == CELL_BLOCKED) {
        return 0;
    }

    int score = 0;
    std::unordered_map<EntityId, bool> entity_considered;

    for (int y = cell.y - PROJECTILE_MOLOTOV_FIRE_SPREAD; y < cell.y + PROJECTILE_MOLOTOV_FIRE_SPREAD; y++) {
        for (int x = cell.x - PROJECTILE_MOLOTOV_FIRE_SPREAD; x < cell.x + PROJECTILE_MOLOTOV_FIRE_SPREAD; x++) {
            // Don't count the corners of the square
            if ((y == cell.y - PROJECTILE_MOLOTOV_FIRE_SPREAD || y == cell.y + PROJECTILE_MOLOTOV_FIRE_SPREAD - 1) &&
                    (x == cell.x - PROJECTILE_MOLOTOV_FIRE_SPREAD || x == cell.x + PROJECTILE_MOLOTOV_FIRE_SPREAD - 1)) {
                continue;
            }

            ivec2 fire_cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state->map, fire_cell)) {
                continue;
            }
            if (fire_cell == pyro.cell) {
                score -= 4;
                continue;
            }
            if (match_is_cell_on_fire(state, fire_cell)) {
                score--;
                continue;
            }

            Cell map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, fire_cell);
            if (map_cell.type == CELL_BUILDING || map_cell.type == CELL_UNIT || map_cell.type == CELL_MINER) {
                if (entity_considered.find(map_cell.id) != entity_considered.end()) {
                    continue;
                }
                entity_considered[map_cell.id] = true;

                const Entity& target = state->entities.get_by_id(map_cell.id);
                if (state->players[target.player_id].team == state->players[pyro.player_id].team) {
                    score -= 2;
                } else if (entity_is_unit(target.type)) {
                    score += 2;
                } else {
                    score++;
                }
            }
        }
    } // End for each fire y

    // Consider other existing or about to exist molotov fires
    std::vector<ivec2> existing_molotov_cells;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.target.type == TARGET_MOLOTOV) {
            existing_molotov_cells.push_back(entity.target.cell);
        }
    }
    for (uint32_t projectile_index = 0; projectile_index < state->projectiles.size(); projectile_index++) {
        const Projectile& projectile = state->projectiles[projectile_index];
        if (projectile.type == PROJECTILE_MOLOTOV) {
            existing_molotov_cells.push_back(projectile.target.to_ivec2() / TILE_SIZE);
        }
    }
    for (ivec2 existing_molotov_cell : existing_molotov_cells) {
        int distance = ivec2::manhattan_distance(cell, existing_molotov_cell);
        if (distance < PROJECTILE_MOLOTOV_FIRE_SPREAD) {
            score -= (PROJECTILE_MOLOTOV_FIRE_SPREAD - distance) * 5;
        }
    }

    return score;
}

ivec2 bot_squad_find_best_molotov_cell(const MatchState* state, const Entity& pyro, ivec2 attack_point) {
    static const int MINIMUM_MOLOTOV_CELL_SCORE = 4;
    ivec2 best_molotov_cell = ivec2(-1, -1);
    int best_molotov_cell_score = MINIMUM_MOLOTOV_CELL_SCORE - 1;

    for (int y = attack_point.y - BOT_NEAR_DISTANCE; y < attack_point.y + BOT_NEAR_DISTANCE; y++) {
        for (int x = attack_point.x - BOT_NEAR_DISTANCE; x < attack_point.x + BOT_NEAR_DISTANCE; x++) {
            ivec2 cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state->map, cell)) {
                continue;
            }

            int molotov_cell_score = bot_squad_get_molotov_cell_score(state, pyro, cell);
            if (molotov_cell_score > best_molotov_cell_score) {
                best_molotov_cell = cell;
                best_molotov_cell_score = molotov_cell_score;
            }
        }
    }

    return best_molotov_cell;
}

MatchInput bot_squad_detective_micro(const MatchState* state, Bot& bot, BotSquad& squad, const Entity& detective, EntityId detective_id) {
    if (detective.energy < CAMO_ENERGY_COST) {
        if (bot_squad_remove_entity_by_id(bot, squad, detective_id)) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }
        log_debug("BOT %u squad_detective_micro, out of energy. returning to nearest hall.", bot.player_id);
        return bot_return_entity_to_nearest_hall(state, bot, detective_id);
    }

    MatchInput input;
    input.type = MATCH_INPUT_CAMO;
    input.camo.unit_count = 1;
    input.camo.unit_ids[0] = detective_id;

    // See if there are any other un-cloaked detectives in the squad
    for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
        EntityId other_detective_id = squad.entity_list[entity_list_index];
        if (other_detective_id == detective_id) {
            continue;
        }
        const Entity& other_detective = state->entities.get_by_id(other_detective_id);
        if (other_detective.type != ENTITY_DETECTIVE ||
                entity_check_flag(other_detective, ENTITY_FLAG_INVISIBLE) ||
                other_detective.energy < CAMO_ENERGY_COST ||
                ivec2::manhattan_distance(other_detective.cell, detective.cell) < BOT_NEAR_DISTANCE) {
            continue;
        }

        input.camo.unit_ids[input.camo.unit_count] = other_detective_id;
        input.camo.unit_count++;

        if (input.camo.unit_count == SELECTION_LIMIT) {
            break;
        }
    }

    return input;
}

MatchInput bot_squad_a_move_miners(const MatchState* state, const BotSquad& squad, const Entity& first_miner, EntityId first_miner_id, ivec2 nearby_enemy_cell) {
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = nearby_enemy_cell;
    input.move.target_id = ID_NULL;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = first_miner_id;

    for (uint32_t entity_list_index = 0; entity_list_index < squad.entity_list.size(); entity_list_index++) {
        EntityId other_miner_id = squad.entity_list[entity_list_index];
        const Entity& other_miner = state->entities.get_by_id(other_miner_id);
        if (other_miner.type != ENTITY_MINER ||
                !entity_is_selectable(other_miner) ||
                !entity_is_mining(state, other_miner) ||
                ivec2::manhattan_distance(other_miner.cell, first_miner.cell) > BOT_NEAR_DISTANCE) {
            continue;
        }

        input.move.entity_ids[input.move.entity_count] = other_miner_id;
        input.move.entity_count++;
        if (input.move.entity_count == SELECTION_LIMIT) {
            break;
        }
    }

    return input;
}

MatchInput bot_squad_move_distant_units_to_target(const MatchState* state, const BotSquad& squad, const EntityList& entity_list) {
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_ATTACK_CELL;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = squad.target_cell;
    input.move.entity_count = 0;

    for (uint32_t index = 0; index < entity_list.size(); index++) {
        EntityId entity_id = entity_list[index];
        const Entity& entity = state->entities.get_by_id(entity_id);

        // Filter out units that are already moving
        if (entity.target.type == TARGET_ATTACK_CELL && ivec2::manhattan_distance(entity.target.cell, squad.target_cell) < BOT_NEAR_DISTANCE) {
            continue;
        }

        input.move.entity_ids[input.move.entity_count] = entity_id;
        input.move.entity_count++;
        if (input.move.entity_count == SELECTION_LIMIT) {
            break;
        }
    }

    if (input.move.entity_count == 0) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    log_debug("BOT %u squad_move_distant_units_to_target. entity count %u", state->entities.get_by_id(input.move.entity_ids[0]).player_id, input.move.entity_count);
    return input;
}

MatchInput bot_squad_return_to_nearest_base(const MatchState* state, Bot& bot, BotSquad& squad) {
    EntityId nearest_base_goldmine_id = bot_squad_get_nearest_base_goldmine_id(state, bot, squad);

    // If we don't control a base, just dissolve the squad
    if (nearest_base_goldmine_id == ID_NULL) {
        bot_squad_dissolve(bot, squad);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = bot_get_unoccupied_cell_near_goldmine(state, bot, nearest_base_goldmine_id);
    input.move.entity_count = 0;

    while (!squad.entity_list.empty()) {
        input.move.entity_ids[input.move.entity_count] = squad.entity_list.back();
        input.move.entity_count++;

        if (!bot_release_entity(bot, squad.entity_list.back())) {
            break;
        }
        squad.entity_list.pop_back();

        if (input.move.entity_count == SELECTION_LIMIT) {
            break;
        }
    }

    if (input.move.entity_count == 0) {
        log_warn("BOT %u squad_return_to_nearest_base, squad_id %u, no entities added to input.", bot.player_id, squad.id);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    return input;
}

EntityId bot_squad_get_nearest_base_goldmine_id(const MatchState* state, const Bot& bot, const BotSquad& squad) {
    ivec2 target_cell = squad.target_cell;
    uint32_t nearest_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player != bot.player_id) {
            continue;
        }

        if (nearest_base_info_index == INDEX_INVALID || 
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, target_cell) < 
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_base_info_index].goldmine_id).cell, target_cell)) {
            nearest_base_info_index = base_info_index;
        }
    }

    GOLD_ASSERT(nearest_base_info_index != INDEX_INVALID);
    return bot.base_info[nearest_base_info_index].goldmine_id;
}

EntityList bot_create_entity_list_from_entity_count(const MatchState* state, const Bot& bot, EntityCount entity_count) {
    EntityList entity_list;
    EntityCount list_entity_count;

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        EntityId entity_id = state->entities.get_id_of(entity_index);

        // Filter down to only bot-owned, unreserved units
        if (entity.player_id != bot.player_id ||
                !entity_is_selectable(entity) ||
                entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED)) {
            continue;
        }

        // Filter out entities that we don't want
        if (list_entity_count[entity.type] >= entity_count[entity.type]) {
            continue;
        }

        if (entity_list.is_full()) {
            log_warn("BOT %u create_entity_list_from_entity_count, entity list is full.", bot.player_id);
            break;
        }

        entity_list.push_back(entity_id);
        list_entity_count[entity.type]++;
    }

    return entity_list;
}

void bot_entity_list_filter(const MatchState* state, EntityList& entity_list, std::function<bool(const Entity& entity, EntityId entity_id)> filter) {
    uint32_t index = 0;
    while (index < entity_list.size()) {
        EntityId entity_id = entity_list[index];
        const Entity& entity = state->entities.get_by_id(entity_id);

        if (filter(entity, entity_id)) {
            entity_list[index] = entity_list.back();
            entity_list.pop_back();
        } else {
            index++;
        }
    }
}

ivec2 bot_entity_list_get_center(const MatchState* state, const EntityList& entity_list) {
    if (entity_list.empty()) {
        return ivec2(-1, -1);
    }

    ivec2 center = ivec2(0, 0);
    for (uint32_t index = 0; index < entity_list.size(); index++) {
        center += state->entities.get_by_id(entity_list[index]).cell;
    }

    return center / (int)entity_list.size();
}

ivec2 bot_squad_choose_target_cell(const MatchState* state, const Bot& bot, BotSquadType type, const EntityList& entity_list) {
    switch (type) {
        case BOT_SQUAD_TYPE_ATTACK:
            return bot_squad_get_attack_target_cell(state, bot, entity_list);
        case BOT_SQUAD_TYPE_DEFEND:
            return bot_squad_get_defend_target_cell(state, bot, entity_list);
        case BOT_SQUAD_TYPE_LANDMINES:
            return bot_squad_get_landmine_target_cell(state, bot, state->entities.get_by_id(entity_list[0]).cell);
        case BOT_SQUAD_TYPE_RESERVES:
        case BOT_SQUAD_TYPE_RETURN:
        case BOT_SQUAD_TYPE_PATROL:
        case BOT_SQUAD_TYPE_HOLD_POSITION:
        case BOT_SQUAD_TYPE_COUNT:
            GOLD_ASSERT_MESSAGE(false, "This squad type should not be used with squad_choose_target_cell.");
            return ivec2(-1, -1);
    }
}

ivec2 bot_squad_get_landmine_target_cell(const MatchState* state, const Bot& bot, ivec2 pyro_cell) {
    uint32_t nearest_controlled_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player != bot.player_id) {
            continue;
        }

        if (nearest_controlled_base_info_index == INDEX_INVALID ||
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, pyro_cell) <
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_controlled_base_info_index].goldmine_id).cell, pyro_cell)) {
            nearest_controlled_base_info_index = base_info_index;
        }
    }

    if (nearest_controlled_base_info_index == INDEX_INVALID) {
        return ivec2(-1, -1);
    }

    return bot_get_unoccupied_cell_near_goldmine(state, bot, bot.base_info[nearest_controlled_base_info_index].goldmine_id);
}

ivec2 bot_squad_get_attack_target_cell(const MatchState* state, const Bot& bot, const EntityList& entity_list) {
    ivec2 squad_center = bot_entity_list_get_center(state, entity_list);

    // Find nearest enemy base and least defended enemy base
    uint32_t nearest_enemy_base_info_index = INDEX_INVALID;
    uint32_t least_defended_enemy_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player == PLAYER_NONE ||
                state->players[base_info.controlling_player].team == state->players[bot.player_id].team) {
            continue;
        }

        if (nearest_enemy_base_info_index == INDEX_INVALID ||
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, squad_center) <
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_enemy_base_info_index].goldmine_id).cell, squad_center)) {
            nearest_enemy_base_info_index = base_info_index;
        }

        if (least_defended_enemy_base_info_index == INDEX_INVALID ||
                base_info.defense_score < bot.base_info[least_defended_enemy_base_info_index].defense_score) {
            least_defended_enemy_base_info_index = base_info_index;
        }
    }

    if (nearest_enemy_base_info_index == INDEX_INVALID) {
        return ivec2(-1, -1);
    }

    // Compare the least defended base with the nearest one.
    // If the nearest is basically as well defended as the least defended hall,
    // then prefer the nearest one
    uint32_t base_to_attack_info_index = 
        bot.base_info[nearest_enemy_base_info_index].defense_score - 
        bot.base_info[least_defended_enemy_base_info_index].defense_score < 
        2 * BOT_UNIT_SCORE
            ? nearest_enemy_base_info_index
            : least_defended_enemy_base_info_index;

    return state->entities.get_by_id(bot.base_info[base_to_attack_info_index].goldmine_id).cell;
}

ivec2 bot_squad_get_defend_target_cell(const MatchState* state, const Bot& bot, const EntityList& entity_list) {
    // For bunker squads, return the bunker cell
    for (uint32_t entity_list_index = 0; entity_list_index < entity_list.size(); entity_list_index++) {
        EntityId entity_id = entity_list[entity_list_index];
        const Entity& entity = state->entities.get_by_id(entity_id);
        if (entity.type == ENTITY_BUNKER) {
            return entity.cell;
        }
    }

    // For other squads, choose the least-defended allied base
    uint32_t least_defended_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player == bot.player_id) {
            continue;
        }
        if (least_defended_base_info_index == INDEX_INVALID ||
                base_info.defense_score < bot.base_info[least_defended_base_info_index].defense_score) {
            least_defended_base_info_index = base_info_index;
        }
    }

    if (least_defended_base_info_index == INDEX_INVALID) {
        return ivec2(-1, -1);
    }
    ivec2 least_defended_base_cell = state->entities.get_by_id(bot.base_info[least_defended_base_info_index].goldmine_id).cell;

    // Find the enemy base nearest to this one
    uint32_t nearest_enemy_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player == PLAYER_NONE ||
                state->players[base_info.controlling_player].team == state->players[bot.player_id].team) {
            continue;
        }
        if (nearest_enemy_base_info_index == INDEX_INVALID ||
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, least_defended_base_cell) < 
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_enemy_base_info_index].goldmine_id).cell, least_defended_base_cell)) {
            nearest_enemy_base_info_index = base_info_index;
        }
    }

    if (nearest_enemy_base_info_index == INDEX_INVALID) {
        return ivec2(-1, -1);
    }
    ivec2 nearest_enemy_base_cell = state->entities.get_by_id(bot.base_info[nearest_enemy_base_info_index].goldmine_id).cell;

    ivec2 path_start_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, least_defended_base_cell, 3, 1, nearest_enemy_base_cell, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_base_cell, 3, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    MapPath path;
    map_pathfind(state->map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS, NULL, &path);

    if (path.empty()) {
        log_warn("BOT %u squad_get_defend_target_cell, path empty.", bot.player_id);
        return ivec2(-1, -1);
    }

    return path.get_from_top(std::min(path.size() - 1U, 7U));
}

MatchInput bot_squad_landmines_micro(const MatchState* state, Bot& bot, const BotSquad& squad, uint32_t match_timer) {
    if (match_timer < bot.next_landmine_time) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    if (!match_player_has_upgrade(state, bot.player_id, UPGRADE_LANDMINES)) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    EntityId pyro_id = squad.entity_list[0];
    const Entity& pyro = state->entities.get_by_id(pyro_id);

    // If the pyro is already making a landmine or doesn't have enough energy, then do nothing
    if (pyro.target.type == TARGET_BUILD || pyro.energy < entity_get_data(ENTITY_LANDMINE).gold_cost) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Find all controlled bases and initialize their landmine count to 0
    bool bot_has_base = false;
    FixedVector<uint32_t, BOT_MAX_BASE_INFO> base_landmine_count;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];

        base_landmine_count.push_back(0);
        if (base_info.controlling_player == bot.player_id) {
            bot_has_base = true;
        }
    }
    // If we have no bases, do nothing
    if (!bot_has_base) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Determine landmine count at each base
    for (uint32_t landmine_index = 0; landmine_index < state->entities.size(); landmine_index++) {
        const Entity& landmine = state->entities[landmine_index];
        if (landmine.type != ENTITY_LANDMINE || landmine.player_id != bot.player_id) {
            continue;
        }

        uint32_t nearest_base_info_index = INDEX_INVALID;
        for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
            const BotBaseInfo& base_info = bot.base_info[base_info_index];
            if (base_info.controlling_player != bot.player_id) {
                continue;
            }

            if (nearest_base_info_index == INDEX_INVALID || 
                    ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, landmine.cell) <
                    ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_base_info_index].goldmine_id).cell, landmine.cell)) {
                nearest_base_info_index = base_info_index;
            }
        }

        GOLD_ASSERT(nearest_base_info_index != INDEX_INVALID);
        base_landmine_count[nearest_base_info_index]++;
    }

    // Find the nearest base which still needs landmines
    uint32_t nearest_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        if (base_landmine_count[base_info_index] >= LANDMINE_MAX_PER_BASE) {
            continue;
        }

        if (nearest_base_info_index == INDEX_INVALID ||
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, pyro.cell) <
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_base_info_index].goldmine_id).cell, pyro.cell)) {
            nearest_base_info_index = base_info_index;
        }
    }

    // If there are no bases which still need landmines, then do nothing
    if (nearest_base_info_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    const Entity& nearest_base_goldmine = state->entities.get_by_id(bot.base_info[nearest_base_info_index].goldmine_id);

    // Find the nearest enemy base to determine where to place mines    
    uint32_t nearest_enemy_base_info_index = INDEX_INVALID;
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.controlling_player == PLAYER_NONE ||
                state->players[base_info.controlling_player].team == state->players[bot.player_id].team) {
            continue;
        }

        if (nearest_enemy_base_info_index == INDEX_INVALID ||
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[base_info_index].goldmine_id).cell, nearest_base_goldmine.cell) < 
                ivec2::manhattan_distance(state->entities.get_by_id(bot.base_info[nearest_enemy_base_info_index].goldmine_id).cell, nearest_base_goldmine.cell)) {
            nearest_enemy_base_info_index = base_info_index;
        }
    }

    // If there are no enemy bases, then do nothing
    if (nearest_enemy_base_info_index == INDEX_INVALID) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Pathfind from the nearest un-landmined base to the nearest enemy base
    const Entity& nearest_enemy_base_goldmine = state->entities.get_by_id(bot.base_info[nearest_enemy_base_info_index].goldmine_id);
    ivec2 path_start_cell = map_get_exit_cell(state->map, CELL_LAYER_GROUND, nearest_base_goldmine.cell, 3, 1, nearest_enemy_base_goldmine.cell, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    ivec2 path_end_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, path_start_cell, 1, nearest_enemy_base_goldmine.cell, 4, MAP_OPTION_IGNORE_UNITS | MAP_OPTION_IGNORE_MINERS);
    MapPath path;
    map_pathfind(state->map, CELL_LAYER_GROUND, path_start_cell, path_end_cell, 1, MAP_OPTION_AVOID_LANDMINES, NULL, &path);

    if (path.size() < BOT_NEAR_DISTANCE) {
        bot.next_landmine_time = match_timer + (UPDATES_PER_SECOND * 60U);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    // Check if landmine cell is safe
    ivec2 landmine_cell = path.get_from_top(BOT_NEAR_DISTANCE - 1);
    EntityId nearby_enemy_id = match_find_entity(state, [&state, &bot, &landmine_cell](const Entity& entity, EntityId /*entity_id*/) {
        return state->players[entity.player_id].team != state->players[bot.player_id].team &&
            entity_is_selectable(entity) && 
            ivec2::manhattan_distance(entity.cell, landmine_cell) < BOT_NEAR_DISTANCE;
    });
    if (nearby_enemy_id == ID_NULL) {
        bot.next_landmine_time = match_timer + (UPDATES_PER_SECOND * 60U);
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_BUILD;
    input.build.shift_command = 0;
    input.build.building_type = ENTITY_LANDMINE;
    input.build.target_cell = landmine_cell;
    input.build.entity_count = 1;
    input.build.entity_ids[0] = pyro_id;
    return input;
}

// SCOUTING

void bot_scout_gather_info(const MatchState* state, Bot& bot) {
    ZoneScoped;

    // Prune entities to scout list
    bot_prune_entities_assumed_to_be_scouted_list(state, bot);
    
    // Check for scout death
    if (bot.scout_id != ID_NULL) {
        uint32_t scout_index = state->entities.get_index_of(bot.scout_id);
        if (scout_index == INDEX_INVALID || state->entities[scout_index].health == 0) {
            bot.scout_id = ID_NULL;
        }
    }

    // Check for invisible units
    EntityId proof_of_landmines_id = match_find_entity(state, [&state, &bot](const Entity& entity, EntityId entity_id) {
        if (state->players[entity.player_id].team == state->players[bot.player_id].team) {
            return false;
        }
        if ((entity.type == ENTITY_PYRO || entity.type == ENTITY_LANDMINE) && 
                entity_is_visible_to_player(state, entity, bot.player_id)) {
            return true;
        }
        if (entity.type == ENTITY_WORKSHOP && bot_has_scouted_entity(state, bot, entity, entity_id)) {
            return true;
        }
        return false;
    });
    if (proof_of_landmines_id != ID_NULL) {
        bitflag_set(&bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES, true);
    }

    EntityId proof_of_detectives_id = match_find_entity(state, [&state, &bot](const Entity& entity, EntityId entity_id) {
        if (state->players[entity.player_id].team == state->players[bot.player_id].team) {
            return false;
        }
        if (entity.type == ENTITY_DETECTIVE && entity_is_visible_to_player(state, entity, bot.player_id)) {
            return true;
        }
        if (entity.type == ENTITY_SHERIFFS && bot_has_scouted_entity(state, bot, entity, entity_id)) {
            return true;
        }
        return false;
    });
    if (proof_of_detectives_id != ID_NULL) {
        bitflag_set(&bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_DETECTIVES, true);
    }

    bot_update_desired_production(bot);
    bot_update_base_info(state, bot);
}

void bot_clear_base_info(BotBaseInfo& info) {
    info.controlling_player = PLAYER_NONE,
    info.has_surrounding_hall = false,
    info.is_saturated = false,
    info.has_gold = false,
    info.is_low_on_gold = false,
    info.is_under_attack = false,
    info.has_been_scouted = false,
    info.defense_score = 0;
}

void bot_update_base_info(const MatchState* state, Bot& bot) {
    
    // Populate the base_info list and figure out who controls each goldmine
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        BotBaseInfo& base_info = bot.base_info[base_info_index];
        const Entity& goldmine = state->entities.get_by_id(base_info.goldmine_id);

        bot_clear_base_info(base_info);

        if (!bot_has_scouted_entity(state, bot, goldmine, base_info.goldmine_id)) {
            continue;
        }

        base_info.has_gold = goldmine.gold_held != 0;
        // Base is not considered low_on_gold if it does not have gold
        // This is to prevent a bug where bot was building an extra base than it should have because
        // it considered its collapsed goldmine base (which it had already replaced) as a low_on_gold base
        base_info.is_low_on_gold = goldmine.gold_held != 0 && goldmine.gold_held < 1000;
        base_info.has_been_scouted = true;

        // First try to find the surrounding hall
        EntityId nearest_building_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);

        // If there's no surrounding hall, fallback to the nearest building
        if (nearest_building_id == ID_NULL) {
            nearest_building_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
                .filter = [&state, &bot, &goldmine](const Entity& building, EntityId building_id) {
                    return entity_is_building(building.type) &&
                            building.type != ENTITY_LANDMINE &&
                            building.mode != MODE_BUILDING_DESTROYED &&
                            ivec2::manhattan_distance(building.cell, goldmine.cell) < BOT_NEAR_DISTANCE &&
                            bot_has_scouted_entity(state, bot, building, building_id);
                },
                .compare = match_compare_closest_manhattan_distance_to(goldmine.cell)
            });
            if (nearest_building_id == ID_NULL) {
                continue;
            }
        }

        const Entity& nearest_building = state->entities.get_by_id(nearest_building_id);
        base_info.controlling_player = nearest_building.player_id;
        base_info.has_surrounding_hall = nearest_building.type == ENTITY_HALL;

        base_info.is_saturated = 
            base_info.has_surrounding_hall &&
            base_info.has_gold &&
            match_get_miners_on_gold(state, base_info.goldmine_id, base_info.controlling_player) == MATCH_MAX_MINERS_ON_GOLD;
    }

    // Calculate base defense score for each controlled goldmine
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        EntityId entity_id = state->entities.get_id_of(entity_index);

        // Filter out goldmines 
        if (entity.type == ENTITY_GOLDMINE) {
            continue;
        }
        // Filter out buildings except for bunkers and landmines
        if (entity_is_building(entity.type) &&
                !(entity.type == ENTITY_BUNKER || entity.type == ENTITY_LANDMINE)) {
            continue;
        }
        // If it's a bunker, make sure we've scouted it
        if (entity.type == ENTITY_BUNKER && !bot_has_scouted_entity(state, bot, entity, entity_id)) {
            continue;
        }
        // If it's an enemy landmine, make sure we've scouted that they have landmines
        if (entity.type == ENTITY_LANDMINE && 
                state->players[entity.player_id].team != state->players[bot.player_id].team &&
                !bitflag_check(bot.scout_info, BOT_SCOUT_INFO_ENEMY_HAS_LANDMINES)) {
            continue;
        }

        uint32_t nearest_base_info_index = INDEX_INVALID;
        for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
            const BotBaseInfo& base_info = bot.base_info[base_info_index];
            const Entity& goldmine = state->entities.get_by_id(base_info.goldmine_id);
            // Make sure this entity only counts towards its own team
            if (base_info.controlling_player == PLAYER_NONE ||
                    state->players[entity.player_id].team != 
                    state->players[base_info.controlling_player].team) {
                continue;
            }
            // Make sure this entity only counts if its close to the base
            if (ivec2::manhattan_distance(entity.cell, goldmine.cell) < BOT_MEDIUM_DISTANCE) {
                continue;
            }

            if (nearest_base_info_index == INDEX_INVALID ||
                    ivec2::manhattan_distance(entity.cell, goldmine.cell) <
                    ivec2::manhattan_distance(entity.cell, state->entities.get_by_id(bot.base_info[nearest_base_info_index].goldmine_id).cell)) {
                nearest_base_info_index = base_info_index;
            }
        }

        if (nearest_base_info_index == INDEX_INVALID) {
            continue;
        }

        int entity_defense_score = entity.type == ENTITY_LANDMINE ? 2 : bot_score_entity(state, bot, entity);
        bot.base_info[nearest_base_info_index].defense_score += entity_defense_score;
    }

    // Add retreat memories to defense score
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        const BotBaseInfo& base_info = bot.base_info[base_info_index];
        if (base_info.retreat_count == 0) {
            continue;
        }

        // Determine retreat memory score
        int retreat_memory_score = bot_score_entity_list(state, bot, base_info.retreat_entity_list);

        // Add a desired lead to the score. Initially this is a lead of 4 units,
        // but for each retreat count above 1, the size of the lead doubles
        int desired_lead = 4 * BOT_UNIT_SCORE * (2 * (base_info.retreat_count - 1));
        retreat_memory_score += desired_lead;

        bot.base_info[base_info_index].defense_score = std::max(bot.base_info[base_info_index].defense_score, retreat_memory_score);
    }

    // Determine if bases are under attack
    for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
        BotBaseInfo& base_info = bot.base_info[base_info_index];

        if (base_info.controlling_player == PLAYER_NONE) {
            base_info.is_under_attack = false;
            continue;
        }

        const Entity& goldmine = state->entities.get_by_id(base_info.goldmine_id);
        EntityId nearby_enemy_id = match_find_entity(state, [&state, &base_info, &goldmine](const Entity& entity, EntityId /*entity_id*/) {
            return entity_is_unit(entity.type) &&
                entity_is_selectable(entity) &&
                !(entity.type == ENTITY_BALLOON || 
                    (entity.type == ENTITY_WAGON && entity.garrisoned_units.empty())) &&
                state->players[entity.player_id].team != state->players[base_info.controlling_player].team &&
                entity_is_visible_to_player(state, entity, base_info.controlling_player) &&
                ivec2::manhattan_distance(entity.cell, goldmine.cell) < BOT_NEAR_DISTANCE;
        });

        base_info.is_under_attack = nearby_enemy_id != ID_NULL;
    }
}

MatchInput bot_scout(const MatchState* state, Bot& bot, uint32_t match_timer) {
    ZoneScoped;
    
    if (bot.scout_id == ID_NULL) {
        if (!bot_should_scout(bot, match_timer)) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        bot.entities_to_scout = bot_determine_entities_to_scout(state, bot);
        if (bot.entities_to_scout.empty()) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        // Find a scout
        bot.scout_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
            .filter = [&bot](const Entity& entity, EntityId /*entity_id*/) {
                return entity.player_id == bot.player_id &&
                    (entity.type == ENTITY_WAGON || entity.type == ENTITY_MINER) &&
                    entity.garrison_id == ID_NULL &&
                    !(entity.target.type == TARGET_BUILD || entity.target.type == TARGET_REPAIR || entity.target.type == TARGET_UNLOAD) &&
                    entity.garrisoned_units.empty() &&
                    !entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED);
            },
            .compare = [](const Entity&a, const Entity& b) {
                if (a.type == ENTITY_WAGON && b.type != ENTITY_WAGON) {
                    return true;
                }
                return false;
            }
        });
        if (bot.scout_id == ID_NULL) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        // Reserve the scout, but not if we're bandit rushing so that we can use the wagon for the rush
        if (!bot_is_bandit_rushing(bot)) {
            bot_reserve_entity(bot, bot.scout_id);
        }

        log_debug("BOT %u scout, begin scouting.", bot.player_id);
    }

    const Entity& scout = state->entities.get_by_id(bot.scout_id);

    // Remove scouted and dead entities from the to-scout list
    {
        uint32_t entities_to_scout_index = 0;
        while (entities_to_scout_index < bot.entities_to_scout.size()) {
            EntityId entity_id = bot.entities_to_scout[entities_to_scout_index];
            uint32_t entity_index = state->entities.get_index_of(entity_id);
            if (entity_index == INDEX_INVALID || 
                    !entity_is_selectable(state->entities[entity_index]) ||
                    bot_has_scouted_entity(state, bot, state->entities[entity_index], entity_id)) {
                bot.entities_to_scout.remove_at_unordered(entities_to_scout_index);
            } else {
                entities_to_scout_index++;
            }
        }
    }

    // Finish scout
    if (bot.entities_to_scout.empty()) {
        EntityId scout_id = bot.scout_id;
        if (!bot_release_scout(state, bot)) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        bot.last_scout_time = match_timer;
        log_debug("BOT %u scout, finish scout", bot.player_id);
        return bot_return_entity_to_nearest_hall(state, bot, scout_id);
    }

    // Flee if taking damage
    if (scout.taking_damage_timer != 0) {
        EntityId attacker_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
            .filter = [&bot](const Entity& entity, EntityId /*entity_id*/) {
                return entity_is_unit(entity.type) &&
                    entity.health != 0 &&
                    entity.target.type == TARGET_ATTACK_ENTITY &&
                    entity.target.id == bot.scout_id;
            },
            .compare = match_compare_closest_manhattan_distance_to(scout.cell)
        });

        if (attacker_id != ID_NULL) {
            const Entity& attacker = state->entities.get_by_id(attacker_id);

            // Check if close to a hall
            EntityId hall_nearest_to_attacker_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
                .filter = [&attacker](const Entity& hall, EntityId /*hall_id*/) {
                    return hall.type == ENTITY_HALL &&
                        entity_is_selectable(hall) &&
                        hall.player_id == attacker.player_id &&
                        ivec2::manhattan_distance(hall.cell, attacker.cell) < BOT_NEAR_DISTANCE;
                },
                .compare = match_compare_closest_manhattan_distance_to(attacker.cell)
            });
            if (bot.entities_to_scout.contains(hall_nearest_to_attacker_id)) {
                bot_assume_entity_is_scouted(bot, hall_nearest_to_attacker_id);
            }

            // Check if close to a goldmine
            EntityId goldmine_nearest_to_attacker_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
                .filter = [&attacker](const Entity& goldmine, EntityId /*goldmine_id*/) {
                    return goldmine.type == ENTITY_GOLDMINE &&
                        ivec2::manhattan_distance(goldmine.cell, attacker.cell) < BOT_NEAR_DISTANCE;
                },
                .compare = match_compare_closest_manhattan_distance_to(attacker.cell)
            });
            if (bot.entities_to_scout.contains(goldmine_nearest_to_attacker_id)) {
                bot_assume_entity_is_scouted(bot, goldmine_nearest_to_attacker_id);
            }

            // Remove any to-scouted entities that are close to the attacker
            // And also remove the current to-scout entity
            uint32_t index = 0;
            while (index < bot.entities_to_scout.size()) {
                EntityId to_scout_id = bot.entities_to_scout[index];
                const Entity& to_scout = state->entities.get_by_id(to_scout_id);
                if (scout.target.id == to_scout_id || 
                        ivec2::manhattan_distance(to_scout.cell, attacker.cell) < BOT_NEAR_DISTANCE) {
                    bot.entities_to_scout.remove_at_unordered(index);
                } else {
                    index++;
                }
            }
        }
    
        MatchInput flee_input = bot_unit_flee(state, bot, bot.scout_id);
        // Check whether scout is already fleeing so that we don't spam flee input
        if (flee_input.type == MATCH_INPUT_MOVE_CELL && scout.target.type == TARGET_CELL &&
                ivec2::manhattan_distance(flee_input.move.target_cell, scout.target.cell) < BOT_NEAR_DISTANCE) {
            return (MatchInput) { .type = MATCH_INPUT_NONE };
        }

        log_debug("BOT %u scout, flee danger.", bot.player_id);
        return flee_input;
    }

    // Resolve deadlock if blocked by another scout
    if (scout.target.type == TARGET_ENTITY && scout.pathfind_attempts == 2) {
        log_debug("BOT %u scout, return to hall to resolve deadlock.", bot.player_id, bot.scout_id);
        return bot_return_entity_to_nearest_hall(state, bot, bot.scout_id);
    }

    // Determine closest entity to scout
    uint32_t closest_unscouted_entity_index = INDEX_INVALID;
    int closest_unscouted_entity_distance;
    for (uint32_t entities_to_scout_index = 0; entities_to_scout_index < bot.entities_to_scout.size(); entities_to_scout_index++) {
        EntityId entity_id = bot.entities_to_scout[entities_to_scout_index];
        uint32_t entity_index = state->entities.get_index_of(entity_id);

        int entity_distance = ivec2::manhattan_distance(state->entities[entity_index].cell, scout.cell);
        if (closest_unscouted_entity_index == INDEX_INVALID || 
                (entity_distance < closest_unscouted_entity_distance && std::abs(entity_distance - closest_unscouted_entity_distance) > BOT_NEAR_DISTANCE)) {
            closest_unscouted_entity_index = entity_index;
            closest_unscouted_entity_distance = entity_distance;
        }
    }
    EntityId unscouted_entity_id = state->entities.get_id_of(closest_unscouted_entity_index);
    GOLD_ASSERT(unscouted_entity_id != ID_NULL);

    // Move towards unscouted entity
    if (!(scout.target.type == TARGET_ENTITY && scout.target.id == unscouted_entity_id)) {
        MatchInput input;
        input.type = MATCH_INPUT_MOVE_ENTITY;
        input.move.shift_command = 0;
        input.move.target_id = unscouted_entity_id;
        input.move.target_cell = ivec2(0, 0);
        input.move.entity_count = 1;
        input.move.entity_ids[0] = bot.scout_id;

        log_debug("BOT %u scout, move towards unscouted entity %u", bot.player_id, unscouted_entity_id);
        return input;
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

EntityList bot_determine_entities_to_scout(const MatchState* state, const Bot& bot) {
    return match_find_entities(state, [&state, &bot](const Entity& entity, EntityId entity_id) {
        // Filter down to halls, workshops, and sheriff's offices
        if (!(entity.type == ENTITY_GOLDMINE ||
                entity.type == ENTITY_HALL ||
                entity.type == ENTITY_WORKSHOP ||
                entity.type == ENTITY_SHERIFFS)) {
            return false;
        }

        // Filter down to unscouted entities only
        if (bot_has_scouted_entity(state, bot, entity, entity_id)) {
            return false;
        }

        return true;
    });
}

void bot_assume_entity_is_scouted(Bot& bot, EntityId entity_id) {
    if (bot.entities_assumed_to_be_scouted.is_full()) {
        log_warn("BOT %u could not assume that entity %u was scouted because entities_assumed_to_be_scouted is full.", bot.player_id, entity_id);
        return;
    }
    bot.entities_assumed_to_be_scouted.push_back(entity_id);
    log_debug("BOT %u assumes entity %u is scouted.", bot.player_id, entity_id);
}

void bot_prune_entities_assumed_to_be_scouted_list(const MatchState* state, Bot& bot) {
    uint32_t index = 0;
    while (index < bot.entities_assumed_to_be_scouted.size()) {
        EntityId entity_id = bot.entities_assumed_to_be_scouted[index];
        uint32_t entity_index = state->entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID || bot_has_scouted_entity(state, bot, state->entities[entity_index], entity_id, false)) {
            log_debug("BOT %u no longer needs to assume entity %u is scouted. Entity index %u", bot.player_id, entity_id, entity_index);
            bot.entities_assumed_to_be_scouted.remove_at_unordered(index);
        } else {
            index++;
        }
    }
}

bool bot_release_scout(const MatchState* state, Bot& bot) {
    if (entity_check_flag(state->entities.get_by_id(bot.scout_id), ENTITY_FLAG_IS_RESERVED)) {
        if (!bot_release_entity(bot, bot.scout_id)) {
            return false;
        }
    }

    bot.scout_id = ID_NULL;
    return true;
}

bool bot_should_scout(const Bot& bot, uint32_t match_timer) {
    if (!bitflag_check(bot.config.flags, BOT_CONFIG_SHOULD_SCOUT)) {
        return false;
    }

    uint32_t next_scout_time;
    if (bot.last_scout_time == 0U)  {
        next_scout_time = 0U;
    } else if (match_timer < 10U * 60U * UPDATES_PER_SECOND) {
        next_scout_time = bot.last_scout_time + (2U * 60U * UPDATES_PER_SECOND);
    } else {
        next_scout_time = bot.last_scout_time + (5U * 60U * UPDATES_PER_SECOND);
    }

    return match_timer >= next_scout_time;
}

// ENTITY RESERVATION

bool bot_reserve_entity(Bot& bot, EntityId entity_id) {
    if (bot.reservation_requests.is_full()) {
        log_warn("BOT %u: could not reserve entity %u. Reservation requests list is full.", bot.player_id, entity_id);
        return false;
    }
    bot.reservation_requests.push((BotReservationRequest) {
        .entity_id = entity_id,
        .value = true
    });
    log_debug("BOT %u: reserved entity %u", bot.player_id, entity_id);
    return true;
}

bool bot_release_entity(Bot& bot, EntityId entity_id) {
    if (bot.reservation_requests.is_full()) {
        log_warn("BOT %u: could not release entity %u. Reservation requests list is full.", bot.player_id, entity_id);
        return false;
    }
    bot.reservation_requests.push((BotReservationRequest) {
        .entity_id = entity_id,
        .value = false
    });
    log_debug("BOT %u: released entity %u", bot.player_id, entity_id);
    return true;
}

// ENTITY TYPE UTILS

EntityType bot_get_building_which_trains(EntityType unit_type) {
    switch (unit_type) {
        case ENTITY_MINER:
            return ENTITY_HALL;
        case ENTITY_COWBOY:
        case ENTITY_BANDIT:
            return ENTITY_SALOON;
        case ENTITY_SAPPER:
        case ENTITY_PYRO:
        case ENTITY_BALLOON:
            return ENTITY_WORKSHOP;
        case ENTITY_JOCKEY:
        case ENTITY_WAGON:
            return ENTITY_COOP;
        case ENTITY_SOLDIER:
        case ENTITY_CANNON:
            return ENTITY_BARRACKS;
        case ENTITY_DETECTIVE:
            return ENTITY_SHERIFFS;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

EntityType bot_get_building_prereq(EntityType building_type) {
    switch (building_type) {
        case ENTITY_HALL:
            return ENTITY_TYPE_COUNT;
        case ENTITY_SALOON:
        case ENTITY_HOUSE:
            return ENTITY_HALL;
        case ENTITY_BUNKER:
        case ENTITY_WORKSHOP:
        case ENTITY_SMITH:
            return ENTITY_SALOON;
        case ENTITY_COOP:
        case ENTITY_BARRACKS:
        case ENTITY_SHERIFFS:
            return ENTITY_SMITH;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

EntityType bot_get_building_which_researches(uint32_t upgrade) {
    switch (upgrade) {
        case UPGRADE_WAR_WAGON:
        case UPGRADE_BAYONETS:
        case UPGRADE_SERRATED_KNIVES:
        case UPGRADE_IRON_SIGHTS:
            return ENTITY_SMITH;
        case UPGRADE_STAKEOUT:
        case UPGRADE_PRIVATE_EYE:
            return ENTITY_SHERIFFS;
        case UPGRADE_LANDMINES:
        case UPGRADE_TAILWIND:
            return ENTITY_WORKSHOP;
        default:
            GOLD_ASSERT(false);
            return ENTITY_TYPE_COUNT;
    }
}

// METRICS

EntityCount bot_count_in_progress_entities(const MatchState* state, const Bot& bot) {
    EntityCount count;

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id != bot.player_id || entity.health == 0) {
            continue;
        }

        if (entity.type == ENTITY_MINER && entity.target.type == TARGET_BUILD) {
            count[entity.target.build.building_type]++;
        }
        if (entity.mode == MODE_BUILDING_FINISHED &&
                !entity.queue.empty() &&
                entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) {
            count[entity.queue[0].unit_type]++;
        }
    }

    return count;
}

EntityCount bot_count_unreserved_entities(const MatchState* state, const Bot& bot) {
    EntityCount count;

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];

        if (entity.player_id == bot.player_id &&
                entity.health != 0 &&
                !entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED)) {
            count[entity.type]++;
        }
    }

    return count;
}

EntityCount bot_count_unreserved_army(const MatchState* state, const Bot& bot) {
    EntityCount count;

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];

        if (bot_is_entity_unreserved_army(bot, entity)) {
            count[entity.type]++;
        }
    }

    return count;
}

bool bot_is_entity_unreserved_army(const Bot& bot, const Entity& entity) {
    if (!entity_is_unit(entity.type) ||
            entity.type == ENTITY_MINER ||
            (entity.type == ENTITY_WAGON && bot.desired_army_ratio[ENTITY_WAGON] == 0)) {
        return false;
    }

    return entity.player_id == bot.player_id && 
        entity.health != 0 &&
        !entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED);
}

EntityCount bot_count_available_production_buildings(const MatchState* state, const Bot& bot) {
    EntityCount count;

    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (entity.player_id == bot.player_id &&
                entity.mode == MODE_BUILDING_FINISHED &&
                entity.queue.empty() &&
                bot_is_entity_type_production_building(entity.type)) {
            count[entity.type]++;
        }
    }
    return count;
}

int bot_score_unreserved_army(const MatchState* state, const Bot& bot) {
    int score = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];

        if (bot_is_entity_unreserved_army(bot, entity)) {
            score += bot_score_entity(state, bot, entity);
        }
    }

    return score;
}

int bot_score_allied_army(const MatchState* state, const Bot& bot) {
    int score = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (!entity_is_unit(entity.type) || entity.health == 0) {
            continue;
        }

        if (state->players[entity.player_id].team == state->players[bot.player_id].team) {
            score += bot_score_entity(state, bot, entity);
        }
    }

    return score;
}

int bot_score_enemy_army(const MatchState* state, const Bot& bot) {
    int score = 0;
    for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
        const Entity& entity = state->entities[entity_index];
        if (!entity_is_unit(entity.type) || entity.health == 0) {
            continue;
        }

        if (state->players[entity.player_id].team != state->players[bot.player_id].team) {
            score += bot_score_entity(state, bot, entity);
        }
    }


    return score;
}

bool bot_is_entity_type_production_building(EntityType type) {
    return type == ENTITY_SALOON ||
            type == ENTITY_WORKSHOP ||
            type == ENTITY_SMITH ||
            type == ENTITY_COOP ||
            type == ENTITY_BARRACKS ||
            type == ENTITY_SHERIFFS;
}

std::vector<EntityType> bot_entity_types_production_buildings() {
    std::vector<EntityType> types;
    for (uint32_t entity_type = 0; entity_type < ENTITY_HALL; entity_type++) {
        if (bot_is_entity_type_production_building((EntityType)entity_type)) {
            types.push_back((EntityType)entity_type);
        }
    }

    return types;
}

// MISC

EntityId bot_find_threatened_in_progress_building(const MatchState* state, const Bot& bot) {
    return match_find_entity(state, [&state, &bot](const Entity& building, EntityId /*building_id*/) {
        if (building.mode != MODE_BUILDING_IN_PROGRESS ||
                building.player_id != bot.player_id) {
            return false;
        }

        int nearby_ally_score = 0;
        int nearby_enemy_score = 0;
        for (uint32_t entity_index = 0; entity_index < state->entities.size(); entity_index++) {
            const Entity& entity = state->entities[entity_index];
            if (entity.health == 0 || 
                    ivec2::manhattan_distance(entity.cell, building.cell) > BOT_NEAR_DISTANCE ||
                    !entity_is_visible_to_player(state, entity, bot.player_id)) {
                continue;
            }

            if (state->players[entity.player_id].team == state->players[bot.player_id].team) {
                nearby_ally_score += bot_score_entity(state, bot, entity);
            } else {
                nearby_enemy_score += bot_score_entity(state, bot, entity);
            }
        }

        if (nearby_enemy_score < BOT_UNIT_SCORE) {
            return false;
        }
        if (building.health > 100 && nearby_enemy_score < 3 * BOT_UNIT_SCORE) {
            return false;
        }
        if (building.health > entity_get_data(building.type).max_health / 4 &&
                nearby_ally_score > nearby_enemy_score) {
            return false;
        }

        log_debug("BOT %u find_threatened_in_progress_building ally score %u enemy score %u", nearby_ally_score, nearby_enemy_score);
        return true;
    });
}

EntityId bot_find_building_in_need_of_repair(const MatchState* state, const Bot& bot) {
    
    return match_find_entity(state, [&state, &bot](const Entity& building, EntityId building_id) {
        // Filter down to on-fire, finished, owned buildings
        if (building.player_id != bot.player_id ||
                building.mode != MODE_BUILDING_FINISHED ||
                !entity_check_flag(building, ENTITY_FLAG_ON_FIRE)) {
            return false;
        }

        // Don't repair if already being repaired
        EntityId existing_repairer = match_find_entity(state, [&building_id](const Entity& repairer, EntityId /*repairer_id*/) {
            return repairer.target.type == TARGET_REPAIR && repairer.target.id == building_id;
        });
        if (existing_repairer != ID_NULL) {
            return false;
        }

        return true;
    });
}

MatchInput bot_repair_building(const MatchState* state, const Bot& bot, EntityId building_id) {
    const Entity& building = state->entities.get_by_id(building_id);

    // Find repairer
    EntityId repairer_id = bot_find_builder(state, bot, building.cell);
    if (repairer_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_REPAIR;
    input.move.shift_command = 0;
    input.move.target_cell = ivec2(0, 0);
    input.move.target_id = building_id;
    input.move.entity_count = 1;
    input.move.entity_ids[0] = repairer_id;

    log_debug("BOT %u repair_building, building id %u", bot.player_id, building_id);
    return input;
}

MatchInput bot_rein_in_stray_units(const MatchState* state, const Bot& bot) {
    EntityList stray_units = match_find_entities(state, [&state, &bot](const Entity& entity, EntityId /*entity_id*/) {
        // Filter down to owned, unreserved units
        if (!entity_is_unit(entity.type) ||
                !entity_is_selectable(entity) ||
                entity.player_id != bot.player_id ||
                entity_check_flag(entity, ENTITY_FLAG_IS_RESERVED)) {
            return false;
        }

        // Filter down to only attacking or idle units
        if (!(entity.target.type == TARGET_ATTACK_ENTITY || entity.target.type == TARGET_NONE)) {
            return false;
        }

        // If the unit is attacking something and its target is close, then don't rein it in
        uint32_t target_index = entity.target.type == TARGET_ATTACK_ENTITY
            ? state->entities.get_index_of(entity.target.id)
            : INDEX_INVALID;
        if (target_index != INDEX_INVALID && 
                ivec2::manhattan_distance(entity.cell, state->entities[target_index].cell) < BOT_NEAR_DISTANCE) {
            return false;
        }

        // If the unit is already nearby an allied base, then skip it
        for (uint32_t base_info_index = 0; base_info_index < bot.base_info.size(); base_info_index++) {
            const BotBaseInfo& base_info = bot.base_info[base_info_index];
            if (base_info.controlling_player != bot.player_id) {
                continue;
            }

            const Entity& goldmine = state->entities.get_by_id(base_info.goldmine_id);
            if (ivec2::manhattan_distance(entity.cell, goldmine.cell) < BOT_MEDIUM_DISTANCE) {
                return false;
            }
        }

        return true;
    });

    if (stray_units.empty()) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input = bot_return_entity_to_nearest_hall(state, bot, stray_units.back());
    stray_units.pop_back();

    while (!stray_units.empty() && input.move.entity_count < SELECTION_LIMIT) {
        input.move.entity_ids[input.move.entity_count] = stray_units.back();
        input.move.entity_count++;
        stray_units.pop_back();
    }

    return input;
}

MatchInput bot_update_building_rally_point(const MatchState* state, const Bot& bot, EntityId building_id) {
    
    const Entity& building = state->entities.get_by_id(building_id);

    ivec2 rally_point = bot_choose_building_rally_point(state, bot, building);
    if (building.rally_point == rally_point) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput rally_input;
    rally_input.type = MATCH_INPUT_RALLY;
    rally_input.rally.building_count = 1;
    rally_input.rally.building_ids[0] = building_id;
    rally_input.rally.rally_point = rally_point;

    log_debug("BOT %u update_building_rally_point %u", bot.player_id, building_id);
    return rally_input;
}

ivec2 bot_choose_building_rally_point(const MatchState* state, const Bot& bot, const Entity& building) {
    if (building.type == ENTITY_HALL) {
        EntityId goldmine_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
            .filter = [](const Entity& goldmine, EntityId /*goldmine_id*/) {
                return goldmine.type == ENTITY_GOLDMINE;
            },
            .compare = match_compare_closest_manhattan_distance_to(building.cell)
        });
        const Entity& goldmine = state->entities.get_by_id(goldmine_id);
        return (goldmine.cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
    }

    std::vector<ivec2> frontier;
    std::vector<bool> explored(state->map.width * state->map.height, false);
    frontier.push_back(building.cell);
    ivec2 fallback_rally_cell = ivec2(-1, -1);
    int building_cell_size = entity_get_data(building.type).cell_size;
    Rect building_rect = (Rect) { 
        .x = building.cell.x,
        .y = building.cell.y,
        .w = building_cell_size,
        .h = building_cell_size
    };

    while (!frontier.empty()) {
        ivec2 next = frontier[0];
        frontier.erase(frontier.begin());

        if (explored[next.x + (next.y * state->map.width)]) {
            continue;
        }

        static const int RALLY_MARGIN = 4;
        if (bot_is_rally_cell_valid(state, next, RALLY_MARGIN, building_rect)) {
            return cell_center(next).to_ivec2();
        }
        if (fallback_rally_cell.x == -1 && bot_is_rally_cell_valid(state, next, 2, building_rect)) {
            fallback_rally_cell = next;
        }

        explored[next.x + (next.y * state->map.width)] = true;
        for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
            ivec2 child = next + DIRECTION_IVEC2[direction];
            if (!map_is_cell_in_bounds(state->map, child)) {
                continue;
            }
            if (map_is_tile_ramp(state->map, child)) {
                continue;
            }
            if (map_get_tile(state->map, next).elevation != map_get_tile(state->map, child).elevation) {
                continue;
            }
            frontier.push_back(child);
        }
    }

    if (fallback_rally_cell.x == -1) {
        log_warn("BOT %u choose_building_rally_point, no rally cell found.", bot.player_id);
        return fallback_rally_cell;
    }
    return cell_center(fallback_rally_cell).to_ivec2();
}

bool bot_is_rally_cell_valid(const MatchState* state, ivec2 rally_cell, int rally_margin, Rect origin_rect) {
    ivec2 origin = Rect::cell_in_a_nearest_to_b(origin_rect, (Rect) { .x = rally_cell.x, .y = rally_cell.y, .w = 1, .h = 1 });
    if (ivec2::manhattan_distance(rally_cell, origin) < 8) {
        return false;
    }

    for (int y = rally_cell.y - rally_margin; y < rally_cell.y + rally_margin + 1; y++) {
        for (int x = rally_cell.x - rally_margin; x < rally_cell.x + rally_margin + 1; x++) {
            ivec2 cell = ivec2(x, y);
            if (!map_is_cell_in_bounds(state->map, cell)) {
                return false;
            }
            if (map_is_tile_ramp(state->map, cell)) {
                return false;
            }
            Cell map_cell = map_get_cell(state->map, CELL_LAYER_GROUND, cell);
            if (map_cell.type == CELL_BUILDING || map_cell.type == CELL_GOLDMINE || map_cell.type == CELL_BLOCKED) {
                return false;
            }
        }
    }

    return true;
}

MatchInput bot_unload_unreserved_carriers(const MatchState* state, const Bot& bot) {
    EntityId carrier_id = match_find_entity(state, [&bot](const Entity& carrier, EntityId /*carrier_id*/) {
        return carrier.player_id == bot.player_id &&
                entity_is_selectable(carrier) &&
                !carrier.garrisoned_units.empty() &&
                carrier.target.type == TARGET_NONE &&
                !entity_check_flag(carrier, ENTITY_FLAG_IS_RESERVED);
    });
    if (carrier_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }
    
    MatchInput input;
    input.type = MATCH_INPUT_MOVE_UNLOAD;
    input.move.shift_command = 0;
    input.move.target_id = ID_NULL;
    input.move.target_cell = state->entities.get_by_id(carrier_id).cell;
    input.move.entity_ids[0] = carrier_id;
    input.move.entity_count = 1;
    return input;
}

// SCORE UTIL

int bot_score_entity(const MatchState* state, const Bot& bot, const Entity& entity) {
    if (entity_is_building(entity.type) && (entity.type != ENTITY_BUNKER || entity.mode == MODE_BUILDING_IN_PROGRESS)) {
        return 0;
    }
    if (!entity_is_selectable(entity)) {
        return 0;
    }
    // The miner is worth 1/4 of a military unit
    // Since the scores are ints, everything is multiplied by BOT_UNIT_SCORE to account for this
    switch (entity.type) {
        case ENTITY_MINER:
            return 1;
        case ENTITY_BUNKER: {
            int bunker_garrison_count;
            if (entity_is_visible_to_player(state, entity, bot.player_id)) {
                bunker_garrison_count = entity.garrisoned_units.size();
            } else {
                bunker_garrison_count = 4;
            } 

            return bunker_garrison_count * BOT_UNIT_SCORE_IN_BUNKER;
        }
        case ENTITY_WAGON:
            return entity.garrisoned_units.size() * BOT_UNIT_SCORE;
        case ENTITY_CANNON:
            return 3 * BOT_UNIT_SCORE;
        default:
            return BOT_UNIT_SCORE;
    }
}

int bot_score_entity_list(const MatchState* state, const Bot& bot, const EntityList& entity_list) {
    int score = 0;
    for (uint32_t entity_list_index = 0; entity_list_index < entity_list.size(); entity_list_index++) {
        EntityId entity_id = entity_list[entity_list_index];
        uint32_t entity_index = state->entities.get_index_of(entity_id);
        if (entity_index == INDEX_INVALID) {
            continue;
        }
        score += bot_score_entity(state, bot, state->entities[entity_index]);
    }

    return score;
}

// UTIL

bool bot_has_scouted_entity(const MatchState* state, const Bot& bot, const Entity& entity, EntityId entity_id, bool allow_assumed_scouts) {
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    uint32_t bot_team = state->players[bot.player_id].team;
    // If fog is revealed, then they can see it
    if (match_is_cell_rect_revealed(state, bot_team, entity.cell, entity_cell_size)) {
        return true;
    }
    // If for is only explored but they have seen the entity before, then they can see it
    if (match_is_cell_rect_explored(state, bot_team, entity.cell, entity_cell_size) &&
            match_team_remembers_entity(state, state->players[bot.player_id].team, entity_id)) {
        return true;
    }
    if (allow_assumed_scouts && bot.entities_assumed_to_be_scouted.contains(entity_id)) {
        return true;
    }
    // Otherwise, they have not scouted it
    return false;
}

EntityId bot_find_hall_surrounding_goldmine(const MatchState* state, const Bot& bot, const Entity& goldmine) {
    return match_find_entity(state, [&state, &bot, &goldmine](const Entity& hall, EntityId hall_id) {
        return hall.type == ENTITY_HALL &&
                entity_is_selectable(hall) &&
                bot_has_scouted_entity(state, bot, hall, hall_id) &&
                bot_does_entity_surround_goldmine(hall, goldmine.cell);
    });
}

bool bot_does_entity_surround_goldmine(const Entity& entity, ivec2 goldmine_cell) {
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    Rect entity_rect = (Rect) {
        .x = entity.cell.x, .y = entity.cell.y,
        .w = entity_cell_size, .h = entity_cell_size
    };

    Rect goldmine_rect = entity_goldmine_get_block_building_rect(goldmine_cell);
    goldmine_rect.x -= 1;
    goldmine_rect.y -= 1;
    goldmine_rect.w += 2;
    goldmine_rect.h += 2;
    
    return entity_rect.intersects(goldmine_rect);
}

MatchInput bot_return_entity_to_nearest_hall(const MatchState* state, const Bot& bot, EntityId entity_id) {
    const Entity& entity = state->entities.get_by_id(entity_id);

    EntityId nearest_hall_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [&bot](const Entity& hall, EntityId /*hall_id*/) {
            return hall.type == ENTITY_HALL &&
                entity_is_selectable(hall) &&
                hall.player_id == bot.player_id;
        },
        .compare = match_compare_closest_manhattan_distance_to(entity.cell)
    });

    if (nearest_hall_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    ivec2 nearest_hall_cell = state->entities.get_by_id(nearest_hall_id).cell;
    EntityId nearest_goldmine_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [](const Entity& goldmine, EntityId /*goldmine_id*/) {
            return goldmine.type == ENTITY_GOLDMINE;
        },
        .compare = match_compare_closest_manhattan_distance_to(nearest_hall_cell)
    });
    GOLD_ASSERT(nearest_goldmine_id != ID_NULL);

    ivec2 target_cell = bot_get_unoccupied_cell_near_goldmine(state, bot, nearest_goldmine_id);
    log_debug("BOT %u return_entity_to_nearest_hall, nearest_hall_id %u nearest_goldmine_id %u target_cell <%i, %i>", bot.player_id, nearest_hall_id, nearest_goldmine_id, target_cell.x, target_cell.y);

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = target_cell;
    input.move.target_id = ID_NULL;
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

MatchInput bot_unit_flee(const MatchState* state, const Bot& bot, EntityId entity_id) {
    const Entity& entity = state->entities.get_by_id(entity_id);

    EntityId nearest_hall_id = match_find_best_entity(state, (MatchFindBestEntityParams) {
        .filter = [&bot](const Entity& hall, EntityId /*hall_id*/) {
            return hall.type == ENTITY_HALL &&
                entity_is_selectable(hall) &&
                hall.player_id == bot.player_id;
        },
        .compare = match_compare_closest_manhattan_distance_to(entity.cell)
    });

    if (nearest_hall_id == ID_NULL) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    int entity_size = entity_get_data(entity.type).cell_size;
    ivec2 target_cell = map_get_nearest_cell_around_rect(state->map, CELL_LAYER_GROUND, entity.cell, entity_size, state->entities.get_by_id(nearest_hall_id).cell, entity_get_data(ENTITY_HALL).cell_size, MAP_OPTION_IGNORE_UNITS);

    static const int FLEE_DISTANCE = 16;
    if (ivec2::manhattan_distance(entity.cell, target_cell) < FLEE_DISTANCE &&
            map_get_tile(state->map, entity.cell).elevation == map_get_tile(state->map, target_cell).elevation) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MapPath path_from_hall;
    map_pathfind(state->map, CELL_LAYER_GROUND, target_cell, entity.cell, entity_size, MAP_OPTION_IGNORE_UNITS, NULL, &path_from_hall);

    if (path_from_hall.empty()) {
        return (MatchInput) { .type = MATCH_INPUT_NONE };
    }

    MatchInput input;
    input.type = MATCH_INPUT_MOVE_CELL;
    input.move.shift_command = 0;
    input.move.target_cell = path_from_hall.get_from_top(std::min(path_from_hall.size() - 1U, (uint32_t)FLEE_DISTANCE));
    input.move.target_id = ID_NULL;
    input.move.entity_ids[0] = entity_id;
    input.move.entity_count = 1;
    return input;
}

ivec2 bot_get_unoccupied_cell_near_goldmine(const MatchState* state, const Bot& bot, EntityId goldmine_id) {
    const Entity& goldmine = state->entities.get_by_id(goldmine_id);

    // To determine the retreat_cell, search around the nearby base
    std::vector<ivec2> frontier;
    std::vector<bool> is_explored(state->map.width * state->map.height, false);

    ivec2 near_goldmine_cell = goldmine.cell;
    EntityId hall_id = bot_find_hall_surrounding_goldmine(state, bot, goldmine);
    if (hall_id != ID_NULL) {
        const Entity& hall = state->entities.get_by_id(hall_id);
        near_goldmine_cell = (goldmine.cell + hall.cell) / 2;
    }

    frontier.push_back(near_goldmine_cell);
    while (!frontier.empty()) {
        uint32_t closest_index = 0;
        for (uint32_t index = 1; index < frontier.size(); index++) {
            if (ivec2::manhattan_distance(frontier[index], near_goldmine_cell) < 
                    ivec2::manhattan_distance(frontier[closest_index], near_goldmine_cell)) {
                closest_index = index;
            }
        }
        ivec2 next = frontier[closest_index];
        frontier[closest_index] = frontier.back();
        frontier.pop_back();

        if (is_explored[next.x + (next.y * state->map.width)]) {
            continue;
        }

        if (ivec2::manhattan_distance(next, near_goldmine_cell) > BOT_NEAR_DISTANCE &&
                map_is_cell_rect_in_bounds(state->map, next - ivec2(1, 1), 3) &&
                !map_is_cell_rect_occupied(state->map, CELL_LAYER_GROUND, next - ivec2(1, 1), 3)) {
            return next;
        }

        is_explored[next.x + (next.y * state->map.width)] = true;

        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
            ivec2 child = next + DIRECTION_IVEC2[direction];

            if (!map_is_cell_in_bounds(state->map, child)) {
                continue;
            }

            frontier.push_back(child);
        }
    }

    log_warn("BOT %u get_unoccupied_cell_near_goldmine, goldmine %u no retreat area found.", bot.player_id, goldmine_id);
    for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
        ivec2 cell = goldmine.cell + (DIRECTION_IVEC2[direction] * BOT_MEDIUM_DISTANCE);
        if (!map_is_cell_in_bounds(state->map, cell)) {
            continue;
        }
        return cell;
    }

    // Leaving this as a return value and backup but the directional for loop above
    // should always be able to find something to return
    GOLD_ASSERT(false);
    return near_goldmine_cell;
}

uint32_t bot_get_index_of_squad_of_type(const Bot& bot, BotSquadType type) {
    uint32_t index;
    for (index = 0; index < bot.squads.size(); index++) {
        if (bot.squads[index].type == type) {
            break;
        }
    }

    return index;
}

bool bot_has_squad_of_type(const Bot& bot, BotSquadType type) {
    return bot_get_index_of_squad_of_type(bot, type) != bot.squads.size();
}

bool bot_has_desired_squad_of_type(const Bot& bot, BotSquadType type) {
    for (uint32_t desired_squad_index = 0; desired_squad_index < bot.desired_squads.size(); desired_squad_index++) {
        const BotDesiredSquad& squad = bot.desired_squads[desired_squad_index];
        if (squad.type == type) {
            return true;
        }
    }

    return false;
}

bool bot_is_bandit_rushing(const Bot& bot) {
    if (bot.unit_comp != BOT_UNIT_COMP_NONE) {
        return false;
    }

    for (uint32_t desired_squad_index = 0; desired_squad_index < bot.desired_squads.size(); desired_squad_index++) {
        const BotDesiredSquad& desired_squad = bot.desired_squads[desired_squad_index];
        if (desired_squad.entity_count[ENTITY_WAGON] == 1 && 
                desired_squad.entity_count[ENTITY_BANDIT] == 4 &&
                desired_squad.entity_count.count() == 5) {
            return true;
        }
    }

    return false;
}

bool bot_is_area_safe(const MatchState* state, const Bot& bot, ivec2 cell) {
    EntityId nearby_enemy_id = match_find_entity(state, [&state, &bot, &cell](const Entity& entity, EntityId /*entity_id*/) {
            return entity.type != ENTITY_GOLDMINE &&
                    entity_is_selectable(entity) &&
                    state->players[entity.player_id].team != state->players[bot.player_id].team &&
                    ivec2::manhattan_distance(entity.cell, cell) < BOT_NEAR_DISTANCE;
    });

    return nearby_enemy_id == ID_NULL;
}

void bot_queue_set_building_rally_point(Bot& bot, EntityId building_id) {
    // Don't queue a building that is already queued
    for (uint32_t queue_index = 0; queue_index < bot.buildings_to_set_rally_points.size(); queue_index++) {
        if (bot.buildings_to_set_rally_points[queue_index] == building_id) {
            log_warn("BOT %u queue_set_building_rally_point, building_id %u is already in queue.", bot.player_id, building_id);
            return;
        }
    }

    // Don't enqueue if the queue is full
    if (bot.buildings_to_set_rally_points.is_full()) {
        log_warn("BOT %u queue_set_building_rally_point, building_id %u, queue is full.", bot.player_id, building_id);
        return;
    }

    bot.buildings_to_set_rally_points.push(building_id);
}