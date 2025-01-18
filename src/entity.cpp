#include "match.h"

#include "network.h"
#include "logger.h"
#include "lcg.h"
#include <unordered_map>
#include <algorithm>

static const uint32_t UNIT_MOVE_BLOCKED_DURATION = 30;
static const uint32_t UNIT_BUILD_TICK_DURATION = 6;
static const uint32_t UNIT_MINE_TICK_DURATION = 60;
static const uint32_t UNIT_MAX_GOLD_HELD = 10;
static const uint32_t UNIT_REPAIR_RATE = 4;
static const uint32_t GOLD_LOW_THRESHOLD = 500;
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
static const int MINE_EXPLOSION_DAMAGE = 101;
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
            .attack_cooldown = 16,
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

            .damage = 6,
            .attack_cooldown = 30,
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
            .speed = fixed::from_int_and_raw_decimal(0, 225),

            .damage = 5,
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

        .gold_cost = 150,
        .train_duration = 38,
        .max_health = 100,
        .sight = 7,
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

        .gold_cost = 150,
        .train_duration = 38,
        .max_health = 100,
        .sight = 7,
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

        .gold_cost = 125,
        .train_duration = 35,
        .max_health = 50,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 40),

            .damage = 6,
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

        .gold_cost = 150,
        .train_duration = 30,
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

            .damage = 101,
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
        .max_health = 50,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 180),

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
        .max_health = 75,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,

        .has_detection = false,

        .unit_data = (unit_data_t) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 140),

            .damage = 30,
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

        .gold_cost = 75,
        .train_duration = 25,
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
            .attack_cooldown = 30,
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
        .max_health = 1000,
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
        .max_health = 600,
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
            .builder_positions_x = { 6, 27, 14 },
            .builder_positions_y = { 27, 7, 1 },
            .builder_flip_h = { false, true, false },
            .can_rally = true
        }
    }},
    { ENTITY_MINE, (entity_data_t) {
        .name = "Mine",
        .sprite = SPRITE_BUILDING_MINE,
        .ui_button = UI_BUTTON_BUILD_MINE,
        .cell_size = 1,

        .gold_cost = 25,
        .train_duration = 0,
        .max_health = 5,
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
    { ENTITY_GOLD, (entity_data_t) {
        .name = "Gold",
        .sprite = SPRITE_TILE_GOLD,
        .ui_button = UI_BUTTON_MINE,
        .cell_size = 1,

        .gold_cost = 0,
        .train_duration = 0,
        .max_health = 0,
        .sight = 0,
        .armor = 0,
        .attack_priority = 0,

        .garrison_capacity = 0,
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

    entity.health = (entity_is_unit(type) || entity.type == ENTITY_MINE) ? entity_data.max_health : entity_data.max_health / 10;
    entity.target = (target_t) { .type = TARGET_NONE };
    entity.remembered_gold_target = (target_t) { .type = TARGET_NONE };
    entity.pathfind_attempts = 0;
    entity.timer = 0;
    entity.rally_point = xy(-1, -1);

    entity.animation = animation_create(ANIMATION_UNIT_IDLE);

    entity.garrison_id = ID_NULL;
    entity.cooldown_timer = 0;
    entity.gold_held = 0;
    entity.gold_patch_id = GOLD_PATCH_ID_NULL;

    entity.taking_damage_timer = 0;
    entity.health_regen_timer = 0;

    if (entity.type == ENTITY_MINE) {
        entity.timer = MINE_ARM_DURATION;
        entity.mode = MODE_MINE_ARM;
    }
    if (entity.type == ENTITY_SPY) {
        entity_set_flag(entity, ENTITY_FLAG_INVISIBLE, true);
    }

    entity_id id = state.entities.push_back(entity);
    if (entity.type == ENTITY_MINE) {
        state.map_mine_cells[entity.cell.x + (entity.cell.y * state.map_width)] = id;
    } else {
        map_set_cell_rect(state, entity.cell, entity_cell_size(type), id);
    }
    map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, true, ENTITY_DATA.at(entity.type).has_detection);

    return id;
}

entity_id entity_create_gold(match_state_t& state, xy cell, uint32_t gold_left, uint32_t gold_patch_id) {
    entity_t entity;
    entity.type = ENTITY_GOLD;
    entity.player_id = PLAYER_NONE;
    entity.flags = 0;
    entity.mode = MODE_GOLD;

    entity.cell = cell;
    entity.position = xy_fixed(entity.cell * TILE_SIZE);
    entity.direction = DIRECTION_SOUTH;

    entity.garrison_id = ID_NULL;
    entity.gold_held = gold_left;
    entity.gold_patch_id = gold_patch_id;

    entity.animation.frame = xy(lcg_rand() % 3, 0);

    entity_id id = state.entities.push_back(entity);
    map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);

    return id;
}

void entity_update(match_state_t& state, uint32_t entity_index) {
    entity_id id = state.entities.get_id_of(entity_index);
    entity_t& entity = state.entities[entity_index];
    const entity_data_t& entity_data = ENTITY_DATA.at(entity.type);

    if (entity.type == ENTITY_GOLD) {
        if (entity.gold_held == 0) {
            ui_deselect_entity_if_selected(state, id);
            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
            map_recalculate_unreachable_cells(state, entity.cell);
            // Triggers gold removal from entities list. This is done this way so that we can guarantee that the cell is set empty before deletion
            entity.mode = MODE_GOLD_MINED_OUT;
        }
        return;
    }

    if (entity_should_die(entity)) {
        if (entity_is_unit(entity.type)) {
            entity.mode = MODE_UNIT_DEATH;
            entity.animation = animation_create(entity_get_expected_animation(entity));
        } else {
            entity.mode = MODE_BUILDING_DESTROYED;
            entity.timer = BUILDING_FADE_DURATION;
            entity.queue.clear();

            if (entity.type == ENTITY_MINE) {
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

        ui_deselect_entity_if_selected(state, id);

        // Handle garrisoned units
        for (entity_id garrisoned_unit_id : entity.garrisoned_units) {
            entity_t& garrisoned_unit = state.entities.get_by_id(garrisoned_unit_id);
            if (entity_is_unit(entity.type)) {
                garrisoned_unit.health = 0;
            } else {
                // For buildings, place garrisoned units inside former-self
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

                // If unit is idle, try to find a nearby target
                if (entity.target.type == TARGET_NONE && entity.type != ENTITY_MINER && ENTITY_DATA.at(entity.type).unit_data.damage != 0) {
                    entity.target = entity_target_nearest_enemy(state, entity.garrison_id == ID_NULL ? entity : state.entities.get_by_id(entity.garrison_id));
                }

                if (entity.target.type == TARGET_NONE) {
                    update_finished = true;
                    break;
                }

                if (entity_is_target_invalid(state, entity)) {
                    entity.target = entity.target.type == TARGET_GOLD 
                                        ? entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id)
                                        : (target_t) { .type = TARGET_NONE };
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

                map_pathfind(state, entity.cell, entity_get_target_cell(state, entity), entity_cell_size(entity.type), &entity.path, entity_should_gold_walk(state, entity));
                if (!entity.path.empty()) {
                    entity.pathfind_attempts = 0;
                    entity.mode = MODE_UNIT_MOVE;
                    break;
                } else {
                    entity.pathfind_attempts++;
                    if (entity.pathfind_attempts == 3) {
                        if (entity.player_id == network_get_player_id() && entity.target.type == TARGET_BUILD) {
                            ui_show_status(state, UI_STATUS_CANT_BUILD);
                        }
                        entity.target = (target_t) { .type = TARGET_NONE };
                        update_finished = true;
                        break;
                    } else if (entity.target.type == TARGET_GOLD) {
                        entity.target = entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                        entity.mode = MODE_UNIT_IDLE;
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
                        if (map_is_cell_rect_occupied(state, entity.path[0], entity_cell_size(entity.type), entity.cell, entity_should_gold_walk(state, entity))) {
                            path_is_blocked = true;
                            // breaks out of while movement left
                            break;
                        }

                        if (map_is_cell_rect_equal_to(state, entity.cell, entity_cell_size(entity.type), id)) {
                            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                        }
                        map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, false, ENTITY_DATA.at(entity.type).has_detection);
                        entity.cell = entity.path[0];
                        if (!entity_should_gold_walk(state, entity)) {
                            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);
                        } 
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
                            if (mine.type != ENTITY_MINE || mine.health == 0 || mine.mode != MODE_BUILDING_FINISHED || mine.player_id == entity.player_id ||
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
                            entity.target = entity.target.type == TARGET_GOLD
                                                ? entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id) 
                                                : (target_t) { .type = TARGET_NONE };
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
                    if (entity.target.type == TARGET_GOLD) {
                        entity.target = entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                        if (entity.target.type != TARGET_NONE && map_get_cell(state, entity.target.cell) != CELL_EMPTY) {
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        // else, if the new target was also blocked, then just path pause
                    }
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
                            if (entity.player_id == network_get_player_id()) {
                                ui_show_status(state, UI_STATUS_CANT_BUILD);
                            }
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }
                        if (state.player_gold[entity.player_id] < ENTITY_DATA.at(entity.target.build.building_type).gold_cost) {
                            if (entity.player_id == network_get_player_id()) {
                                ui_show_status(state, UI_STATUS_NOT_ENOUGH_GOLD);
                            }
                            entity.target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        state.player_gold[entity.player_id] -= ENTITY_DATA.at(entity.target.build.building_type).gold_cost;

                        if (entity.target.build.building_type == ENTITY_MINE) {
                            entity_create(state, entity.target.build.building_type, entity.player_id, entity.target.build.building_cell);

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
                        ui_deselect_entity_if_selected(state, id);

                        bool is_targeting_only_builders = true;
                        for (entity_id selected_id : state.selection) {
                            entity_t& selected_entity = state.entities.get_by_id(selected_id);
                            if (!(selected_entity.target.type == TARGET_BUILD_ASSIST && selected_entity.target.id == id)) {
                                is_targeting_only_builders = false;
                                break;
                            }
                        }
                        if (is_targeting_only_builders) {
                            state.selection.clear();
                            state.selection.push_back(entity.target.id);
                        }

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
                        if (entity.type == ENTITY_MINER && (target.type == ENTITY_HALL || target.type == ENTITY_CAMP) && entity.player_id == target.player_id && entity.gold_held != 0 && entity.target.type != TARGET_REPAIR) {
                            state.player_gold[entity.player_id] += entity.gold_held;
                            entity.gold_held = 0;
                            entity.target = entity.remembered_gold_target.type != TARGET_NONE 
                                                ? entity.remembered_gold_target
                                                : entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                            // Clear the remembered gold target, this way if the remembered target doesn't work out, we don't keep returning to it
                            entity.remembered_gold_target = (target_t) { .type = TARGET_NONE };
                            entity.mode = MODE_UNIT_IDLE;
                            update_finished = true;
                            break;
                        }

                        // Garrison
                        if (entity.player_id == target.player_id && entity_data.garrison_size != ENTITY_CANNOT_GARRISON && entity_get_garrisoned_occupancy(state, target) + entity_data.garrison_size <= ENTITY_DATA.at(target.type).garrison_capacity && entity.target.type != TARGET_REPAIR) {
                            target.garrisoned_units.push_back(id);
                            entity.garrison_id = entity.target.id;
                            entity.mode = MODE_UNIT_IDLE;
                            entity.target = (target_t) { .type = TARGET_NONE };
                            map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY);
                            map_fog_update(state, entity.player_id, entity.cell, entity_cell_size(entity.type), ENTITY_DATA.at(entity.type).sight, false, ENTITY_DATA.at(entity.type).has_detection);
                            ui_deselect_entity_if_selected(state, id);
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
                    case TARGET_GOLD: {
                        if (entity_is_target_invalid(state, entity)) {
                            entity.target = entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        if (!entity_has_reached_target(state, entity)) {
                            // This will trigger a repath
                            entity.target = entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        entity_t& target = state.entities.get_by_id(entity.target.id);
                        if (entity.gold_held == UNIT_MAX_GOLD_HELD) {
                            entity.target = entity_target_nearest_camp(state, entity);
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        // The cell should either be empty (meaning we mineral walked to this cell) or filled with the current entity ID (meaning we started at this cell)
                        // If it's not then we should find another gold cell to mine
                        if (!(map_is_cell_rect_equal_to(state, entity.cell, entity_cell_size(entity.type), CELL_EMPTY) || 
                                map_is_cell_rect_equal_to(state, entity.cell, entity_cell_size(entity.type), id))) {
                            entity.target = entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                            entity.mode = MODE_UNIT_IDLE;
                            break;
                        }

                        // Occupy this cell and begin mining
                        map_set_cell_rect(state, entity.cell, entity_cell_size(entity.type), id);
                        entity.timer = UNIT_MINE_TICK_DURATION;
                        entity.remembered_gold_target = entity.target;
                        entity.mode = MODE_UNIT_MINE;
                        entity.direction = enum_direction_to_rect(entity.cell, target.cell, entity_cell_size(target.type));

                        update_finished = true;
                        break;
                    }
                }
                update_finished = update_finished || !(entity.mode == MODE_UNIT_MOVE && movement_left.raw_value > 0);
                break;
            } // End mode move finished
            case MODE_UNIT_BUILD: {
                // This code handles the case where 1. the building is destroyed while the unit is building it
                // and 2. the unit was unable to exit the building and is now stuck inside it
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
                    if (entity.type == ENTITY_CANNON) {
                        entity_t& target = state.entities.get_by_id(entity.target.id);
                        entity_cannon_explode(state, id, target.cell, entity_cell_size(target.type));
                    } else {
                        entity_attack_target(state, id, state.entities.get_by_id(entity.target.id));
                    }
                    entity.timer = ENTITY_DATA.at(entity.type).unit_data.attack_cooldown;
                    entity.mode = MODE_UNIT_ATTACK_COOLDOWN;
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_ATTACK_COOLDOWN: {
                if (entity_is_target_invalid(state, entity)) {
                    entity.timer = 0;
                    entity.target = entity_target_nearest_enemy(state, entity);
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                if (!entity_has_reached_target(state, entity)) {
                    // Repath to target
                    entity.timer = 0;
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    if (entity.type == ENTITY_SOLDIER) {
                        entity_t& target = state.entities.get_by_id(entity.target.id);
                        SDL_Rect entity_rect = (SDL_Rect) { .x = entity.cell.x, .y = entity.cell.y, .w = entity_cell_size(entity.type), .h = entity_cell_size(entity.type) };
                        SDL_Rect target_rect = (SDL_Rect) { .x = target.cell.x, .y = target.cell.y, .w = entity_cell_size(target.type), .h = entity_cell_size(target.type) };
                        if (euclidean_distance_squared_between(entity_rect, target_rect) < ENTITY_DATA.at(entity.type).unit_data.min_range_squared) {
                            if (match_player_has_upgrade(state, entity.player_id, UPGRADE_BAYONETS)) {
                                entity.mode = MODE_UNIT_ATTACK_WINDUP;
                            } else {
                                entity.mode = MODE_UNIT_IDLE;
                            }
                        } else {
                            entity.mode = MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP;
                        }
                    } else {
                        entity.mode = MODE_UNIT_ATTACK_WINDUP;
                    }
                }

                update_finished = true;
                break;
            }
            case MODE_UNIT_MINE: {
                if (entity_is_target_invalid(state, entity)) {
                    entity.target = entity_target_nearest_gold(state, entity.player_id, entity.cell, entity.gold_patch_id);
                    if (entity.target.type == TARGET_NONE && entity.gold_held != 0) {
                        entity.target = entity_target_nearest_camp(state, entity);
                    }
                    entity.mode = MODE_UNIT_IDLE;
                    break;
                }

                entity.timer--;
                if (entity.timer == 0) {
                    entity.gold_held++;
                    entity_t& gold = state.entities.get_by_id(entity.target.id);
                    gold.gold_held--;
                    if (entity.gold_held == UNIT_MAX_GOLD_HELD) {
                        entity.target = entity_target_nearest_camp(state, entity);
                        entity.mode = MODE_UNIT_IDLE;
                        update_finished = true;
                        break;
                    }

                    entity.timer = UNIT_MINE_TICK_DURATION;
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
                            if (entity.timer == 0 && entity.player_id == network_get_player_id()) {
                                ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
                            }
                            entity.timer = BUILDING_QUEUE_EXIT_BLOCKED;
                            update_finished = true;
                            break;
                        } 

                        entity.timer = 0;
                        entity_id unit_id = entity_create(state, entity.queue[0].unit_type, entity.player_id, exit_cell);

                        // Rally unit
                        entity_t& unit = state.entities.get_by_id(unit_id);
                        if (unit.type == ENTITY_MINER && map_get_cell(state, rally_cell) < CELL_EMPTY && state.entities.get_by_id(map_get_cell(state, rally_cell)).type == ENTITY_GOLD) {
                            // Rally to gold
                            unit.target = entity_target_gold(state, entity, map_get_cell(state, rally_cell));
                        } else {
                            // Rally to cell
                            unit.target = (target_t) {
                                .type = TARGET_CELL,
                                .cell = rally_cell
                            };
                        }

                        // Create alert
                        SDL_Rect unit_rect = entity_get_rect(unit);
                        unit_rect.x -= state.camera_offset.x;
                        unit_rect.y -= state.camera_offset.y;
                        if (unit.player_id == network_get_player_id() && SDL_HasIntersection(&SCREEN_RECT, &unit_rect) != SDL_TRUE) {
                            state.alerts.push_back((alert_t) {
                                .color = ALERT_COLOR_GREEN,
                                .cell = unit.cell,
                                .cell_size = entity_cell_size(unit.type),
                                .timer = MATCH_ALERT_TOTAL_DURATION
                            });
                        }

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
                        if (entity.player_id == network_get_player_id()) {
                            char upgrade_status[128];
                            sprintf(upgrade_status, "%s research complete.", UPGRADE_DATA.at(entity.queue[0].upgrade).name);
                            ui_show_status(state, upgrade_status);
                        }

                        // Create alert
                        SDL_Rect building_rect = entity_get_rect(entity);
                        building_rect.x -= state.camera_offset.x;
                        building_rect.y -= state.camera_offset.y;
                        if (entity.player_id == network_get_player_id() && SDL_HasIntersection(&SCREEN_RECT, &building_rect) != SDL_TRUE) {
                            state.alerts.push_back((alert_t) {
                                .color = ALERT_COLOR_GREEN,
                                .cell = entity.cell,
                                .cell_size = entity_cell_size(entity.type),
                                .timer = MATCH_ALERT_TOTAL_DURATION
                            });
                        }

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

    if (entity.taking_damage_timer != 0) {
        entity.taking_damage_timer--;
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
        animation_update(entity.animation);
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
    } else if (entity.type == ENTITY_MINE && entity.mode == MODE_MINE_PRIME) {
        animation_update(entity.animation);
    }
}

bool entity_is_unit(EntityType type) {
    return type < ENTITY_HALL;
}

bool entity_is_building(EntityType type) {
    return type < ENTITY_GOLD;
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
        if (entity.type == ENTITY_MINE) {
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
    if (entity.type == ENTITY_GOLD) {
        return SPRITE_SELECT_RING_GOLD;
    }
    if (entity.type == ENTITY_MINE) {
        return is_ally ? SPRITE_SELECT_RING_MINE : SPRITE_SELECT_RING_MINE_ENEMY;
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
    if (entity.type == ENTITY_GOLD) {
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
    if (!(entity.target.type == TARGET_ENTITY || entity.target.type == TARGET_ATTACK_ENTITY || entity.target.type == TARGET_REPAIR || entity.target.type == TARGET_BUILD_ASSIST || entity.target.type == TARGET_GOLD)) {
        return false;
    }

    uint32_t target_index = state.entities.get_index_of(entity.target.id);
    if (target_index == INDEX_INVALID) {
        return true;
    }

    if (state.entities[target_index].type == ENTITY_GOLD && state.entities[target_index].gold_held == 0) {
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
            if (entity.target.build.building_type == ENTITY_MINE) {
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
        case TARGET_GOLD: {
            return entity.cell == entity.target.cell;
        }
    }
}

xy entity_get_target_cell(const match_state_t& state, const entity_t& entity) {
    switch (entity.target.type) {
        case TARGET_NONE:
            return entity.cell;
        case TARGET_BUILD: {
            if (entity.target.build.building_type == ENTITY_MINE) {
                return map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), entity.target.build.building_cell, entity_cell_size(ENTITY_MINE), false);
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
            return map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), target.cell, entity_cell_size(target.type), entity_should_gold_walk(state, entity));
        }
        case TARGET_GOLD: {
            return entity.target.cell;
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
        case MODE_UNIT_MINE:
            return ANIMATION_UNIT_MINE;
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
        } else if (entity.animation.name == ANIMATION_UNIT_DEATH || entity.animation.name == ANIMATION_UNIT_DEATH_FADE) {
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
        return xy(entity.animation.frame.x, entity.gold_held < GOLD_LOW_THRESHOLD ? 1 : 0);
    }
}

bool entity_should_flip_h(const entity_t& entity) {
    return entity_is_unit(entity.type) && entity.direction > DIRECTION_SOUTH;
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

target_t entity_target_gold(const match_state_t& state, const entity_t& entity, entity_id gold_id) {
    const entity_t& gold = state.entities.get_by_id(gold_id);
    target_t target = (target_t) {
        .type = TARGET_GOLD,
        .id = gold_id,
        .cell = gold.cell
    };
    int nearest_dist = -1;
    for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
        xy gold_cell = gold.cell + DIRECTION_XY[direction];
        if (!map_is_cell_in_bounds(state, gold_cell)) {
            continue;
        }
        if (gold_cell == entity.cell) {
            target.cell = gold_cell;
            break;
        }
        if (nearest_dist == -1 || 
                (map_get_cell(state, gold_cell) == CELL_EMPTY && map_get_cell(state, target.cell) != CELL_EMPTY) || 
                xy::manhattan_distance(entity.cell, gold_cell) < nearest_dist) {
            target.cell = gold_cell;
            nearest_dist = xy::manhattan_distance(entity.cell, gold_cell);
        }
    }

    return target;
}

target_t entity_target_nearest_enemy(const match_state_t& state, const entity_t& entity) {
    SDL_Rect entity_rect = (SDL_Rect) { .x = entity.cell.x, .y = entity.cell.y, .w = entity_cell_size(entity.type),. h = entity_cell_size(entity.type) };
    SDL_Rect entity_sight_rect = entity_get_sight_rect(entity);
    uint32_t nearest_enemy_index = INDEX_INVALID;
    int nearest_enemy_dist = -1;
    uint32_t nearest_attack_priority;

    for (uint32_t other_index = 0; other_index < state.entities.size(); other_index++) {
        const entity_t& other = state.entities[other_index];

        if (other.type == ENTITY_GOLD) {
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

target_t entity_target_nearest_gold(const match_state_t& state, uint8_t entity_player_id, xy start_cell, uint32_t gold_patch_id) {
    uint32_t nearest_index = INDEX_INVALID;
    xy nearest_cell = xy(-1, -1);
    int nearest_dist = -1;

    for (uint32_t gold_index = 0; gold_index < state.entities.size(); gold_index++) {
        const entity_t& gold = state.entities[gold_index];

        if (gold.type != ENTITY_GOLD || gold.gold_held == 0 ||
                (gold_patch_id != GOLD_PATCH_ID_NULL && gold_patch_id != gold.gold_patch_id) ||
                (state.map_fog[entity_player_id][gold.cell.x + (gold.cell.y * state.map_width)] == FOG_HIDDEN)) {
            continue;
        }

        for (int direction = 0; direction < DIRECTION_COUNT; direction += 2) {
            xy gold_neighbor_cell = gold.cell + DIRECTION_XY[direction];
            if (!map_is_cell_in_bounds(state, gold_neighbor_cell)) {
                continue;
            }
            if (nearest_dist == -1) {
                nearest_index = gold_index;
                nearest_dist = xy::manhattan_distance(start_cell, gold_neighbor_cell);
                nearest_cell = gold_neighbor_cell;
                continue;
            }
            // Don't choose a blocked cell if we've already found an empty cell
            if (map_get_cell(state, gold_neighbor_cell) != CELL_EMPTY && map_get_cell(state, nearest_cell) == CELL_EMPTY) {
                continue;
            }
            if ((map_get_cell(state, gold_neighbor_cell) == CELL_EMPTY && map_get_cell(state, nearest_cell) != CELL_EMPTY) ||
                    xy::manhattan_distance(start_cell, gold_neighbor_cell) < nearest_dist) {
                nearest_index = gold_index;
                nearest_dist = xy::manhattan_distance(start_cell, gold_neighbor_cell);
                nearest_cell = gold_neighbor_cell;
            }
        }
    }

    if (nearest_index == INDEX_INVALID) {
        return (target_t) {
            .type = TARGET_NONE
        };
    }

    return (target_t) {
        .type = TARGET_GOLD,
        .id = state.entities.get_id_of(nearest_index),
        .cell = nearest_cell
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

bool entity_should_gold_walk(const match_state_t& state, const entity_t& entity) {
    if (!(entity.target.type == TARGET_ENTITY || entity.target.type == TARGET_GOLD) || entity.type != ENTITY_MINER) {
        return false;
    }
    uint32_t target_index = state.entities.get_index_of(entity.target.id);
    if (target_index == INDEX_INVALID) {
        return false;
    }

    return state.entities[target_index].type == ENTITY_GOLD ||
           ((state.entities[target_index].type == ENTITY_CAMP || state.entities[target_index].type == ENTITY_HALL) && entity.gold_held != 0);
}

SDL_Rect entity_gold_get_block_building_rect(xy cell) {
    return (SDL_Rect) { .x = cell.x - 3, .y = cell.y - 3, .w = 7, .h = 7 };
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
    entity.remembered_gold_target = (target_t) { .type = TARGET_NONE };
    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, false);

    if (entity.mode != MODE_UNIT_MOVE) {
        // Abandon current behavior in favor of new order
        entity.timer = 0;
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
        defender.health = std::max(0, defender.health - damage);

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
}

void entity_on_attack(match_state_t& state, entity_id attacker_id, entity_t& defender) {
    entity_t& attacker = state.entities.get_by_id(attacker_id);

    // Alerts / Taking damage flicker
    if (attacker.player_id != defender.player_id && defender.player_id == network_get_player_id()) {
        SDL_Rect defender_rect = entity_get_rect(defender);
        defender_rect.x -= state.camera_offset.x;
        defender_rect.y -= state.camera_offset.y;
        if (defender.taking_damage_timer != 0 && SDL_HasIntersection(&SCREEN_RECT, &defender_rect) != SDL_TRUE) {
            bool is_existing_attack_alert_nearby = false;
            for (const alert_t& existing_alert : state.alerts) {
                if (existing_alert.color == ALERT_COLOR_RED && xy::manhattan_distance(existing_alert.cell, defender.cell) < MATCH_ATTACK_ALERT_DISTANCE) {
                    is_existing_attack_alert_nearby = true;
                    break;
                }
            }

            if (!is_existing_attack_alert_nearby) {
                state.alerts.push_back((alert_t) {
                    .color = ALERT_COLOR_RED,
                    .cell = defender.cell,
                    .cell_size = entity_cell_size(defender.type),
                    .timer = MATCH_ALERT_TOTAL_DURATION
                });
                ui_show_status(state, UI_STATUS_UNDER_ATTACK);
            }
        }
        defender.taking_damage_timer = MATCH_TAKING_DAMAGE_TIMER_DURATION;
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
        if (defender.type == ENTITY_GOLD || !entity_is_selectable(defender)) {
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
    ui_deselect_entity_if_selected(state, id);
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

    // Create particle
    state.particles.push_back((particle_t) {
        .sprite = SPRITE_PARTICLE_EXPLOSION,
        .animation = animation_create(ANIMATION_PARTICLE_EXPLOSION),
        .vframe = 0,
        .position = entity.type == ENTITY_SAPPER ? entity.position.to_xy() : cell_center(entity.cell).to_xy()
    });
}

void entity_cannon_explode(match_state_t& state, entity_id attacker_id, xy cell, int cell_size) {
    entity_t& attacker = state.entities.get_by_id(attacker_id);

    // Calculate accuracy
    bool attack_missed = false;
    int accuracy = 100;
    if (entity_get_elevation(state, attacker) < state.map_tiles[cell.x + (cell.y * state.map_width)].elevation) {
        accuracy /= 2;
    }
    if (accuracy < lcg_rand() % 100) {
        attack_missed = true;
    }

    if (attack_missed) {
        return;
    }

    // Check which enemies we hit. 
    int attacker_damage = ENTITY_DATA.at(attacker.type).unit_data.damage;
    xy position = (cell * TILE_SIZE) + ((xy(cell_size, cell_size) * TILE_SIZE) / 2);
    SDL_Rect full_damage_rect = (SDL_Rect) {
        .x = position.x - (TILE_SIZE / 2), .y = position.y - (TILE_SIZE / 2),
        .w = TILE_SIZE, .h = TILE_SIZE
    };
    SDL_Rect splash_damage_rect = (SDL_Rect) {
        .x = full_damage_rect.x - (TILE_SIZE / 2), .y = full_damage_rect.y - (TILE_SIZE / 2),
        .w = full_damage_rect.w + TILE_SIZE, .h = full_damage_rect.h + TILE_SIZE
    };
    for (entity_t& defender : state.entities) {
        if (defender.type == ENTITY_GOLD || !entity_is_selectable(defender)) {
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

    // Create particle
    state.particles.push_back((particle_t) {
        .sprite = SPRITE_PARTICLE_CANNON_EXPLOSION,
        .animation = animation_create(ANIMATION_PARTICLE_CANNON_EXPLOSION),
        .vframe = 0,
        .position = position
    });
}

xy entity_get_exit_cell(const match_state_t& state, xy building_cell, int building_size, int unit_size, xy rally_cell) {
    xy exit_cell = xy(-1, -1);
    int exit_cell_dist = -1;
    for (int x = building_cell.x - unit_size; x < building_cell.x + building_size + unit_size; x++) {
        xy cell = xy(x, building_cell.y - unit_size);
        int cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = xy(x, building_cell.y + building_size);
        cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
    }
    for (int y = building_cell.y - unit_size; y < building_cell.y + building_size + unit_size; y++) {
        xy cell = xy(building_cell.x - unit_size, y);
        int cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
           (exit_cell_dist == -1 || cell_dist < exit_cell_dist)) {
            exit_cell = cell;
            exit_cell_dist = cell_dist;
        }
        cell = xy(building_cell.x + building_size, y);
        cell_dist = xy::manhattan_distance(cell, rally_cell);
        if (map_is_cell_rect_in_bounds(state, cell, unit_size) && 
           !map_is_cell_rect_occupied(state, cell, unit_size) && 
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
                if (garrisoned_unit.player_id == network_get_player_id() && entity_is_building(entity.type)) {
                    ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
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
        } else {
            index++;
        }
    }
}

void entity_stop_building(match_state_t& state, entity_id id) {
    entity_t& entity = state.entities.get_by_id(id);

    xy exit_cell = map_get_nearest_cell_around_rect(state, entity.cell, entity_cell_size(entity.type), entity.target.build.building_cell, entity_cell_size(entity.target.build.building_type), false);
    if (exit_cell == entity.cell) {
        // Unable to exit the building
        exit_cell = xy(-1, -1);
        for (int x = entity.target.build.building_cell.x; x < entity.target.build.building_cell.x + entity_cell_size(entity.target.build.building_type); x++) {
            for (int y = entity.target.build.building_cell.y; x < entity.target.build.building_cell.y + entity_cell_size(entity.target.build.building_type); y++) {
                if (!map_is_cell_rect_occupied(state, xy(x, y), entity_cell_size(entity.type))) {
                    exit_cell = xy(x, y);
                    break;
                }
            }
            if (exit_cell.x != -1) {
                break;
            }
        }
        if (exit_cell.x == -1) {
            return;
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
    if (building.player_id == network_get_player_id()) {
        SDL_Rect building_rect = entity_get_rect(building);
        building_rect.x -= state.camera_offset.x;
        building_rect.y -= state.camera_offset.y;
        if (SDL_HasIntersection(&SCREEN_RECT, &building_rect) != SDL_TRUE) {
            state.alerts.push_back((alert_t) {
                .color = ALERT_COLOR_GREEN,
                .cell = building.cell,
                .cell_size = entity_cell_size(building.type),
                .timer = MATCH_ALERT_TOTAL_DURATION
            });
        }
    }

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
            if (entity.mode != MODE_UNIT_IDLE && entity.player_id == network_get_player_id()) {
                ui_show_status(state, UI_STATUS_BUILDING_EXIT_BLOCKED);
            }
        } else if (entity.mode == MODE_UNIT_REPAIR) {
            entity.mode = MODE_UNIT_IDLE;
        }

        entity.target = building.type == ENTITY_CAMP || building.type == ENTITY_HALL
                            ? entity_target_nearest_gold(state, entity.player_id, entity.cell, GOLD_PATCH_ID_NULL)
                            : (target_t) { .type = TARGET_NONE };
    }
}

void entity_building_enqueue(match_state_t& state, entity_t& building, building_queue_item_t item) {
    GOLD_ASSERT(building.queue.size() < BUILDING_QUEUE_MAX);
    building.queue.push_back(item);
    if (building.queue.size() == 1) {
        if (entity_building_is_supply_blocked(state, building)) {
            if (building.player_id == network_get_player_id() && building.timer != BUILDING_QUEUE_BLOCKED) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
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
            if (building.player_id == network_get_player_id() && building.timer != BUILDING_QUEUE_BLOCKED) {
                ui_show_status(state, UI_STATUS_NOT_ENOUGH_HOUSE);
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