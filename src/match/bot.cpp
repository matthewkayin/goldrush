#include "bot.h"

#include "core/logger.h"

Bot bot_init(const MatchState& state, uint8_t player_id, int32_t lcg_seed) {
    Bot bot;
    bot.player_id = player_id;

    // The bot takes a copy of the random seed, that way
    // it can make deterministic random decisions that will be
    // synced across each player's computer, but since the 
    // bot code will not be rerun during a replay, the bot will
    // not mess up replay playback
    bot.lcg_seed = lcg_seed;

    return bot;
}

MatchInput bot_get_turn_input(const MatchState& state, Bot& bot, uint32_t match_time_minutes) {
    MatchInput saturate_bases_input = bot_saturate_bases(state, bot);
    if (saturate_bases_input.type != MATCH_INPUT_NONE) {
        return saturate_bases_input;
    }

    MatchInput rally_input = bot_set_rally_points(state, bot);
    if (rally_input.type != MATCH_INPUT_NONE) {
        return rally_input;
    }

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

MatchInput bot_saturate_bases(const MatchState& state, Bot& bot) {
    for (uint32_t goldmine_index = 0; goldmine_index < state.entities.size(); goldmine_index++) {
        const Entity& goldmine = state.entities[goldmine_index];
        EntityId goldmine_id = state.entities.get_id_of(goldmine_index);
        if (goldmine.type != ENTITY_GOLDMINE || goldmine.gold_held == 0) {
            continue;
        }

        Rect goldmine_rect = entity_goldmine_get_block_building_rect(goldmine.cell);
        goldmine_rect.x -= 1;
        goldmine_rect.y -= 1;
        goldmine_rect.w += 2;
        goldmine_rect.h += 2;

        EntityId hall_id = bot_find_entity((BotFindEntityParams) {
            .state = state,
            .filter = [&goldmine_rect, &bot](const Entity& hall, EntityId hall_id) {
                if (hall.player_id != bot.player_id || hall.type != ENTITY_HALL || hall.mode != MODE_BUILDING_FINISHED) {
                    return false;
                }

                int hall_cell_size = entity_get_data(hall.type).cell_size;
                Rect entity_rect = (Rect) {
                    .x = hall.cell.x, .y = hall.cell.y,
                    .w = hall_cell_size, .h = hall_cell_size
                };

                return entity_rect.intersects(goldmine_rect);
            }
        });

        // If there is no hall, tell any miners to stop
        // This handles the case that the town hall is destroyed
        if (hall_id == ID_NULL) {
            MatchInput input;
            input.type = MATCH_INPUT_STOP;
            input.stop.entity_count = 0;

            for (uint32_t miner_index = 0; miner_index < state.entities.size(); miner_index++) {
                const Entity& miner = state.entities[miner_index];
                if (miner.player_id == bot.player_id && miner.goldmine_id == goldmine_id) {
                    input.stop.entity_ids[input.stop.entity_count] = state.entities.get_id_of(miner_index);
                    input.stop.entity_count++;
                }
            }

            if (input.stop.entity_count != 0) {
                GOLD_ASSERT(input.stop.entity_count <= SELECTION_LIMIT);
                return input;
            }

            continue;
        }

        const Entity& hall = state.entities.get_by_id(hall_id);

        // Check the miner count
        uint32_t miner_count = match_get_miners_on_gold(state, goldmine_id, bot.player_id);

        // If oversaturated, pull workers off gold
        if (miner_count > MATCH_MAX_MINERS_ON_GOLD) {
            EntityId miner_id = bot_pull_worker_off_gold(state, bot, goldmine_id);
            if (miner_id != ID_NULL) {
                ivec2 goldmine_cell = state.entities.get_by_id(goldmine_id).cell;

                MatchInput input;
                input.type = MATCH_INPUT_MOVE_CELL;
                input.move.shift_command = 0;
                input.move.target_cell = map_clamp_cell(state.map, hall.cell + ((hall.cell - goldmine_cell) * -1));
                input.move.target_id = ID_NULL;
                input.move.entity_count = 1;
                input.move.entity_ids[0] = miner_id;
                return input;
            }
        }

        // If undersaturated, put workers on gold
        if (miner_count < MATCH_MAX_MINERS_ON_GOLD) {
            EntityId idle_worker_id = bot_find_nearest_idle_worker(state, bot, hall.cell);
            if (idle_worker_id != ID_NULL) {
                MatchInput input;
                input.type = MATCH_INPUT_MOVE_ENTITY;
                input.move.shift_command = 0;
                input.move.target_cell = ivec2(0, 0);
                input.move.target_id = goldmine_id;
                input.move.entity_count = 1;
                input.move.entity_ids[0] = idle_worker_id;
                return input;
            }

            // If we're still here, then there were no idle workers
            // So we'll create one out of the town hall
            if (bot_get_effective_gold(state, bot) >= entity_get_data(ENTITY_MINER).gold_cost && hall.queue.empty()) {
                return (MatchInput) {
                    .type = MATCH_INPUT_BUILDING_ENQUEUE,
                    .building_enqueue = (MatchInputBuildingEnqueue) {
                        .building_id = hall_id,
                        .item_type = BUILDING_QUEUE_ITEM_UNIT,
                        .item_subtype = ENTITY_MINER
                    }
                };
            }
        }
    } // End for each goldmine

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

MatchInput bot_set_rally_points(const MatchState& state, const Bot& bot) {
    for (uint32_t building_index = 0; building_index < state.entities.size(); building_index++) {
        const Entity& building = state.entities[building_index];
        EntityId building_id = state.entities.get_id_of(building_index);

        // Filter down to allied buildings which are training a unit
        if (building.player_id != bot.player_id ||
                !entity_is_selectable(building) ||
                building.queue.empty() || building.queue.front().type != BUILDING_QUEUE_ITEM_UNIT) {
            continue;
        }

        // Halls only need their rally point set once
        if (building.type == ENTITY_HALL && building.rally_point.x != -1) {
            continue;
        }

        // Set hall rally point
        if (building.type == ENTITY_HALL) {
            EntityId goldmine_id = bot_find_best_entity((BotFindBestEntityParams) {
                .state = state,
                .filter = [](const Entity& goldmine, EntityId goldmine_id) {
                    return goldmine.type == ENTITY_GOLDMINE;
                },
                .compare = bot_closest_manhattan_distance_to(building.cell)
            });

            MatchInput rally_input;
            rally_input.type = MATCH_INPUT_RALLY;
            rally_input.rally.building_count = 1;
            rally_input.rally.building_ids[0] = building_id;
            rally_input.rally.rally_point = (state.entities.get_by_id(goldmine_id).cell * TILE_SIZE) + ivec2((3 * TILE_SIZE) / 2, (3 * TILE_SIZE) / 2);
            return rally_input;
        }
    } // End for each building

    return (MatchInput) { .type = MATCH_INPUT_NONE };
}

// Entity management

bool bot_is_entity_reserved(const Bot& bot, EntityId entity_id) {
    return bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end();
}

void bot_reserve_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: reserve entity %u", entity_id);
    bot.is_entity_reserved[entity_id] = true;
}

void bot_release_entity(Bot& bot, EntityId entity_id) {
    log_trace("BOT: release entity %u", entity_id);
    GOLD_ASSERT(bot.is_entity_reserved.find(entity_id) != bot.is_entity_reserved.end());
    bot.is_entity_reserved.erase(entity_id);
}

EntityId bot_find_entity(BotFindEntityParams params) {
    for (uint32_t entity_index = 0; entity_index < params.state.entities.size(); entity_index++) {
        const Entity& entity = params.state.entities[entity_index];
        EntityId entity_id = params.state.entities.get_id_of(entity_index);
        if (params.filter(entity, entity_id)) {
            return entity_id;
        }
    }

    return ID_NULL;
}

EntityId bot_find_best_entity(BotFindBestEntityParams params) {
    uint32_t best_entity_index = INDEX_INVALID;
    for (uint32_t entity_index = 0; entity_index < params.state.entities.size(); entity_index++) {
        const Entity& entity = params.state.entities[entity_index];
        EntityId entity_id = params.state.entities.get_id_of(entity_index);

        if (!params.filter(entity, entity_id)) {
            continue;
        }
        if (best_entity_index == INDEX_INVALID || params.compare(entity, params.state.entities[best_entity_index])) {
            best_entity_index = entity_index;
        }
    }

    if (best_entity_index == INDEX_INVALID) {
        return ID_NULL;
    }
    return params.state.entities.get_id_of(best_entity_index);
}

std::function<bool(const Entity&, const Entity&)> bot_closest_manhattan_distance_to(ivec2 cell) {
    return [&cell](const Entity& a, const Entity& b) {
        return ivec2::manhattan_distance(a.cell, cell) < ivec2::manhattan_distance(b.cell, cell);
    };
}

EntityId bot_find_nearest_idle_worker(const MatchState& state, const Bot& bot, ivec2 cell) {
    return bot_find_best_entity((BotFindBestEntityParams) {
        .state = state,
        .filter = [&bot](const Entity& entity, EntityId entity_id) {
            return entity.type == ENTITY_MINER &&
                    entity.player_id == bot.player_id &&
                    entity.mode == MODE_UNIT_IDLE &&
                    entity.target.type == TARGET_NONE &&
                    !bot_is_entity_reserved(bot, entity_id);
        },
        .compare = bot_closest_manhattan_distance_to(cell)
    });
}

EntityId bot_pull_worker_off_gold(const MatchState& state, const Bot& bot, EntityId goldmine_id) {
    return bot_find_entity((BotFindEntityParams) {
        .state = state,
        .filter = [&state, &bot, &goldmine_id](const Entity& entity, EntityId entity_id) {
            return entity.player_id == bot.player_id &&
                    entity_is_selectable(entity) &&
                    match_is_entity_mining(state, entity) &&
                    entity.goldmine_id == goldmine_id;
        }
    });
}

// Helpers

uint32_t bot_get_effective_gold(const MatchState& state, const Bot& bot) {
    uint32_t effective_gold = state.players[bot.player_id].gold;

    for (const Entity& entity : state.entities) {
        if (entity.player_id == bot.player_id &&
                entity.type == ENTITY_MINER &&
                entity.mode != MODE_UNIT_BUILD &&
                entity.target.type == TARGET_BUILD) {
            uint32_t gold_cost = entity_get_data(entity.target.build.building_type).gold_cost;
            effective_gold = effective_gold > gold_cost ? effective_gold - gold_cost : 0;
        }
    }

    return effective_gold;
}