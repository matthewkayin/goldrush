#include "match.h"

#include "logger.h"
#include "lcg.h"
#include <unordered_map>
#include <algorithm>

static const uint32_t UNIT_MOVE_BLOCKED_DURATION = 30;
static const uint32_t UNIT_BUILD_TICK_DURATION = 6;
static const uint32_t UNIT_IN_MINE_DURATION = 60;
static const uint32_t UNIT_OUT_MINE_DURATION = 10;
static const uint32_t UNIT_MAX_GOLD_HELD = 10;
static const uint32_t UNIT_REPAIR_RATE = 4;
static const uint32_t ENTITY_BUNKER_FIRE_OFFSET = 10;
static const xy ENTITY_BUNKER_PARTICLE_OFFSETS[4] = { xy(3, 23), xy(11, 26), xy(20, 25), xy(28, 23) };
static const xy ENTITY_WAR_WAGON_DOWN_PARTICLE_OFFSETS[4] = { xy(14, 6), xy(17, 8), xy(21, 6), xy(24, 8) };
static const xy ENTITY_WAR_WAGON_UP_PARTICLE_OFFSETS[4] = { xy(16, 20), xy(18, 22), xy(21, 20), xy(23, 22) };
static const xy ENTITY_WAR_WAGON_RIGHT_PARTICLE_OFFSETS[4] = { xy(7, 18), xy(11, 19), xy(12, 20), xy(16, 18) };
static const uint32_t ENTITY_CANNOT_GARRISON = 0;
static const uint32_t UNIT_HEALTH_REGEN_DURATION = 64;
static const uint32_t UNIT_HEALTH_REGEN_DELAY = 10 * 60;
static const uint32_t MINE_ARM_DURATION = 16;
static const uint32_t MINE_PRIME_DURATION = 6 * 6;
static const int MINE_EXPLOSION_DAMAGE = 200;
static const int SOLDIER_BAYONET_DAMAGE = 4;
static const int SMOKE_BOMB_THROW_RANGE_SQUARED = 36;
static const int SMOKE_BOMB_COOLDOWN = 60 * 60;

// Building train time = (HP * 0.9) / 10

const std::unordered_map<EntityType, entity_data_t> ENTITY_DATA = {
    { ENTITY_MINER, (entity_data_t) {
        .name = "Miner",
        .sprite = SPRITE_UNIT_MINER,
        .ui_button = UI_BUTTON_UNIT_MINER,
        .cell_size = 1,

        .gold_cost = 50,
        .train_duration = 15,
        .max_health = 30,
        .sight = 7,
        .armor = 0,
        .attack_priority = 1,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),

            .damage = 3,
            .attack_cooldown = 22,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_COWBOY, (entity_data_t) {
        .name = "Cowboy",
        .sprite = SPRITE_UNIT_COWBOY,
        .ui_button = UI_BUTTON_UNIT_COWBOY,
        .cell_size = 1,

        .gold_cost = 75,
        .train_duration = 25,
        .max_health = 50,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),

            .damage = 8,
            .attack_cooldown = 40,
            .range_squared = 25,
            .min_range_squared = 1
        }
    }},
    { ENTITY_BANDIT, (entity_data_t) {
        .name = "Bandit",
        .sprite = SPRITE_UNIT_BANDIT,
        .ui_button = UI_BUTTON_UNIT_BANDIT,
        .cell_size = 1,

        .gold_cost = 50,
        .train_duration = 20,
        .max_health = 50,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 225),

            .damage = 6,
            .attack_cooldown = 15,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_WAGON, (entity_data_t) {
        .name = "Wagon",
        .sprite = SPRITE_UNIT_WAGON,
        .ui_button = UI_BUTTON_UNIT_WAGON,
        .cell_size = 2,

        .gold_cost = 200,
        .train_duration = 38,
        .max_health = 120,
        .sight = 9,
        .armor = 1,
        .attack_priority = 1,

        .garrison_capacity = 4,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 20),

            .damage = 0,
            .attack_cooldown = 0,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_WAR_WAGON, (entity_data_t) {
        .name = "War Wagon",
        .sprite = SPRITE_UNIT_WAR_WAGON,
        .ui_button = UI_BUTTON_UNIT_WAR_WAGON,
        .cell_size = 2,

        .gold_cost = 200,
        .train_duration = 38,
        .max_health = 120,
        .sight = 9,
        .armor = 2,
        .attack_priority = 1,

        .garrison_capacity = 4,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 20),

            .damage = 0,
            .attack_cooldown = 0,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_JOCKEY, (entity_data_t) {
        .name = "Jockey",
        .sprite = SPRITE_UNIT_JOCKEY,
        .ui_button = UI_BUTTON_UNIT_JOCKEY,
        .cell_size = 1,

        .gold_cost = 100,
        .train_duration = 30,
        .max_health = 100,
        .sight = 9,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 40),

            .damage = 8,
            .attack_cooldown = 30,
            .range_squared = 25,
            .min_range_squared = 1
        }
    }},
    { ENTITY_SAPPER, (entity_data_t) {
        .name = "Sapper",
        .sprite = SPRITE_UNIT_SAPPER,
        .ui_button = UI_BUTTON_UNIT_SAPPER,
        .cell_size = 1,

        .gold_cost = 100,
        .train_duration = 27,
        .max_health = 40,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 225),

            .damage = 200,
            .attack_cooldown = 15,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_TINKER, (entity_data_t) {
        .name = "Tinker",
        .sprite = SPRITE_UNIT_TINKER,
        .ui_button = UI_BUTTON_UNIT_TINKER,
        .cell_size = 1,

        .gold_cost = 150,
        .train_duration = 30,
        .max_health = 50,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = true,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),

            .damage = 0,
            .attack_cooldown = 15,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_SOLDIER, (entity_data_t) {
        .name = "Soldier",
        .sprite = SPRITE_UNIT_SOLDIER,
        .ui_button = UI_BUTTON_UNIT_SOLDIER,
        .cell_size = 1,

        .gold_cost = 125,
        .train_duration = 30,
        .max_health = 60,
        .sight = 7,
        .armor = 1,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 170),

            .damage = 15,
            .attack_cooldown = 30,
            .range_squared = 49,
            .min_range_squared = 4
        }
    }},
    { ENTITY_CANNON, (entity_data_t) {
        .name = "Cannoneer",
        .sprite = SPRITE_UNIT_CANNON,
        .ui_button = UI_BUTTON_UNIT_CANNON,
        .cell_size = 2,

        .gold_cost = 200,
        .train_duration = 45,
        .max_health = 100,
        .sight = 7,
        .armor = 1,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(0, 140),

            .damage = 20,
            .attack_cooldown = 60,
            .range_squared = 49,
            .min_range_squared = 9
        }
    }},
    { ENTITY_SPY, (entity_data_t) {
        .name = "Detective",
        .sprite = SPRITE_UNIT_SPY,
        .ui_button = UI_BUTTON_UNIT_SPY,
        .cell_size = 1,

        .gold_cost = 175,
        .train_duration = 30,
        .max_health = 50,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = true,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 170),

            .damage = 6,
            .attack_cooldown = 45,
            .range_squared = 25,
            .min_range_squared = 1
        }
    }},
    { ENTITY_HALL, (entity_data_t) {
        .name = "Town Hall",
        .sprite = SPRITE_BUILDING_HALL,
        .ui_button = UI_BUTTON_BUILD_HALL,
        .cell_size = 4,

        .gold_cost = 400,
        .train_duration = 0,
        .max_health = 840,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 16, 23, 7 },
            .builder_positions_y = { 43, 25, 22 },
            .builder_flip_h = { false, true, false },
            .can_rally = true
        }
    }},
    { ENTITY_CAMP, (entity_data_t) {
        .name = "Mining Camp",
        .sprite = SPRITE_BUILDING_CAMP,
        .ui_button = UI_BUTTON_BUILD_CAMP,
        .cell_size = 2,

        .gold_cost = 150,
        .train_duration = 0,
        .max_health = 400,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 1, 15, 14 },
            .builder_positions_y = { 13, 13, 2 },
            .builder_flip_h = { false, true, false },
            .can_rally = false
        }
    }},
    { ENTITY_HOUSE, (entity_data_t) {
        .name = "House",
        .sprite = SPRITE_BUILDING_HOUSE,
        .ui_button = UI_BUTTON_BUILD_HOUSE,
        .cell_size = 2,

        .gold_cost = 100,
        .train_duration = 0,
        .max_health = 300,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 3, 16, -4 },
            .builder_positions_y = { 15, 15, 3 },
            .builder_flip_h = { false, true, false },
            .can_rally = false
        }
    }},
    { ENTITY_SALOON, (entity_data_t) {
        .name = "Saloon",
        .sprite = SPRITE_BUILDING_SALOON,
        .ui_button = UI_BUTTON_BUILD_SALOON,
        .cell_size = 3,

        .gold_cost = 150,
        .train_duration = 0,
        .max_health = 600,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 6, 27, 9 },
            .builder_positions_y = { 32, 27, 9 },
            .builder_flip_h = { false, true, false },
            .can_rally = true
        }
    }},
    { ENTITY_BUNKER, (entity_data_t) {
        .name = "Bunker",
        .sprite = SPRITE_BUILDING_BUNKER,
        .ui_button = UI_BUTTON_BUILD_BUNKER,
        .cell_size = 2,

        .gold_cost = 100,
        .train_duration = 0,
        .max_health = 200,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 4,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 1, 14, 5 },
            .builder_positions_y = { 15, 9, -3 },
            .builder_flip_h = { false, true, false },
            .can_rally = false
        }
    }},
    { ENTITY_COOP, (entity_data_t) {
        .name = "Chicken Coop",
        .sprite = SPRITE_BUILDING_COOP,
        .ui_button = UI_BUTTON_BUILD_COOP,
        .cell_size = 3,

        .gold_cost = 150,
        .train_duration = 0,
        .max_health = 500,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 9, 27, 26 },
            .builder_positions_y = { 24, 18, 4 },
            .builder_flip_h = { false, true, true },
            .can_rally = true
        }
    }},
    { ENTITY_SMITH, (entity_data_t) {
        .name = "Blacksmith",
        .sprite = SPRITE_BUILDING_SMITH,
        .ui_button = UI_BUTTON_BUILD_SMITH,
        .cell_size = 3,

        .gold_cost = 250,
        .train_duration = 0,
        .max_health = 560,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 10, 28, 28 },
            .builder_positions_y = { 29, 17, 4 },
            .builder_flip_h = { false, true, true },
            .can_rally = false
        }
    }},
    { ENTITY_BARRACKS, (entity_data_t) {
        .name = "Barracks",
        .sprite = SPRITE_BUILDING_BARRACKS,
        .ui_button = UI_BUTTON_BUILD_BARRACKS,
        .cell_size = 3,

        .gold_cost = 300,
        .train_duration = 0,
        .max_health = 600,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 6, 27, 3 },
            .builder_positions_y = { 27, 7, 3 },
            .builder_flip_h = { false, true, false },
            .can_rally = true
        }
    }},
    { ENTITY_SHERIFFS, (entity_data_t) {
        .name = "Sheriff's Office",
        .sprite = SPRITE_BUILDING_SHERIFFS,
        .ui_button = UI_BUTTON_BUILD_SHERIFFS,
        .cell_size = 3,

        .gold_cost = 200,
        .train_duration = 0,
        .max_health = 560,
        .sight = 7,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 6, 27, 14 },
            .builder_positions_y = { 27, 7, 1 },
            .builder_flip_h = { false, true, false },
            .can_rally = true
        }
    }},
    { ENTITY_LAND_MINE, (entity_data_t) {
        .name = "Land Mine",
        .sprite = SPRITE_BUILDING_MINE,
        .ui_button = UI_BUTTON_BUILD_MINE,
        .cell_size = 1,

        .gold_cost = 50,
        .train_duration = 0,
        .max_health = 5,
        .sight = 3,
        .armor = 0,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .building_data = (building_data_t) {
            .builder_positions_x = { 10, 28, 28 },
            .builder_positions_y = { 29, 17, 4 },
            .builder_flip_h = { false, true, true },
            .can_rally = false
        }
    }},
    { ENTITY_GOLD_MINE, (entity_data_t) {
        .name = "Gold Mine",
        .sprite = SPRITE_GOLD_MINE,
        .ui_button = UI_BUTTON_MINE,
        .cell_size = 3,

        .gold_cost = 0,
        .train_duration = 0,
        .max_health = 0,
        .sight = 0,
        .armor = 0,
        .attack_priority = 0,

        .garrison_capacity = 4,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false
    }}
};

entity_id entity_create(match_state_t& state, EntityType type, uint8_t player_id, xy cell) {
    auto entity_data_it = ENTITY_DATA.find(type);
    GOLD_ASSERT_MESSAGE(entity_data_it != ENTITY_DATA.end(), "Entity data not defined for this entity");
    const entity_data_t& entity_data = entity_data_it->second;

    entity_t entity;
    entity.type = type;
    entity.mode = entity_is_unit(type) ? MODE_UNIT_IDLE : MODE_BUILDING_IN_PROGRESS;
    entity.player_id = player_id;
    entity.flags = 0;

    entity.cell = cell;
    entity.position = entity_is_unit(type) 
                        ? entity_get_target_position(entity)
                        : xy_fixed(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.health = (entity_is_unit(type) || entity.type == ENTITY_LAND_MINE) ? entity_data.max_health : entity_data.max_health / 10;
    entity.target = (target_t) { .type = TARGET_NONE };
    entity.pathfind_attempts = 0;
    entity.timer = 0;
    entity.rally_point = xy(-1, -1);

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = 0;

    entity.taking_damage_counter = 0;
    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;

    if (entity.type == ENTITY_LAND_MINE) {
        entity.timer = MINE_ARM_DURATION;
        entity.mode = MODE_MINE_ARM;
    }
    if (entity.type == ENTITY_SPY) {
        entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, true);
    }

    entity_id id = state.entities.push_back(entity);
    if (entity.type == ENTITY_LAND_MINE) {
        state.map_mine_cells[entity.cell.x + (entity.cell.y * state.map_width)] = id;
    } else {
        map_set_cell_rect(state, entity.cell, entity_cell_size(type), id);
    }
    map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, true, ENTITY_DATA.at(entity.type).has_detection);

    log_trace("created entity id %u type %s", id, ENTITY_DATA.at(entity.type).name);

    return id;
}

entity_id entity_create_gold_mine(match_state_t& state, xy cell, uint32_t gold_left) {
    entity_t entity;
    entity.type = ENTITY_GOLD_MINE;
    entity.player_id = PLAYER_NONE;
    entity.flags = 0;
    entity.mode = MODE_GOLD_MINE;

    entity.cell = cell;
    entity.position = xy_fixed(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.garrison_id = ID_NULL;
    entity.gold_held = gold_left;

    entity_id id = state.entities.push_back(entity);
    map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);

    return id;
}

void entity_update(match_state_t& state, uint32_t entity_index) {
    entity_id id = state.entities.get_id_of(entity_index);
    entity_t& entity = state.entities[entity_index];
    const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

    if (entity_should_die(entity)) {
        if (entity_is_unit(entity.type)) {
            entity.mode = MODE_UNIT_DEATH;
            entity.animation = animation_create(entity_get_expected_animation(entity));
        } else {
            entity.mode = MODE_BUILDING_DESTROYED;
            entity.timer = BUILDING_FADE_DURATION;
            entity.queue.clear();

            if (entity.type == ENTITY_LAND_MINE) {
                state.map_mine_cells[entity.cell.x + (entity.cell.y * state.map_width)] = ID_NULL;
            } else {
                // Set building cells to empty, done a special way to avoid overriding the miner cell
                // in the case of a building cancel where the builder is placed on top of the building's old location
                for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size(entity.type); x++) {
                    for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size(entity.type); y++) {
                        if (map_get_cell(state, xy(x, y)) == id) {
                            state.map_cells[x + (y * state.map_width)] = CELL_EMPTY;
                        }
                    }
                }
            }
        }

        match_event_play_sound(state, entity_get_death_sound(entity.type), entity.position.to_xy());

        // Handle garrisoned units for buildings
        // Units handle them after MODE_UNIT_DEATH is over
        if (!entity_is_unit(entity.type)) {
            entity_on_death_release_garrisoned_units(state, entity);
        }
        return;
    } // End if entity_should_die

    bool update_finished = false;
    fixed movement_left = entity_is_unit(entity.type) ? entity_data.unit_data.speed : fixed::from_raw(0);
    while (!update_finished) {
        switch (entity.mode) {
            case MODE_UNIT_IDLE: {
                // Unit is garrisoned
                if (entity.garrison_id != ID_NULL) {
                    const entity_t& carrier = state.entities.get_by_id(entity.garrison_id);
                    if (!(carrier.type == ENTITY_BUNKER || carrier.type == ENTITY_WAR_WAGON)) {
                        update_finished = true;
                        break;
                    } 
                }

                // If unit is idle, check target queue
                if (entity.target.type == TARGET_NONE && !entity.target_queue.empty()) {
                    entity_set_target(entity, entity.target_queue[0]);
                    entity.target_queue.erase(entity.target_queue.begin());
                }

                // If unit is idle, try to find a nearby target
                if (entity.target.type == TARGET_NONE && entity.type != ENTITY_MINER && ENTITY_DATA.at(entity.type).unit_data.damage != 0) {
                    entity.target = entity_target_nearest_enemy(state, entity.garrison_id == ID_NULL ? entity : state.entities.get_by_id(entity.garrison_id));
                }

                if (entity.target.type == TARGET_NONE) {
                    update_finished = true;
                    break;
                }

                if (entity_is_target_invalid(state, entity)) {
                    entity.target = (target_t) { .type = TARGET_NONE };
                    update_finished = true;
                    break;
                }

                if (entity_has_reached_target(state, entity)) {
                    entity.mode = MODE_UNIT_MOVE_FINISHED;
                    break;
                }

                if (entity_check_flag(entity, ENTITY_FLAG_HOLD_POSITION) || entity.garrison_id != ID_NULL) {
                    // Throw away targets if garrisoned. This prevents bunkered units from fixating on a target they can no longer reach
                    if (entity.garrison_id != ID_NULL) {
                        entity.target = (target_t) { .type = TARGET_NONE };
                    }
                    update_finished = true;
                    break;
                }

                map_pathfind(state, entity.cell, entity_get_target_cell(state, entity), entity_cell_size(entity.type), &entity.path, entity_is_mining(state, entity));
                if (!entity.path.empty()) {
                    entity.pathfind_attempts = 0;
                    entity.mode = MODE_UNIT_MOVE;
                    break;
                } else {
                    entity.pathfind_attempts++;
                    if (entity.pathfind_attempts >= 3) {
                        if (entity.target.type == TARGET_BUILD) {
                            state.events.push_back((match_event_t) { .type = MATCH_EVENT_BUILDING_EXIT_BLOCKED, .player_id = entity.player_id });
                        }
                        entity.target = (target_t) { .type = TARGET_NONE };
                        entity.pathfind_attempts = 0;
                        update_finished = true;
                        break;
                    } else {
                        entity.timer = UNIT_MOVE_BLOCKED_DURATION;
                        entity.mode = MODE_UNIT_MOVE_BLOCKED;
                        update_finished = true;
                        break;
                    }
                }

                break;
            }
            case MODE_UNIT_MOVE_BLOCKED: {
                entity.timer--;
                if (entity.timer == 0) {
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_MOVE: {
                bool path_is_blocked = false;

                while (movement_left.raw_value > 0) {
                    // If the unit is not moving between tiles, then pop the next cell off the path
                    if (entity.position == entity_get_target_position(entity) && !entity.path.empty()) {
                        entity.direction = enum_from_xy_direction(entity.path[0] - entity.cell);
                        if (map_is_cell_rect_occupied(state, entity.path[0], entity_cell_size(entity.type), entity.cell, false)) {
                            path_is_blocked = true;
                            // breaks out of while movement left
                            break;
                        }

                        if (map_is_cell_rect_equal_to(state, entity.cell, entity_cell_size(entity.type), id)) {
                            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                        }
                        map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, false, ENTITY_DATA.at(entity.type).has_detection);
                        entity.cell = entity.path[0];
                        map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);
                        map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, true, ENTITY_DATA.at(entity.type).has_detection);
                        entity.path.erase(entity.path.begin());
                    }

                    // Step unit along movement
                    if (entity.position.distance_to(entity_get_target_position(entity)) > movement_left) {
                        entity.position += DIRECTION_XY_FIXED[entity.direction] * movement_left;
                        movement_left = fixed::from_raw(0);
                    } else {
                        movement_left -= entity.position.distance_to(entity_get_target_position(entity));
                        entity.position = entity_get_target_position(entity);
                        // On step finished
                        // Check to see if we triggered a mine
                        for (entity_t& mine : state.entities) {
                            if (mine.type != ENTITY_LAND_MINE || mine.health == 0 || mine.mode != MODE_BUILDING_FINISHED || mine.player_id == entity.player_id ||
                                    std::abs(entity.cell.x - mine.cell.x) > 1 || std::abs(entity.cell.y - mine.cell.y) > 1) {
                                continue;
                            }
                            mine.animation = animation_create(ANIMATION_MINE_PRIME);
                            mine.timer = MINE_PRIME_DURATION;
                            mine.mode = MODE_MINE_PRIME;
                            entity_set_flag(mine, ENTITY_FLAG_INVISIBLE, false);
                        }
                        if (entity.target.type == TARGET_ATTACK_CELL) {
                            target_t attack_target = entity_target_nearest_enemy(state, entity);
                            if (attack_target.type != TARGET_NONE) {
                                entity.target = attack_target;
                                entity.path.clear();
                                entity.mode = entity_has_reached_target(state, entity) ? MODE_UNIT_MOVE_FINISHED : MODE_UNIT_IDLE;
                                // breaks out of while movement left > 0
                                break;
                            }
                        }
                        if (entity_is_target_invalid(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.path.clear();
                            break;
                        }
                        if (entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_MOVE_FINISHED;
                            entity.path.clear();
                            // break out of while movement left
                            break;
                        }
                        if (entity.path.empty()) {
                            entity.mode = MODE_UNIT_IDLE;
                            // break out of while movement left
                            break;
                        }
                    }
                } // End while movement left

                if (path_is_blocked) {
                    entity.mode = MODE_UNIT_MOVE_BLOCKED;
                    entity.timer = UNIT_MOVE_BLOCKED_DURATION;
                }

                update_finished = entity.mode != MODE_UNIT_MOVE_FINISHED;
                break;
            }
            case MODE_UNIT_MOVE_FINISHED: {
                switch (entity.target.type) {
                    case TARGET_NONE:
                    case TARGET_ATTACK_CELL:
                    case TARGET_CELL: {
                        entity.target = (target_t) {
                            .type = TARGET_NONE
                        };
                        entity.mode = MODE_UNIT_IDLE;
                        break;
                    }
                    case TARGET_UNLOAD: {
                        if (!entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        
                        entity_unload_unit(state, entity, ENTITY_UNLOAD_ALL);
                        entity.mode = MODE_UNIT_IDLE;
                        entity.target = (target_t) { .type = TARGET_NONE };
                        break;
                    }
                    case TARGET_SMOKE: {
                        if (!entity_has_reached_target(state, entity)) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        entity.direction = enum_direction_to_rect(entity.cell, entity.target.cell, 1);
                        entity.mode = MODE_UNIT_TINKER_THROW;
                        entity.animation = animation_create(ANIMATION_UNIT_ATTACK);
                        match_event_play_sound(state, SOUND_THROW, entity.position.to_xy());
                        break;
                    }
                    case TARGET_BUILD: {
                        bool can_build = true;
                        for (int x = entity.target.build.building_cell.x; x < entity.target.build.building_cell.x + entity_cell_size(entity.target.build.building_type); x++) {
                            for (int y = entity.target.build.building_cell.y; y < entity.target.build.building_cell.y + entity_cell_size(entity.target.build.building_type); y++) {
                                if ((xy(x, y) != entity.cell && state.map_cells[x + (y * state.map_width)] != CELL_EMPTY) || 
                                        state.map_mine_cells[x + (y * state.map_width)] != ID_NULL) {
                                    can_build = false;
                                }
                            }
                        }
                        if (!can_build) {
                            state.events.push_back((match_event_t) { .type = MATCH_EVENT_CANT_BUILD, .player_id = entity.player_id });
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        if (state.player_gold[entity.player_id] < ENTITY_DATA.at(entity.target.build.building_type).gold_cost) {
                            state.events.push_back((match_event_t) { .type = MATCH_EVENT_NOT_ENOUGH_GOLD, .player_id = entity.player_id });
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        state.player_gold[entity.player_id] -= ENTITY_DATA.at(entity.target.build.building_type).gold_cost;

                        if (entity.target.build.building_type == ENTITY_LAND_MINE) {
                            entity_create(state, entity.target.build.building_type, entity.player_id, entity.target.build.building_cell);
                            match_event_play_sound(state, SOUND_MINE_INSERT, cell_center(entity.target.build.building_cell).to_xy());

                            entity.direction = enum_direction_to_rect(entity.cell, entity.target.build.building_cell, entity_cell_size(entity.target.build.building_type));
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_TINKER_THROW;
                            entity.animation = animation_create(ANIMATION_UNIT_ATTACK);
                            break;
                        }

                        map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                        map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, false, ENTITY_DATA.at(entity.type).has_detection);
                        entity.target.id = entity_create(state, entity.target.build.building_type, entity.player_id, entity.target.build.building_cell);
                        entity.mode = MODE_UNIT_BUILD;
                        entity.timer = UNIT_BUILD_TICK_DURATION;

                        state.events.push_back((match_event_t) {
                            .type = MATCH_EVENT_SELECTION_HANDOFF,
                            .selection_handoff = (match_event_selection_handoff_t) {
                                .player_id = entity.player_id,
                                .to_deselect = id,
                                .to_select = entity.target.id
                            }
                        });

                        break;
                    }
                    case TARGET_BUILD_ASSIST: {
                        if (entity_is_target_invalid(state, entity)) {
                            entity.target = (target_t) {
                                .type = TARGET_NONE
                            };
                            entity.mode = MODE_UNIT_IDLE;
                        }

                        entity_t& builder = state.entities.get_by_id(entity.target.id);
                        if (builder.mode == MODE_UNIT_BUILD) {
                            entity.target = (target_t) {
                                .type = TARGET_REPAIR,
                                .id = builder.target.id,
                                .repair = (target_repair_t) {
                                    .health_repaired = 0
                                }
                            };
                            entity.mode = MODE_UNIT_REPAIR;
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            entity.direction = enum_direction_to_rect(entity.cell, builder.target.build.building_cell, entity_cell_size(builder.target.build.building_type));
                        }
                        break;
                    }
                    case TARGET_REPAIR:
                    case TARGET_ENTITY: 
                    case TARGET_ATTACK_ENTITY: {
                        if (entity_is_target_invalid(state, entity)) {
                            entity.target = entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        if (!entity_has_reached_target(state, entity)) {
                            // This will trigger a repath
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        entity_t& target = state.entities.get_by_id(entity.target.id);

                        // Sapper explosion
                        if (entity.target.type == TARGET_ATTACK_ENTITY && entity.type == ENTITY_SAPPER) {
                            entity_explode(state, id);
                            update_finished = true;
                            break;
                        }

                        // Begin attack
                        if (entity.target.type == TARGET_ATTACK_ENTITY && entity_data.unit_data.damage != 0) {
                            if (entity.cooldown_timer != 0) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            // Check min range
                            SDL_Rect entity_rect = (SDL_Rect) { .x = entity.cell.x, .y = entity.cell.y, .w = entity_cell_size(entity.type), .h = entity_cell_size(entity.type) };
                            SDL_Rect target_rect = (SDL_Rect) { .x = target.cell.x, .y = target.cell.y, .w = entity_cell_size(target.type), .h = entity_cell_size(target.type) };
                            bool attack_with_bayonets = false;
                            if (euclidean_distance_squared_between(entity_rect, target_rect) < ENTITY_DATA.at(entity.type).unit_data.min_range_squared) {
                                if (entity.type == ENTITY_SOLDIER && match_player_has_upgrade(state, entity.player_id, UPGRADE_BAYONETS)) {
                                    attack_with_bayonets = true;
                                } else {
                                    entity.direction = enum_direction_to_rect(entity.cell, target.cell, entity_cell_size(target.type));
                                    entity.mode = MODE_UNIT_IDLE;
                                    update_finished = true;
                                    break;
                                }
                            }

                            // Attack inside bunker
                            if (entity.garrison_id != ID_NULL) {
                                entity_t& carrier = state.entities.get_by_id(entity.garrison_id);
                                // Don't attack during bunker cooldown or if this is a melee unit
                                if (carrier.cooldown_timer != 0 || entity_data.unit_data.range_squared == 1) {
                                    update_finished = true;
                                    break;
                                }

                                carrier.cooldown_timer = ENTITY_BUNKER_FIRE_OFFSET;
                            }

                            // Begin attack windup
                            entity.direction = enum_direction_to_rect(entity.cell, target.cell, entity_cell_size(target.type));
                            entity.mode = entity.type == ENTITY_SOLDIER && !attack_with_bayonets ? MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP : MODE_UNIT_ATTACK_WINDUP;
                            update_finished = true;
                            break;
                        }

                        // Return gold
                        if (entity.type == ENTITY_MINER && (target.type == ENTITY_HALL || target.type == ENTITY_CAMP) && target.mode == MODE_BUILDING_FINISHED && entity.player_id == target.player_id && entity.gold_held != 0 && entity.target.type != TARGET_REPAIR) {
                            state.player_gold[entity.player_id] += entity.gold_held;
                            entity.gold_held = 0;
                            entity.target = entity_target_nearest_gold_mine(state, entity);

                            if (entity.target.type == TARGET_NONE) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }
                            entity_t& mine = state.entities.get_by_id(entity.target.id);
                            xy rally_cell = map_get_nearest_cell_around_rect(state, mine.cell + xy(1, 1), 1, target.cell, entity_cell_size(target.type), true);
                            xy mine_exit_cell = entity_get_exit_cell(state, mine.cell, entity_cell_size(mine.type), entity_cell_size(entity.type), rally_cell, true);
                            if (mine_exit_cell.x == -1) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            std::vector<xy> mine_exit_path;
                            map_pathfind(state, mine_exit_cell, rally_cell, 1, &mine_exit_path, true);
                            mine_exit_path.push_back(mine_exit_cell);

                            map_pathfind(state, entity.cell, entity_get_target_cell(state, entity), 1, &entity.path, true, &mine_exit_path);
                            if (entity.path.empty()) {
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            entity.mode = MODE_UNIT_MOVE;
                            update_finished = true;
                            break;
                        }

                        // Enter mine
                        if (entity.type == ENTITY_MINER && target.type == ENTITY_GOLD_MINE && target.gold_held > 0) {
                            if (entity.gold_held != 0) {
                                entity.target = entity_target_nearest_camp(state, entity);
                                entity.mode = MODE_UNIT_IDLE;
                                update_finished = true;
                                break;
                            }

                            if (entity_get_garrisoned_occupancy(state, target) + entity_data.garrison_size <= ENTITY_DATA.at(target.type).garrison_capacity) {
                                target.garrisoned_units.push_back(id);
                                entity.garrison_id = entity.target.id;
                                entity.mode = MODE_UNIT_IN_MINE;
                                entity.timer = UNIT_IN_MINE_DURATION;
                                entity.target = (target_t) { .type = TARGET_NONE };
                                entity.gold_held = std::min(UNIT_MAX_GOLD_HELD, target.gold_held);
                                target.gold_held -= entity.gold_held;
                                map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                                map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, false, ENTITY_DATA.at(entity.type).has_detection);
                            }

                            // If no space available in the mine, we still want to end update here so that unit keeps their target and waits
                            update_finished = true;
                            break;
                        }

                        // Garrison
                        if (entity.player_id == target.player_id && entity_data.garrison_size != ENTITY_CANNOT_GARRISON && entity_get_garrisoned_occupancy(state, target) + entity_data.garrison_size <= ENTITY_DATA.at(target.type).garrison_capacity && entity.target.type != TARGET_REPAIR && (entity_is_unit(target.type) || target.mode == MODE_BUILDING_FINISHED)) {
                            target.garrisoned_units.push_back(id);
                            entity.garrison_id = entity.target.id;
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (target_t) { .type = TARGET_NONE };
                            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                            map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, false, ENTITY_DATA.at(entity.type).has_detection);
                            match_event_play_sound(state, SOUND_GARRISON_IN, target.position.to_xy());
                            update_finished = true;
                            break;
                        }

                        // Begin repair
                        if (entity.player_id == target.player_id && entity.type == ENTITY_MINER && entity_is_building(target.type) && target.health < ENTITY_DATA.at(target.type).max_health) {
                            if (entity.target.type != TARGET_REPAIR) {
                                entity.target = (target_t) {
                                    .type = TARGET_REPAIR,
                                    .id = entity.target.id,
                                    .repair = (target_repair_t) {
                                        .health_repaired = 0
                                    }
                                };
                            }
                            entity.mode = MODE_UNIT_REPAIR;
                            entity.direction = enum_direction_to_rect(entity.cell, target.cell, entity_cell_size(target.type));
                            entity.timer = UNIT_BUILD_TICK_DURATION;
                            break;
                        }

                        // Don't attack allied units
                        if (entity.player_id == target.player_id || entity_data.unit_data.damage == 0) {
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (target_t) {
                                .type = TARGET_NONE
                            };
                            update_finished = true;
                            break;
                        }

                        break;
                    }
                }
                update_finished = update_finished || !(entity.mode == MODE_UNIT_MOVE && movement_left.raw_value > 0);
                break;
            } // End mode move finished
            case MODE_UNIT_BUILD: {
                // This code handles the case where the building is destroyed while the unit is building it
                uint32_t building_index = state.entities.get_index_of(entity.target.id);
                if (building_index == INDEX_INVALID || !entity_is_selectable(state.entities[building_index]) || state.entities[building_index].mode != MODE_BUILDING_IN_PROGRESS) {
                    entity_stop_building(state, id);
                    update_finished = true;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    // Building tick
                    entity_t& building = state.entities[building_index];

                    int building_hframe = entity_get_animation_frame(building).x;
                    #ifdef GOLD_DEBUG_FAST_BUILD
                        building.health = std::min(building.health + 20, ENTITY_DATA.at(building.type).max_health);
                    #else
                        building.health++;
                    #endif
                    if (building.health == ENTITY_DATA.at(building.type).max_health) {
                        entity_building_finish(state, entity.target.id);
                    } else {
                        entity.timer = UNIT_BUILD_TICK_DURATION;
                    }

                    if (entity_get_animation_frame(building).x != building_hframe) {
                        // state.is_fog_dirty = true;
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_REPAIR: {
                // Stop repairing if the building is destroyed
                if (entity_is_target_invalid(state, entity)) {
                    entity.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }

                entity_t& target = state.entities.get_by_id(entity.target.id);
                if (target.health == ENTITY_DATA.at(target.type).max_health || (target.mode == MODE_BUILDING_FINISHED && state.player_gold[entity.player_id] == 0)) {
                    entity.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    int building_hframe = entity_get_animation_frame(target).x;
                    target.health++;
                    if (target.mode == MODE_BUILDING_FINISHED) {
                        entity.target.repair.health_repaired++;
                        if (entity.target.repair.health_repaired == UNIT_REPAIR_RATE) {
                            state.player_gold[entity.player_id]--;
                            entity.target.repair.health_repaired = 0;
                        }
                    }
                    if (target.health == ENTITY_DATA.at(target.type).max_health) {
                        if (target.mode == MODE_BUILDING_IN_PROGRESS) {
                            entity_building_finish(state, entity.target.id);
                        } else {
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                        }
                    } else {
                        entity.timer = UNIT_BUILD_TICK_DURATION;
                    }

                    if (entity_get_animation_frame(target).x != building_hframe) {
                        // state.is_fog_dirty = true;
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_ATTACK_WINDUP: 
            case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP: {
                if (entity_is_target_invalid(state, entity)) {
                    // TOOD target = nearest insight enemy
                    entity.target = (target_t) {
                        .type = TARGET_NONE
                    };
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                if (!animation_is_playing(entity.animation)) {
                    entity_attack_target(state, id, state.entities.get_by_id(entity.target.id));
                    entity.cooldown_timer = ENTITY_DATA.at(entity.type).unit_data.attack_cooldown;
                    match_event_play_sound(state, entity_get_attack_sound(entity), entity.position.to_xy());
                    entity.mode = MODE_UNIT_IDLE;

                    // If garrisoned, reasses targets. This is so that units don't get stuck shooting a building when a unit may have become a bigger priority
                    if (entity.garrison_id != ID_NULL) {
                        entity.target = entity_target_nearest_enemy(state, entity);
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_IN_MINE: {
                if (entity.timer != 0) {
                    entity.timer--;
                }
                if (entity.timer == 0) {
                    entity_t& exit_building = state.entities.get_by_id(entity.garrison_id);
                    target_t target = entity.mode == MODE_UNIT_IN_MINE ? entity_target_nearest_camp(state, entity) : entity_target_nearest_gold_mine(state, entity);
                    uint32_t target_index = target.type == TARGET_NONE ? INDEX_INVALID : state.entities.get_index_of(target.id);
                    xy rally_cell = target.type == TARGET_NONE 
                                        ? (exit_building.cell + xy(1, entity_cell_size(ENTITY_GOLD_MINE))) 
                                        : map_get_nearest_cell_around_rect(state, exit_building.cell + xy(1, 1), 1, state.entities[target_index].cell, entity_cell_size(state.entities[target_index].type), true);
                    xy exit_cell = entity_get_exit_cell(state, exit_building.cell, entity_cell_size(exit_building.type), entity_cell_size(ENTITY_MINER), rally_cell, true);

                    if (exit_cell.x == -1) {
                        state.events.push_back((match_event_t) { .type = entity.mode == MODE_UNIT_IN_MINE ? MATCH_EVENT_MINE_EXIT_BLOCKED : MATCH_EVENT_BUILDING_EXIT_BLOCKED });
                    } else {
                        entity_remove_garrisoned_unit(exit_building, id);
                        entity.garrison_id = ID_NULL;
                        entity.target = target;
                        entity.mode = MODE_UNIT_OUT_MINE;
                        entity.direction = enum_direction_to_rect(rally_cell, entity.cell, 1);
                        entity.timer = UNIT_OUT_MINE_DURATION;
                        entity.cell = exit_cell;
                        entity.position = cell_center(entity.cell);
                        if (map_get_cell(state, entity.cell) == CELL_EMPTY) {
                            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);
                        }
                        map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, true, ENTITY_DATA.at(entity.type).has_detection);

                        if (exit_building.type == ENTITY_GOLD_MINE && exit_building.garrisoned_units.empty() && exit_building.gold_held == 0) {
                            entity.mode = MODE_GOLD_MINE_COLLAPSED;
                            // TODO send alert to player of collapse
                        }
                    }
                }
                update_finished = true;
                break;
            }
            case MODE_UNIT_OUT_MINE: {
                entity.timer--;
                if (entity.timer == 0) {
                    entity.target = entity_target_nearest_camp(state, entity);
                    entity.mode = MODE_UNIT_IDLE;
                    update_finished = true;
                    break;
                }
                update_finished = true;
                break;
            }
            case MODE_UNIT_TINKER_THROW: {
                if (!animation_is_playing(entity.animation)) {
                    if (entity.target.type == TARGET_SMOKE) {
                        state.projectiles.push_back((projectile_t) {
                            .type = PROJECTILE_SMOKE,
                            .position = entity.position + xy_fixed(DIRECTION_XY[entity.direction] * 10),
                            .target = cell_center(entity.target.cell)
                        });
                        entity.cooldown_timer = SMOKE_BOMB_COOLDOWN;
                    }
                    entity.target = (target_t) { .type = TARGET_NONE };
                    entity.mode = MODE_UNIT_IDLE;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_DEATH: {
                if (!animation_is_playing(entity.animation)) {
                    map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                    entity_on_death_release_garrisoned_units(state, entity);
                    entity.mode = MODE_UNIT_DEATH_FADE;
                }
                update_finished = true;
                break;
            }
            case MODE_BUILDING_FINISHED: {
                if (!entity.queue.empty() && entity.timer != 0) {
                    if (entity.timer == BUILDING_QUEUE_BLOCKED && !entity_building_is_supply_blocked(state, entity)) {
                        entity.timer = building_queue_item_duration(entity.queue[0]);
                    } else if (entity.timer != BUILDING_QUEUE_BLOCKED && entity_building_is_supply_blocked(state, entity)) {
                        entity.timer = BUILDING_QUEUE_BLOCKED;
                    }

                    if (entity.timer != BUILDING_QUEUE_BLOCKED && entity.timer != BUILDING_QUEUE_EXIT_BLOCKED) {
                        #ifdef GOLD_DEBUG_FAST_TRAIN
                            entity.timer = std::max((int)entity.timer - 10, 0);
                        #else
                            entity.timer--;
                        #endif
                    }

                    if ((entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UNIT) || entity.timer == BUILDING_QUEUE_EXIT_BLOCKED) {
                        xy rally_cell = entity.rally_point.x == -1 
                                            ? entity.cell + xy(0, entity_cell_size(entity.type))
                                            : entity.rally_point / TILE_SIZE;
                        xy exit_cell = entity_get_exit_cell(state, entity.cell, entity_cell_size(entity.type), entity_cell_size(entity.queue[0].unit_type), rally_cell);
                        if (exit_cell.x == -1) {
                            if (entity.timer == 0) {
                                state.events.push_back((match_event_t) { .type = MATCH_EVENT_BUILDING_EXIT_BLOCKED, .player_id = entity.player_id });
                            }
                            entity.timer = BUILDING_QUEUE_EXIT_BLOCKED;
                            update_finished = true;
                            break;
                        } 

                        entity.timer = 0;
                        entity_id unit_id = entity_create(state, entity.queue[0].unit_type, entity.player_id, exit_cell);

                        // Rally unit
                        entity_t& unit = state.entities.get_by_id(unit_id);
                        if (unit.type == ENTITY_MINER && map_get_cell(state, rally_cell) < CELL_EMPTY && state.entities.get_by_id(map_get_cell(state, rally_cell)).type == ENTITY_GOLD_MINE) {
                            // Rally to gold
                            unit.target = (target_t) {
                                .type = TARGET_ENTITY,
                                .id = map_get_cell(state, rally_cell)
                            };
                        } else {
                            // Rally to cell
                            unit.target = (target_t) {
                                .type = TARGET_CELL,
                                .cell = rally_cell
                            };
                        }

                        // Create alert
                        match_event_alert(state, MATCH_ALERT_TYPE_UNIT, unit.player_id, unit.cell, entity_cell_size(unit.type));

                        entity_building_dequeue(state, entity);
                    } else if (entity.timer == 0 && entity.queue[0].type == BUILDING_QUEUE_ITEM_UPGRADE) {
                        match_grant_player_upgrade(state, entity.player_id, entity.queue[0].upgrade);

                        // Set all existing wagons to war wagons
                        if (entity.queue[0].upgrade == UPGRADE_WAR_WAGON) {
                            for (entity_t& other_entity : state.entities) {
                                if (other_entity.player_id != entity.player_id) {
                                    continue;
                                }
                                if (other_entity.type == ENTITY_WAGON) {
                                    other_entity.type = ENTITY_WAR_WAGON;
                                } else if (entity_is_building(other_entity.type)) {
                                    for (building_queue_item_t& other_item : other_entity.queue) {
                                        if (other_item.type == BUILDING_QUEUE_ITEM_UNIT && other_item.unit_type == ENTITY_WAGON) {
                                            other_item.unit_type = ENTITY_WAR_WAGON;
                                        }
                                    }
                                }
                            }
                        }

                        // Show status
                        state.events.push_back((match_event_t) {
                            .type = MATCH_EVENT_RESEARCH_COMPLETE,
                            .research_complete = (match_event_research_complete_t) {
                                .upgrade = entity.queue[0].upgrade,
                                .player_id = entity.player_id
                            }
                        });

                        // Create alert
                        match_event_alert(state, MATCH_ALERT_TYPE_RESEARCH, entity.player_id, entity.cell, entity_cell_size(entity.type));

                        entity_building_dequeue(state, entity);
                    }
                } 

                update_finished = true;
                break;
            }
            case MODE_BUILDING_DESTROYED: {
                if (entity.timer != 0) {
                    entity.timer--;
                }
                update_finished = true;
                break;
            }
            case MODE_MINE_ARM: {
                entity.timer--;
                if (entity.timer == 0) {
                    entity.mode = MODE_BUILDING_FINISHED;
                    entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, true);
                }
                update_finished = true;
                break;
            }
            case MODE_MINE_PRIME: {
                entity.timer--;
                if (entity.timer == 0) {
                    entity_explode(state, id);
                }
                update_finished = true;
            }
            default:
                update_finished = true;
                break;
        }
    } // End while !update_finished

    if (entity.cooldown_timer != 0) {
        entity.cooldown_timer--;
    }

    if (entity.taking_damage_counter != 0) {
        entity.taking_damage_timer--;
        if (entity.taking_damage_timer == 0) {
            entity.taking_damage_counter--;
            entity_set_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER, entity.taking_damage_counter == 0 ? false : !entity_check_flag(entity, ENTITY_FLAG_DAMAGE_FLICKER));
            entity.taking_damage_timer = entity.taking_damage_counter == 0 ? 0 : MATCH_TAKING_DAMAGE_FLICKER_DURATION;
        } 
    }
    if (entity.health == ENTITY_DATA.at(entity.type).max_health) {
        entity.health_regen_timer = 0;
    }
    if (entity.health_regen_timer != 0) {
        entity.health_regen_timer--;
        if (entity.health_regen_timer == 0) {
            entity.health++;
            if (entity.health != ENTITY_DATA.at(entity.type).max_health) {
                entity.health_regen_timer = UNIT_HEALTH_REGEN_DURATION;
            }
        }
    }

    // Update animation
    if (entity_is_unit(entity.type)) {
        AnimationName expected_animation = entity_get_expected_animation(entity);
        if (entity.animation.name != expected_animation || !animation_is_playing(entity.animation)) {
            entity.animation = animation_create(expected_animation);
        }
        int prev_hframe = entity.animation.frame.x;
        animation_update(entity.animation);
        if (prev_hframe != entity.animation.frame.x) {
            if ((entity.mode == MODE_UNIT_REPAIR || entity.mode == MODE_UNIT_BUILD) && prev_hframe == 0) {
                match_event_play_sound(state, SOUND_HAMMER, entity.position.to_xy());
            } 
        }
    } else if (entity.type == ENTITY_SMITH) {
        if (entity.queue.empty() && entity.animation.name != ANIMATION_UNIT_IDLE && entity.animation.name != ANIMATION_SMITH_END) {
            entity.animation = animation_create(ANIMATION_SMITH_END);
        } else if (entity.animation.name == ANIMATION_SMITH_END && !animation_is_playing(entity.animation)) {
            entity.animation = animation_create(ANIMATION_UNIT_IDLE);
        } else if (!entity.queue.empty() && entity.animation.name == ANIMATION_UNIT_IDLE) {
            entity.animation = animation_create(ANIMATION_SMITH_BEGIN);
        } else if (entity.animation.name == ANIMATION_SMITH_BEGIN && !animation_is_playing(entity.animation)) {
            entity.animation = animation_create(ANIMATION_SMITH_LOOP);
        }
        animation_update(entity.animation);
    } else if (entity.type == ENTITY_LAND_MINE && entity.mode == MODE_MINE_PRIME) {
        int prev_hframe = entity.animation.frame.x;
        animation_update(entity.animation);
        if (prev_hframe != entity.animation.frame.x) {
            match_event_play_sound(state, SOUND_MINE_PRIME, entity.position.to_xy());
        }
    }
}

bool entity_is_unit(EntityType type) {
    return type < ENTITY_HALL;
}

bool entity_is_building(EntityType type) {
    return type < ENTITY_GOLD_MINE;
}

int entity_cell_size(EntityType type) {
    return ENTITY_DATA.at(type).cell_size;
}

SDL_Rect entity_get_rect(const entity_t& entity) {
    SDL_Rect rect = (SDL_Rect) {
        .x = entity.position.x.integer_part(),
        .y = entity.position.y.integer_part(),
        .w = entity_cell_size(entity.type) * TILE_SIZE,
        .h = entity_cell_size(entity.type) * TILE_SIZE
    };
    if (entity_is_unit(entity.type)) {
        rect.x -= rect.w / 2;
        rect.y -= rect.h / 2;
    }
    return rect;
}

xy entity_get_center_position(const entity_t& entity) {
    if (entity_is_unit(entity.type)) {
        return entity.position.to_xy();
    }
    SDL_Rect rect = entity_get_rect(entity);

    return xy(rect.x + (rect.w / 2), rect.y + (rect.h / 2));
}

Sprite entity_get_sprite(const entity_t& entity) {
    if (entity.mode == MODE_BUILDING_DESTROYED) {
        if (entity.type == ENTITY_BUNKER) {
            return SPRITE_BUILDING_DESTROYED_BUNKER;
        }
        if (entity.type == ENTITY_LAND_MINE) {
            return SPRITE_BUILDING_DESTROYED_MINE;
        }
        switch (entity_cell_size(entity.type)) {
            case 2:
                return SPRITE_BUILDING_DESTROYED_2;
            case 3:
                return SPRITE_BUILDING_DESTROYED_3;
            case 4:
                return SPRITE_BUILDING_DESTROYED_4;
            default:
                GOLD_ASSERT_MESSAGE(false, "Destroyed sprite needed for building of this size");
                return SPRITE_BUILDING_DESTROYED_2;
        }
    }
    if (entity.mode == MODE_UNIT_BUILD || entity.mode == MODE_UNIT_REPAIR) {
        return SPRITE_MINER_BUILDING;
    }
    return ENTITY_DATA.at(entity.type).sprite;
}

Sprite entity_get_select_ring(const entity_t& entity, bool is_ally) {
    if (entity.type == ENTITY_LAND_MINE) {
        return is_ally ? SPRITE_SELECT_RING_MINE : SPRITE_SELECT_RING_MINE_ENEMY;
    }
    if (entity.type == ENTITY_GOLD_MINE) {
        return SPRITE_SELECT_RING_GOLD_MINE;
    }

    Sprite select_ring; 
    if (entity_is_unit(entity.type)) {
        select_ring = (Sprite)(SPRITE_SELECT_RING_UNIT_1 + ((entity_cell_size(entity.type) - 1) * 2));
    } else {
        select_ring = (Sprite)(SPRITE_SELECT_RING_BUILDING_2 + ((entity_cell_size(entity.type) - 2) * 2));
    }
    if (!is_ally) {
        select_ring = (Sprite)(select_ring + 1);
    }
    return select_ring;
}

uint16_t entity_get_elevation(const match_state_t& state, const entity_t& entity) {
    // If unit is garrisoned, elevation is based on bunker elevation
    if (entity.garrison_id != ID_NULL) {
        return entity_get_elevation(state, state.entities.get_by_id(entity.garrison_id));
    }


    uint16_t elevation = state.map_tiles[entity.cell.x + (entity.cell.y * state.map_width)].elevation;

    if (entity.type == ENTITY_JOCKEY || entity.type == ENTITY_SOLDIER || entity.type == ENTITY_COWBOY || entity.type == ENTITY_SAPPER || entity.type == ENTITY_SPY) {
        for (int y = 0; y < 2; y++) {
            xy above_cell = entity.cell - xy(0, y + 1);
            if (map_is_cell_in_bounds(state, above_cell) && map_is_tile_ramp(state, above_cell)) {
                elevation = std::max(elevation, state.map_tiles[above_cell.x + (above_cell.y * state.map_width)].elevation);
            }
        }
    }

    for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size(entity.type); x++) {
        for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size(entity.type); y++) {
            elevation = std::max(elevation, state.map_tiles[x + (y * state.map_width)].elevation);
        }
    }

    if (entity.mode == MODE_UNIT_MOVE) {
        xy unit_prev_cell = entity.cell - DIRECTION_XY[entity.direction];
        for (int x = unit_prev_cell.x; x < unit_prev_cell.x + entity_cell_size(entity.type); x++) {
            for (int y = unit_prev_cell.y; y < unit_prev_cell.y + entity_cell_size(entity.type); y++) {
                elevation = std::max(elevation, state.map_tiles[x + (y * state.map_width)].elevation);
            }
        }
    }

    return elevation;
}

bool entity_is_selectable(const entity_t& entity) {
    if (entity.type == ENTITY_GOLD_MINE) {
        return true;
    }

    return !(
        entity.health == 0 || 
        entity.mode == MODE_UNIT_BUILD || 
        entity.garrison_id != ID_NULL 
    );
}

bool entity_check_flag(const entity_t& entity, uint32_t flag) {
    return (entity.flags & flag) == flag;
}

void entity_set_flag(entity_t& entity, uint32_t flag, bool value) {
    if (value) {
        entity.flags |= flag;
    } else {
        entity.flags &= ~flag;
    }
}

bool entity_is_target_invalid(const match_state_t& state, const entity_t& entity) {
    if (!(entity.target.type == TARGET_ENTITY || entity.target.type == TARGET_ATTACK_ENTITY || entity.target.type == TARGET_REPAIR || entity.target.type == TARGET_BUILD_ASSIST)) {
        return false;
    }

    uint32_t target_index = state.entities.get_index_of(entity.target.id);
    if (target_index == INDEX_INVALID) {
        return true;
    }

    if (state.entities[target_index].type == ENTITY_GOLD_MINE && state.entities[target_index].gold_held == 0) {
        return true;
    }
    
    if (entity.target.type == TARGET_BUILD_ASSIST) {
        return state.entities[target_index].health == 0 || state.entities[target_index].target.type != TARGET_BUILD;
    }

    return !entity_is_selectable(state.entities[target_index]);
}

bool entity_has_reached_target(const match_state_t& state, const entity_t& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return true;
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
            return entity.cell == entity.target.cell;
        case TARGET_BUILD: {
            if (entity.target.build.building_type == ENTITY_LAND_MINE) {
                return xy::manhattan_distance(entity.cell, entity.target.build.building_cell) == 1;
            }
            return entity.cell == entity.target.build.unit_cell;
        }
        case TARGET_BUILD_ASSIST: {
            const entity_t& builder = state.entities.get_by_id(entity.target.id);
            SDL_Rect building_rect = (SDL_Rect) {
                .x = builder.target.build.building_cell.x, .y = builder.target.build.building_cell.y,
                .w = entity_cell_size(builder.target.build.building_type), .h = entity_cell_size(builder.target.build.building_type)
            };
            SDL_Rect unit_rect = (SDL_Rect) {
                .x = entity.cell.x, .y = entity.cell.y,
                .w = entity_cell_size(entity.type), .h = entity_cell_size(entity.type)
            };
            return sdl_rects_are_adjacent(building_rect, unit_rect);
        }
        case TARGET_UNLOAD:
            return entity.path.empty() && xy::manhattan_distance(entity.cell, entity.target.cell) < 3;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            const entity_t& target = state.entities.get_by_id(entity.target.id);
            const entity_t& reference_entity = entity.garrison_id == ID_NULL ? entity : state.entities.get_by_id(entity.garrison_id);
            SDL_Rect entity_rect = (SDL_Rect) {
                .x = reference_entity.cell.x, .y = reference_entity.cell.y,
                .w = entity_cell_size(reference_entity.type), .h = entity_cell_size(reference_entity.type)
            };
            SDL_Rect target_rect = (SDL_Rect) {
                .x = target.cell.x, .y = target.cell.y,
                .w = entity_cell_size(target.type), .h = entity_cell_size(target.type)
            };

            int entity_range_squared = ENTITY_DATA.at(entity.type).unit_data.range_squared;
            return entity.target.type != TARGET_ATTACK_ENTITY || entity_range_squared == 1
                        ? sdl_rects_are_adjacent(entity_rect, target_rect)
                        : euclidean_distance_squared_between(entity_rect, target_rect) <= entity_range_squared;
        }
        case TARGET_SMOKE: {
            return xy::euclidean_distance_squared(entity.cell, entity.target.cell) <= SMOKE_BOMB_THROW_RANGE_SQUARED;
        }
    }
}

xy entity_get_target_cell(const match_state_t& state, const entity_t& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return entity.cell;
        case TARGET_BUILD: {
            if (entity.target.build.building_type == ENTITY_LAND_MINE) {
                return map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), entity.target.build.building_cell, entity_cell_size(ENTITY_LAND_MINE), false);
            }
            return entity.target.build.unit_cell;
        }
        case TARGET_BUILD_ASSIST: {
            const entity_t& builder = state.entities.get_by_id(entity.target.id);
            return map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), builder.target.build.building_cell, entity_cell_size(builder.target.build.building_type), false);
        }
        case TARGET_CELL:
        case TARGET_ATTACK_CELL:
        case TARGET_UNLOAD:
        case TARGET_SMOKE:
            return entity.target.cell;
        case TARGET_ENTITY:
        case TARGET_ATTACK_ENTITY:
        case TARGET_REPAIR: {
            const entity_t& target = state.entities.get_by_id(entity.target.id);
            return map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), target.cell, entity_cell_size(target.type), entity_is_mining(state, entity));
        }
    }
}

xy_fixed entity_get_target_position(const entity_t& entity) {
    int unit_size = ENTITY_DATA.at(entity.type).cell_size * TILE_SIZE;
    return xy_fixed((entity.cell * TILE_SIZE) + xy(unit_size / 2, unit_size / 2));
}

AnimationName entity_get_expected_animation(const entity_t& entity) {
    if (entity.type == ENTITY_CANNON) {
        switch (entity.mode) {
            case MODE_UNIT_MOVE:
                return ANIMATION_UNIT_MOVE_CANNON;
            case MODE_UNIT_ATTACK_WINDUP:
                return ANIMATION_CANNON_ATTACK;
            case MODE_UNIT_DEATH:
                return ANIMATION_CANNON_DEATH;
            case MODE_UNIT_DEATH_FADE:
                return ANIMATION_CANNON_DEATH_FADE;
            default:
                return ANIMATION_UNIT_IDLE;
        }
    }
    
    switch (entity.mode) {
        case MODE_UNIT_MOVE:
            return ANIMATION_UNIT_MOVE;
        case MODE_UNIT_BUILD:
        case MODE_UNIT_REPAIR:
            return ANIMATION_UNIT_BUILD;
        case MODE_UNIT_ATTACK_WINDUP:
        case MODE_UNIT_TINKER_THROW:
            return ANIMATION_UNIT_ATTACK;
        case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP:
            return ANIMATION_SOLDIER_RANGED_ATTACK;
        case MODE_UNIT_DEATH:
            return ANIMATION_UNIT_DEATH;
        case MODE_UNIT_DEATH_FADE:
            return ANIMATION_UNIT_DEATH_FADE;
        default:
            return ANIMATION_UNIT_IDLE;
    }
}

xy entity_get_animation_frame(const entity_t& entity) {
    if (entity_is_unit(entity.type)) {
        xy frame = entity.animation.frame;

        if (entity.mode == MODE_UNIT_BUILD) {
            frame.y = 2;
        } else if (entity.animation.name == ANIMATION_UNIT_DEATH || entity.animation.name == ANIMATION_UNIT_DEATH_FADE || entity.animation.name == ANIMATION_CANNON_DEATH || entity.animation.name == ANIMATION_CANNON_DEATH_FADE) {
            frame.y = entity.direction == DIRECTION_NORTH ? 1 : 0;
        } else if (entity.direction == DIRECTION_NORTH) {
            frame.y = 1;
        } else if (entity.direction == DIRECTION_SOUTH) {
            frame.y = 0;
        } else {
            frame.y = 2;
        }

        if (entity.gold_held && (entity.animation.name == ANIMATION_UNIT_MOVE || entity.animation.name == ANIMATION_UNIT_IDLE)) {
            frame.y += 3;
        }
        if (entity.type == ENTITY_SAPPER && entity.target.type == TARGET_ATTACK_ENTITY) {
            frame.y += 3;
        }

        return frame;
    } else if (entity_is_building(entity.type)) {
        if (entity.mode == MODE_BUILDING_DESTROYED || entity.mode == MODE_MINE_ARM) {
            return xy(0, 0);
        }
        if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
            return xy((3 * entity.health) / ENTITY_DATA.at(entity.type).max_health, 0);
        } 
        if (entity.mode == MODE_MINE_PRIME) {
            return entity.animation.frame;
        }
        // Building finished frame
        return xy(3, 0);
    } else {
        // Gold
        if (entity.mode == MODE_GOLD_MINE_COLLAPSED) {
            return xy(2, 0);
        }
        if (!entity.garrisoned_units.empty()) {
            return xy(1, 0);
        }
        return xy(0, 0);
    }
}

bool entity_should_flip_h(const entity_t& entity) {
    return entity_is_unit(entity.type) && entity.direction > DIRECTION_SOUTH;
}

Sound entity_get_attack_sound(const entity_t& entity) {
    if (entity.mode == MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP) {
        return SOUND_MUSKET;
    } else if (entity.type == ENTITY_COWBOY || entity.type == ENTITY_JOCKEY || entity.type == ENTITY_SPY) {
        return SOUND_GUN;
    } else if (entity.type == ENTITY_BANDIT || entity.type == ENTITY_SOLDIER) {
        return SOUND_SWORD;
    } else if (entity.type == ENTITY_CANNON) {
        return SOUND_CANNON;
    } else {
        return SOUND_PUNCH;
    }
}

Sound entity_get_death_sound(EntityType type) {
    if (entity_is_unit(type)) {
        switch (type) {
            case ENTITY_WAGON:
            case ENTITY_WAR_WAGON:
            case ENTITY_JOCKEY:
                return SOUND_DEATH_CHICKEN;
            default:
                return SOUND_DEATH;
        }
    } else {
        switch (type) {
            case ENTITY_BUNKER:
                return SOUND_BUNKER_DESTROY;
            case ENTITY_LAND_MINE:
                return SOUND_MINE_DESTROY;
            default:
                return SOUND_BUILDING_DESTROY;
        }
    }
}

bool entity_should_die(const entity_t& entity) {
    if (entity.health != 0) {
        return false;
    }

    if (entity_is_unit(entity.type)) {
        if (entity.mode == MODE_UNIT_DEATH || entity.mode == MODE_UNIT_DEATH_FADE) {
            return false;
        }
        if(entity.garrison_id != ID_NULL) {
            return false;
        }

        return true;
    } else if (entity_is_building(entity.type)) {
        return entity.mode != MODE_BUILDING_DESTROYED;
    }

    return false;
}

SDL_Rect entity_get_sight_rect(const entity_t& entity) {
    int entity_sight = ENTITY_DATA.at(entity.type).sight;
    return (SDL_Rect) { .x = entity.cell.x - entity_sight, .y = entity.cell.y - entity_sight, .w = (2 * entity_sight) + 1, .h = (2 * entity_sight) + 1 };
}

bool entity_can_see_rect(const entity_t& entity, xy rect_position, int rect_size) {
    SDL_Rect sight_rect = entity_get_sight_rect(entity);
    SDL_Rect rect = (SDL_Rect) { .x = rect_position.x, .y = rect_position.y, .w = rect_size, .h = rect_size };
    return SDL_HasIntersection(&sight_rect, &rect) == SDL_TRUE;
}

target_t entity_target_nearest_enemy(const match_state_t& state, const entity_t& entity) {
    SDL_Rect entity_rect = (SDL_Rect) { .x = entity.cell.x, .y = entity.cell.y, .w = entity_cell_size(entity.type),. h = entity_cell_size(entity.type) };
    SDL_Rect entity_sight_rect = entity_get_sight_rect(entity);
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;
    uint32_t nearest_attack_priority;

    for (uint32_t other_index = 0; other_index < state.entities.size(); other_index++) {
        const entity_t& other = state.entities[other_index];

        if (other.type == ENTITY_GOLD_MINE) {
            continue;
        }

        SDL_Rect other_rect = (SDL_Rect) { .x = other.cell.x, .y = other.cell.y, .w = entity_cell_size(other.type),. h = entity_cell_size(other.type) };
        if (other.player_id == entity.player_id || !entity_is_selectable(other) || 
                !map_is_cell_rect_revealed(state, entity.player_id, other.cell, entity_cell_size(other.type)) || 
                (entity_check_flag(other, ENTITY_FLAG_INVISIBLE) && state.map_detection[entity.player_id][other.cell.x + (other.cell.y * state.map_width)] == 0) ||
                SDL_HasIntersection(&entity_sight_rect, &other_rect) != SDL_TRUE) {
            continue;
        }

        int other_dist = euclidean_distance_squared_between(entity_rect, other_rect);
        uint32_t other_attack_priority = ENTITY_DATA.at(other.type).attack_priority;
        if (nearest_enemy_index == INDEX_INVALID || other_attack_priority > nearest_attack_priority || (other_dist < nearest_enemy_dist && other_attack_priority == nearest_attack_priority)) {
            nearest_enemy_index = other_index;
            nearest_enemy_dist = other_dist;
            nearest_attack_priority = other_attack_priority;
        }
    }

    if (nearest_enemy_index == INDEX_INVALID) {
        return (target_t) {
            .type = TARGET_NONE
        };
    }

    return (target_t) {
        .type = TARGET_ATTACK_ENTITY,
        .id = state.entities.get_id_of(nearest_enemy_index)
    };
}

target_t entity_target_nearest_gold_mine(const match_state_t& state, const entity_t& entity) {
    SDL_Rect entity_rect = (SDL_Rect) { .x = entity.cell.x, .y = entity.cell.y, .w = entity_cell_size(entity.type),. h = entity_cell_size(entity.type) };
    uint32_t nearest_index = INDEX_INVALID;
    xy nearest_cell = xy(-1, -1);
    int nearest_dist = -1;

    for (uint32_t gold_index = 0; gold_index < state.entities.size(); gold_index++) {
        const entity_t& gold = state.entities[gold_index];

        if (gold.type != ENTITY_GOLD_MINE || gold.gold_held == 0 ||
                (state.map_fog[entity.player_id][gold.cell.x + (gold.cell.y * state.map_width)] == FOG_HIDDEN)) {
            continue;
        }

        SDL_Rect gold_mine_rect = (SDL_Rect) { .x = gold.cell.x, .y = gold.cell.y, .w = entity_cell_size(gold.type), .h = entity_cell_size(gold.type) };
        int gold_mine_dist = euclidean_distance_squared_between(entity_rect, gold_mine_rect);
        if (nearest_index == INDEX_INVALID || gold_mine_dist < nearest_dist) {
            nearest_index = gold_index;
            nearest_dist = gold_mine_dist;
        }
    }

    if (nearest_index == INDEX_INVALID) {
        return (target_t) {
            .type = TARGET_NONE
        };
    }

    return (target_t) {
        .type = TARGET_ENTITY,
        .id = state.entities.get_id_of(nearest_index)
    };
}

target_t entity_target_nearest_camp(const match_state_t& state, const entity_t& entity) {
    SDL_Rect entity_rect = (SDL_Rect) { .x = entity.cell.x, .y = entity.cell.y, .w = entity_cell_size(entity.type),. h = entity_cell_size(entity.type) };
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;

    for (uint32_t other_index = 0; other_index < state.entities.size(); other_index++) {
        const entity_t& other = state.entities[other_index];

        if (other.player_id != entity.player_id || !(other.type == ENTITY_CAMP || other.type == ENTITY_HALL) || other.mode != MODE_BUILDING_FINISHED) {
            continue;
        }

        SDL_Rect other_rect = (SDL_Rect) { .x = other.cell.x, .y = other.cell.y, .w = entity_cell_size(other.type),. h = entity_cell_size(other.type) };

        int other_dist = euclidean_distance_squared_between(entity_rect, other_rect);
        if (nearest_enemy_index == INDEX_INVALID || other_dist < nearest_enemy_dist) {
            nearest_enemy_index = other_index;
            nearest_enemy_dist = other_dist;
        }
    }

    if (nearest_enemy_index == INDEX_INVALID) {
        return (target_t) {
            .type = TARGET_NONE
        };
    }

    return (target_t) {
        .type = TARGET_ENTITY,
        .id = state.entities.get_id_of(nearest_enemy_index)
    };
}

bool entity_is_mining(const match_state_t& state, const entity_t& entity) {
    if (entity.target.type != TARGET_ENTITY) {
        return false;
    }
    const entity_t& target = state.entities.get_by_id(entity.target.id);
    return (target.type == ENTITY_GOLD_MINE && target.gold_held > 0) || 
           ((target.type == ENTITY_HALL || target.type == ENTITY_CAMP) && target.mode == MODE_BUILDING_FINISHED && entity.player_id == target.player_id && entity.gold_held > 0);
}

SDL_Rect entity_gold_get_block_building_rect(xy cell) {
    return (SDL_Rect) { .x = cell.x - 4, .y = cell.y - 4, .w = 11, .h = 11 };
}

uint32_t entity_get_garrisoned_occupancy(const match_state_t& state, const entity_t& entity) {
    uint32_t occupancy = 0;
    for (entity_id id : entity.garrisoned_units) {
        occupancy += ENTITY_DATA.at(state.entities.get_by_id(id).type).garrison_size;
    }
    return occupancy;
}

void entity_set_target(entity_t& entity, target_t target) {
    GOLD_ASSERT(entity.mode != MODE_UNIT_BUILD);
    entity.target = target;
    entity.path.clear();
    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, false);

    if (entity.mode != MODE_UNIT_MOVE) {
        // Abandon current behavior in favor of new order
        entity.timer = 0;
        entity.pathfind_attempts = 0;
        entity.mode = MODE_UNIT_IDLE;
    }
}

void entity_attack_target(match_state_t& state, entity_id attacker_id, entity_t& defender) {
    entity_t& attacker = state.entities.get_by_id(attacker_id);
    bool attack_missed = false;

    bool attack_with_bayonets = attacker.type == ENTITY_SOLDIER && attacker.mode == MODE_UNIT_ATTACK_WINDUP;
    int attacker_damage = attack_with_bayonets ? SOLDIER_BAYONET_DAMAGE : ENTITY_DATA.at(attacker.type).unit_data.damage;
    int defender_armor = ENTITY_DATA.at(defender.type).armor;
    int damage = std::max(1, attacker_damage - defender_armor);
    int accuracy = 100;

    if (entity_get_elevation(state, attacker) < entity_get_elevation(state, defender)) {
        accuracy /= 2;
    }

    // check for smoke clouds
    bool is_ranged_attack = !(attack_with_bayonets || ENTITY_DATA.at(attacker.type).unit_data.range_squared == 1);
    if (is_ranged_attack) {
        SDL_Rect defender_rect = entity_get_rect(defender);
        for (const particle_t& particle : state.particles) {
            if (particle.animation.name != ANIMATION_PARTICLE_SMOKE) {
                continue;
            }

            SDL_Rect particle_rect = (SDL_Rect) {
                .x = particle.position.x - (engine.sprites[SPRITE_PARTICLE_SMOKE].frame_size.x / 2),
                .y = particle.position.y - (engine.sprites[SPRITE_PARTICLE_SMOKE].frame_size.x / 2),
                .w = engine.sprites[SPRITE_PARTICLE_SMOKE].frame_size.x,
                .h = engine.sprites[SPRITE_PARTICLE_SMOKE].frame_size.y
            };
            if (SDL_HasIntersection(&particle_rect, &defender_rect) == SDL_TRUE) {
                accuracy = 0;
                break;
            }
        }
    }

    if (accuracy < lcg_rand() % 100) {
        attack_missed = true;
    }
    
    if (!attack_missed) {
        xy defender_center_position = entity_is_unit(defender.type) 
            ? defender.position.to_xy() 
            : ((defender.cell * TILE_SIZE) + ((xy(entity_cell_size(defender.type), entity_cell_size(defender.type)) * TILE_SIZE) / 2));
        if (attacker.type == ENTITY_CANNON) {
            // Check which enemies we hit. 
            int attacker_damage = ENTITY_DATA.at(attacker.type).unit_data.damage;
            SDL_Rect full_damage_rect = (SDL_Rect) {
                .x = defender_center_position.x - (TILE_SIZE / 2), .y = defender_center_position.y - (TILE_SIZE / 2),
                .w = TILE_SIZE, .h = TILE_SIZE
            };
            SDL_Rect splash_damage_rect = (SDL_Rect) {
                .x = full_damage_rect.x - (TILE_SIZE / 2), .y = full_damage_rect.y - (TILE_SIZE / 2),
                .w = full_damage_rect.w + TILE_SIZE, .h = full_damage_rect.h + TILE_SIZE
            };
            for (entity_t& defender : state.entities) {
                if (defender.type == ENTITY_GOLD_MINE || !entity_is_selectable(defender)) {
                    continue;
                }

                // To check if we've hit this enemy, first check the splash damage rect
                // We have to check both, but since the splash rect is bigger, we know that 
                // if they're outside of it, they will be outside of the full damage rect as well
                SDL_Rect defender_rect = entity_get_rect(defender);
                if (SDL_HasIntersection(&defender_rect, &splash_damage_rect) == SDL_TRUE) {
                    int damage = attacker_damage; 
                    // Half damage if they are only within splash damage range
                    if (SDL_HasIntersection(&defender_rect, &full_damage_rect) != SDL_TRUE) {
                        damage /= 2;
                    }
                    damage = std::max(1, damage - ENTITY_DATA.at(defender.type).armor);

                    defender.health = std::max(0, defender.health - damage);
                    entity_on_attack(state, attacker_id, defender);
                }
            }
        } else {
            defender.health = std::max(0, defender.health - damage);
        }


        // Create particle effect
        if (attacker.type == ENTITY_COWBOY || (attacker.type == ENTITY_SOLDIER && !attack_with_bayonets) || attacker.type == ENTITY_SPY || attacker.type == ENTITY_JOCKEY) {
            SDL_Rect defender_rect = entity_get_rect(defender);

            xy particle_position;
            particle_position.x = defender_rect.x + (defender_rect.w / 4) + (lcg_rand() % (defender_rect.w / 2));
            particle_position.y = defender_rect.y + (defender_rect.h / 4) + (lcg_rand() % (defender_rect.h / 2));
            state.particles.push_back((particle_t) {
                .sprite = SPRITE_PARTICLE_SPARKS,
                .animation = animation_create(ANIMATION_PARTICLE_SPARKS),
                .vframe = lcg_rand() % 3,
                .position = particle_position
            });
        } else if (attacker.type == ENTITY_CANNON) {
            // Create particle
            state.particles.push_back((particle_t) {
                .sprite = SPRITE_PARTICLE_CANNON_EXPLOSION,
                .animation = animation_create(ANIMATION_PARTICLE_CANNON_EXPLOSION),
                .vframe = 0,
                .position = defender_center_position
            });
        }

        entity_on_attack(state, attacker_id, defender);
    }

    // Add bunker particle for garrisoned unit. Happens even if they miss
    if (attacker.garrison_id != ID_NULL) {
        entity_t& carrier = state.entities.get_by_id(attacker.garrison_id);
        int particle_index = lcg_rand() % 4;
        xy particle_position;
        if (carrier.type == ENTITY_BUNKER) {
            particle_position = (carrier.cell * TILE_SIZE) + ENTITY_BUNKER_PARTICLE_OFFSETS[particle_index];
        } else if (carrier.type == ENTITY_WAR_WAGON) {
            particle_position = carrier.position.to_xy() - (engine.sprites[SPRITE_UNIT_WAR_WAGON].frame_size / 2);
            if (carrier.direction == DIRECTION_SOUTH) {
                particle_position += ENTITY_WAR_WAGON_DOWN_PARTICLE_OFFSETS[particle_index];
            } else if (carrier.direction == DIRECTION_NORTH) {
                particle_position += ENTITY_WAR_WAGON_UP_PARTICLE_OFFSETS[particle_index];
            } else {
                xy offset = ENTITY_WAR_WAGON_RIGHT_PARTICLE_OFFSETS[particle_index];
                if (carrier.direction > DIRECTION_SOUTH) {
                    offset.x = engine.sprites[SPRITE_UNIT_WAR_WAGON].frame_size.x - offset.x;
                }
                particle_position += offset;
            }
        }
        state.particles.push_back((particle_t) {
            .sprite = SPRITE_PARTICLE_BUNKER_COWBOY,
            .animation = animation_create(ANIMATION_PARTICLE_BUNKER_COWBOY),
            .vframe = 0,
            .position = particle_position
        });
    }

    // Reveal cell if on highground
    if (entity_get_elevation(state, attacker) > entity_get_elevation(state, defender) && !entity_check_flag(attacker, ENTITY_FLAG_INVISIBLE)) {
        log_trace("map reveal");
        map_reveal_t reveal = (map_reveal_t) {
            .player_id = defender.player_id,
            .cell = attacker.cell,
            .cell_size = entity_cell_size(attacker.type),
            .sight = 3,
            .timer = 60
        };
        map_fog_update(state, reveal.player_id, reveal.cell, reveal.cell_size, reveal.sight, true, false);
        state.map_reveals.push_back(reveal);
    }
}

void entity_on_attack(match_state_t& state, entity_id attacker_id, entity_t& defender) {
    entity_t& attacker = state.entities.get_by_id(attacker_id);

    // Alerts / Taking damage flicker
    if (attacker.player_id != defender.player_id) {
        if (defender.taking_damage_counter == 0) {
            match_event_alert(state, MATCH_ALERT_TYPE_ATTACK, defender.player_id, defender.cell, entity_cell_size(defender.type));
        }
    }
    defender.taking_damage_counter = 3;
    if (defender.taking_damage_timer == 0) {
        defender.taking_damage_timer = MATCH_TAKING_DAMAGE_FLICKER_DURATION;
    }

    // Health regen timer
    if (entity_is_unit(defender.type)) {
        defender.health_regen_timer = UNIT_HEALTH_REGEN_DURATION + UNIT_HEALTH_REGEN_DELAY;
    }

    // Make the enemy attack back
    if (entity_is_unit(defender.type) && defender.mode == MODE_UNIT_IDLE && 
            defender.target.type == TARGET_NONE && ENTITY_DATA.at(defender.type).unit_data.damage != 0 && 
            defender.player_id != attacker.player_id && map_is_cell_rect_revealed(state, defender.player_id, attacker.cell, entity_cell_size(attacker.type)) &&
            !(entity_check_flag(attacker, ENTITY_FLAG_INVISIBLE) && state.map_detection[defender.player_id][attacker.cell.x + (attacker.cell.y * state.map_width)] == 0)) {
        defender.target = (target_t) {
            .type = TARGET_ATTACK_ENTITY,
            .id = attacker_id
        };
    }
}

void entity_explode(match_state_t& state, entity_id id) {
    entity_t& entity = state.entities.get_by_id(id);

    // Apply damage
    SDL_Rect explosion_rect = (SDL_Rect) { 
        .x = (entity.cell.x - 1) * TILE_SIZE,
        .y = (entity.cell.y - 1) * TILE_SIZE,
        .w = TILE_SIZE * 3,
        .h = TILE_SIZE * 3
    };
    int explosion_damage = entity.type == ENTITY_SAPPER ? ENTITY_DATA.at(entity.type).unit_data.damage : MINE_EXPLOSION_DAMAGE;
    for (uint32_t defender_index = 0; defender_index < state.entities.size(); defender_index++) {
        if (defender_index == state.entities.get_index_of(id)) {
            continue;
        }
        entity_t& defender = state.entities[defender_index];
        if (defender.type == ENTITY_GOLD_MINE || !entity_is_selectable(defender)) {
            continue;
        }

        SDL_Rect defender_rect = entity_get_rect(defender);
        if (SDL_HasIntersection(&defender_rect, &explosion_rect)) {
            int defender_armor = ENTITY_DATA.at(defender.type).armor;
            int damage = std::max(1, explosion_damage - defender_armor);
            defender.health = std::max(defender.health - damage, 0);
            entity_on_attack(state, id, defender);
        }
    }

    // Kill the entity
    entity.health = 0;
    if (entity.type == ENTITY_SAPPER) {
        entity.target = (target_t) { .type = TARGET_NONE };
        entity.mode = MODE_UNIT_DEATH_FADE;
        entity.animation = animation_create(ANIMATION_UNIT_DEATH_FADE);
        map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
    } else {
        entity.mode = MODE_BUILDING_DESTROYED;
        entity.timer = BUILDING_FADE_DURATION;
        state.map_mine_cells[entity.cell.x + (entity.cell.y * state.map_width)] = ID_NULL;
    }

    match_event_play_sound(state, SOUND_EXPLOSION, entity.position.to_xy());

    // Create particle
    state.particles.push_back((particle_t) {
        .sprite = SPRITE_PARTICLE_EXPLOSION,
        .animation = animation_create(ANIMATION_PARTICLE_EXPLOSION),
        .vframe = 0,
        .position = entity.type == ENTITY_SAPPER ? entity.position.to_xy() : cell_center(entity.cell).to_xy()
    });
}

xy entity_get_exit_cell(const match_state_t& state, xy building_cell, int building_size, int unit_size, xy rally_cell, bool allow_blocked_cells) {
    xy exit_cell = xy(-1, -1);
    int exit_cell_dist = -1;
    for (int x = building_cell.x - unit_size; x < building_cell.x + building_size + unit_size; x++) {
        xy cell = xy(x, building_cell.y - unit_size);
        int cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
                (allow_blocked_cells || !map_is_cell_rect_occupied(state, cell, unit_size)) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = xy(x, building_cell.y + building_size);
        cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
                (allow_blocked_cells || !map_is_cell_rect_occupied(state, cell, unit_size)) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }
    for (int y = building_cell.y - unit_size; y < building_cell.y + building_size + unit_size; y++) {
        xy cell = xy(building_cell.x - unit_size, y);
        int cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
                (allow_blocked_cells || !map_is_cell_rect_occupied(state, cell, unit_size)) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = xy(building_cell.x + building_size, y);
        cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
                (allow_blocked_cells || !map_is_cell_rect_occupied(state, cell, unit_size)) &&
                (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }

    return exit_cell;
}

void entity_remove_garrisoned_unit(entity_t& entity, entity_id garrisoned_unit_id) {
    for (uint32_t index = 0; index < entity.garrisoned_units.size(); index++) {
        if (entity.garrisoned_units[index] == garrisoned_unit_id) {
            entity.garrisoned_units.erase(entity.garrisoned_units.begin() + index);
            return;
        }
    }
}

void entity_unload_unit(match_state_t& state, entity_t& entity, entity_id garrisoned_unit_id) {
    uint32_t index = 0;
    while (index < entity.garrisoned_units.size()) {
        if (garrisoned_unit_id == ENTITY_UNLOAD_ALL || entity.garrisoned_units[index] == garrisoned_unit_id) {
            entity_t& garrisoned_unit = state.entities.get_by_id(entity.garrisoned_units[index]);

            // Find the exit cell
            xy exit_cell = entity_get_exit_cell(state, entity.cell, entity_cell_size(entity.type), entity_cell_size(garrisoned_unit.type), entity.cell + xy(0, entity_cell_size(entity.type)));
            if (exit_cell.x == -1) {
                if (entity_is_building(entity.type)) {
                    state.events.push_back((match_event_t) { .type = MATCH_EVENT_BUILDING_EXIT_BLOCKED, .player_id = garrisoned_unit.player_id });
                }
                return;
            }

            // Place the unit in the world
            garrisoned_unit.cell = exit_cell;
            garrisoned_unit.position = entity_get_target_position(garrisoned_unit);
            map_set_cell_rect(state, garrisoned_unit.cell, entity_cell_size(garrisoned_unit.type), entity.garrisoned_units[index]);
            map_fog_update(state, garrisoned_unit.player_id, garrisoned_unit.cell, entity_cell_size(garrisoned_unit.type), ENTITY_DATA.at(garrisoned_unit.type).sight, true, ENTITY_DATA.at(entity.type).has_detection);
            garrisoned_unit.mode = MODE_UNIT_IDLE;
            garrisoned_unit.target = (target_t) { .type = TARGET_NONE };
            garrisoned_unit.garrison_id = ID_NULL;

            // Remove the unit from the garrisoned units list
            entity.garrisoned_units.erase(entity.garrisoned_units.begin() + index);

            match_event_play_sound(state, SOUND_GARRISON_OUT, entity.position.to_xy());
        } else {
            index++;
        }
    }
}

void entity_stop_building(match_state_t& state, entity_id id) {
    entity_t& entity = state.entities.get_by_id(id);

    xy exit_cell = entity.target.build.building_cell + xy(-1, 0);
    xy search_corners[4] = {
        entity.target.build.building_cell + xy(-1, entity_cell_size(entity.target.build.building_type)),
        entity.target.build.building_cell + xy(entity_cell_size(entity.target.build.building_type), entity_cell_size(entity.target.build.building_type)),
        entity.target.build.building_cell + xy(entity_cell_size(entity.target.build.building_type), -1),
        entity.target.build.building_cell + xy(-1, -1)
    };
    const Direction search_directions[4] = { DIRECTION_SOUTH, DIRECTION_EAST, DIRECTION_NORTH, DIRECTION_WEST };
    int search_index = 0;
    while (!map_is_cell_in_bounds(state, exit_cell) || map_is_cell_rect_occupied(state, exit_cell, entity_cell_size(entity.type))) {
        exit_cell += DIRECTION_XY[search_directions[search_index]];
        if (exit_cell == search_corners[search_index]) {
            search_index++;
            if (search_index == 4) {
                search_index = 0;
                search_corners[0] += xy(-1, 1);
                search_corners[1] += xy(1, 1);
                search_corners[2] += xy(1, -1);
                search_corners[3] += xy(-1, -1);
            }
        }
    }

    entity.cell = exit_cell;
    entity.position = entity_get_target_position(entity);
    entity.target = (target_t) {
        .type = TARGET_NONE
    };
    entity.mode = MODE_UNIT_IDLE;
    map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);
    map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, true, ENTITY_DATA.at(entity.type).has_detection);
}

void entity_building_finish(match_state_t& state, entity_id building_id) {
    entity_t& building = state.entities.get_by_id(building_id);

    building.mode = MODE_BUILDING_FINISHED;

    // Show alert
    match_event_alert(state, MATCH_ALERT_TYPE_BUILDING, building.player_id, building.cell, entity_cell_size(building.type));

    for (uint32_t entity_index = 0; entity_index < state.entities.size(); entity_index++) {
        entity_t& entity = state.entities[entity_index];
        if (!entity_is_unit(entity.type)) {
            continue;
        }
        if (entity.target.id != building_id || !(entity.target.type == TARGET_BUILD || entity.target.type == TARGET_REPAIR)) {
            continue;
        }

        if (entity.target.type == TARGET_BUILD) {
            entity_stop_building(state, state.entities.get_id_of(entity_index));
            // If the unit was unable to stop building, notify the user that the exit is blocked
            if (entity.mode != MODE_UNIT_IDLE) {
                state.events.push_back((match_event_t) { .type = MATCH_EVENT_BUILDING_EXIT_BLOCKED, .player_id = entity.player_id });
            }
        } else if (entity.mode == MODE_UNIT_REPAIR) {
            entity.mode = MODE_UNIT_IDLE;
        }

        entity.target = building.type == ENTITY_CAMP || building.type == ENTITY_HALL
                            ? entity_target_nearest_gold_mine(state, entity)
                            : (target_t) { .type = TARGET_NONE };
    }
}

void entity_building_enqueue(match_state_t& state, entity_t& building, building_queue_item_t item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    building.queue.push_back(item);
    if (building.queue.size() == 1) {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.timer != BUILDING_QUEUE_BLOCKED) {
                state.events.push_back((match_event_t) { .type = MATCH_EVENT_NOT_ENOUGH_HOUSE, .player_id = building.player_id });
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(item);
        }
    }
}

void entity_building_dequeue(match_state_t& state, entity_t& building) {
    GOLD_ASSERT(!building.queue.empty());

    building.queue.erase(building.queue.begin());
    if (building.queue.empty()) {
        building.timer = 0;
    } else {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.timer != BUILDING_QUEUE_BLOCKED) {
                state.events.push_back((match_event_t) { .type = MATCH_EVENT_NOT_ENOUGH_HOUSE, .player_id = building.player_id });
            }
            building.timer = BUILDING_QUEUE_BLOCKED;
        } else {
            building.timer = building_queue_item_duration(building.queue[0]);
        }
    }
}

bool entity_building_is_supply_blocked(const match_state_t& state, const entity_t& building) {
    const building_queue_item_t& item = building.queue[0];
    if (item.type == BUILDING_QUEUE_ITEM_UNIT) {
        uint32_t required_population = match_get_player_population(state, building.player_id) + ENTITY_DATA.at(item.unit_type).unit_data.population_cost;
        if (match_get_player_max_population(state, building.player_id) < required_population) {
            return true;
        }
    }
    return false;
}

UiButton building_queue_item_icon(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).ui_button;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return UPGRADE_DATA.at(item.upgrade).ui_button;
        }
    }
}

uint32_t building_queue_item_duration(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).train_duration * 60;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return UPGRADE_DATA.at(item.upgrade).research_duration * 60;
        }
    }
}

uint32_t building_queue_item_cost(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).gold_cost;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return UPGRADE_DATA.at(item.upgrade).gold_cost;
        }
    }
}

uint32_t building_queue_population_cost(const building_queue_item_t& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).unit_data.population_cost;
        }
        default:
            return 0;
    }
}

void entity_on_death_release_garrisoned_units(match_state_t& state, entity_t& entity) {
    for (entity_id garrisoned_unit_id : entity.garrisoned_units) {
        entity_t& garrisoned_unit = state.entities.get_by_id(garrisoned_unit_id);
        // place garrisoned units inside former-self
        bool unit_is_placed = false;
        for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size(entity.type); x++) {
            for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size(entity.type); y++) {
                if (!map_is_cell_rect_occupied(state, xy(x, y), entity_cell_size(garrisoned_unit.type))) {
                    garrisoned_unit.cell = xy(x, y);
                    garrisoned_unit.position = entity_get_target_position(garrisoned_unit);
                    garrisoned_unit.garrison_id = ID_NULL;
                    garrisoned_unit.mode = MODE_UNIT_IDLE;
                    garrisoned_unit.target = (target_t) { .type = TARGET_NONE };
                    map_set_cell_rect(state, garrisoned_unit.cell, entity_cell_size(garrisoned_unit.type), garrisoned_unit_id);
                    map_fog_update(state, garrisoned_unit.player_id, garrisoned_unit.cell, entity_cell_size(garrisoned_unit.type), ENTITY_DATA.at(garrisoned_unit.type).sight, true, ENTITY_DATA.at(garrisoned_unit.type).has_detection);
                    log_trace("placed unit %u at cell %xi position %xd", garrisoned_unit_id, &garrisoned_unit.cell, &garrisoned_unit.position);
                    unit_is_placed = true;
                    break;
                }
            }
            if (unit_is_placed) {
                break;
            }
        }

        if (!unit_is_placed) {
            log_warn("Unable to place garrisoned unit.");
        }
    }
}