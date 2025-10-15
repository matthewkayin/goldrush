#include "entity.h"

#include "core/asserts.h"
#include "upgrade.h"
#include <unordered_map>

static const std::unordered_map<EntityType, EntityData> ENTITY_DATA = {
    { ENTITY_GOLDMINE, (EntityData) {
        .name = "Gold Mine",
        .sprite = SPRITE_GOLDMINE,
        .icon = SPRITE_BUTTON_ICON_GOLDMINE,
        .death_sound = SOUND_BUNKER_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
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
    }},
    { ENTITY_MINER, (EntityData) {
        .name = "Miner",
        .sprite = SPRITE_UNIT_MINER,
        .icon = SPRITE_BUTTON_ICON_MINER,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 50,
        .train_duration = 20,
        .max_health = 25,
        .sight = 9,
        .armor = 0,
        .attack_priority = 1,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 145),
            .max_energy = 0,

            .attack_sound = SOUND_PICKAXE,
            .damage = 3,
            .accuracy = 100,
            .evasion = 10,
            .attack_cooldown = 22,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_COWBOY, (EntityData) {
        .name = "Cowboy",
        .sprite = SPRITE_UNIT_COWBOY,
        .icon = SPRITE_BUTTON_ICON_COWBOY,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 100,
        .train_duration = 25,
        .max_health = 30,
        .sight = 9,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 145),
            .max_energy = 0,

            .attack_sound = SOUND_GUN,
            .damage = 8,
            .accuracy = 80,
            .evasion = 10,
            .attack_cooldown = 40,
            .range_squared = 25,
            .min_range_squared = 1
        }
    }},
    { ENTITY_BANDIT, (EntityData) {
        .name = "Bandit",
        .sprite = SPRITE_UNIT_BANDIT,
        .icon = SPRITE_BUTTON_ICON_BANDIT,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 75,
        .train_duration = 20,
        .max_health = 40,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 195),
            .max_energy = 0,

            .attack_sound = SOUND_SWORD,
            .damage = 5,
            .accuracy = 100,
            .evasion = 13,
            .attack_cooldown = 15,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_WAGON, (EntityData) {
        .name = "Wagon",
        .sprite = SPRITE_UNIT_WAGON,
        .icon = SPRITE_BUTTON_ICON_WAGON,
        .death_sound = SOUND_DEATH_CHICKEN,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 2,
        
        .gold_cost = 200,
        .train_duration = 38,
        .max_health = 120,
        .sight = 11,
        .armor = 1,
        .attack_priority = 1,

        .garrison_capacity = 4,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(2, 40),
            .max_energy = 0,

            .attack_sound = SOUND_PICKAXE,
            .damage = 0,
            .accuracy = 0,
            .evasion = 12,
            .attack_cooldown = 0,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_JOCKEY, (EntityData) {
        .name = "Jockey",
        .sprite = SPRITE_UNIT_JOCKEY,
        .icon = SPRITE_BUTTON_ICON_JOCKEY,
        .death_sound = SOUND_DEATH_CHICKEN,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 150,
        .train_duration = 30,
        .max_health = 80,
        .sight = 11,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(2, 80),
            .max_energy = 0,

            .attack_sound = SOUND_GUN,
            .damage = 8,
            .accuracy = 75,
            .evasion = 15,
            .attack_cooldown = 45,
            .range_squared = 36,
            .min_range_squared = 1
        }
    }},
    { ENTITY_SAPPER, (EntityData) {
        .name = "Sapper",
        .sprite = SPRITE_UNIT_SAPPER,
        .icon = SPRITE_BUTTON_ICON_SAPPER,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 125,
        .train_duration = 27,
        .max_health = 40,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 195),
            .max_energy = 0,

            .attack_sound = SOUND_PICKAXE,
            .damage = 200,
            .accuracy = 0,
            .evasion = 13,
            .attack_cooldown = 15,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_PYRO, (EntityData) {
        .name = "Pyro",
        .sprite = SPRITE_UNIT_PYRO,
        .icon = SPRITE_BUTTON_ICON_PYRO,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 225,
        .train_duration = 30,
        .max_health = 40,
        .sight = 11,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = true,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 145),
            .max_energy = 120,

            .attack_sound = SOUND_PICKAXE,
            .damage = 0,
            .accuracy = 0,
            .evasion = 10,
            .attack_cooldown = 15,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_SOLDIER, (EntityData) {
        .name = "Soldier",
        .sprite = SPRITE_UNIT_SOLDIER,
        .icon = SPRITE_BUTTON_ICON_SOLDIER,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 175,
        .train_duration = 30,
        .max_health = 40,
        .sight = 9,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 85),
            .max_energy = 0,

            .attack_sound = SOUND_MUSKET,
            .damage = 15,
            .accuracy = 90,
            .evasion = 8,
            .attack_cooldown = 30,
            .range_squared = 49,
            .min_range_squared = 4
        }
    }},
    { ENTITY_CANNON, (EntityData) {
        .name = "Cannoneer",
        .sprite = SPRITE_UNIT_CANNON,
        .icon = SPRITE_BUTTON_ICON_CANNON,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 2,
        
        .gold_cost = 250,
        .train_duration = 45,
        .max_health = 100,
        .sight = 9,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 25),
            .max_energy = 0,

            .attack_sound = SOUND_CANNON,
            .damage = 20,
            .accuracy = 70,
            .evasion = 6,
            .attack_cooldown = 60,
            .range_squared = 49,
            .min_range_squared = 9 
        }
    }},
    { ENTITY_DETECTIVE, (EntityData) {
        .name = "Detective",
        .sprite = SPRITE_UNIT_DETECTIVE,
        .icon = SPRITE_BUTTON_ICON_DETECTIVE,
        .death_sound = SOUND_DEATH,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 1,
        
        .gold_cost = 225,
        .train_duration = 30,
        .max_health = 25,
        .sight = 11,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(1, 85),
            .max_energy = 80,

            .attack_sound = SOUND_PISTOL_SILENCED,
            .damage = 10,
            .accuracy = 80,
            .evasion = 9,
            .attack_cooldown = 45,
            .range_squared = 24,
            .min_range_squared = 1
        }
    }},
    { ENTITY_BALLOON, (EntityData) {
        .name = "Balloon",
        .sprite = SPRITE_UNIT_BALLOON,
        .icon = SPRITE_BUTTON_ICON_BALLOON,
        .death_sound = SOUND_BALLOON_DEATH,
        .cell_layer = CELL_LAYER_SKY,
        .cell_size = 1,
        
        .gold_cost = 200,
        .train_duration = 30,
        .max_health = 120,
        .sight = 15,
        .armor = 0,
        .attack_priority = 1,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = true,

        .unit_data = (EntityDataUnit) {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 160),
            .max_energy = 0,

            .attack_sound = SOUND_PISTOL_SILENCED,
            .damage = 0,
            .accuracy = 0,
            .evasion = 8,
            .attack_cooldown = 45,
            .range_squared = 1,
            .min_range_squared = 1
        }
    }},
    { ENTITY_HALL, (EntityData) {
        .name = "Town Hall",
        .sprite = SPRITE_BUILDING_HALL,
        .icon = SPRITE_BUTTON_ICON_HALL,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 4,
        
        .gold_cost = 400,
        .train_duration = 0,
        .max_health = 840,
        .sight = 11,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 32, 46, 14 },
            .builder_positions_y = { 86, 50, 44 },
            .builder_flip_h = { false, true, false },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_HOUSE, (EntityData) {
        .name = "House",
        .sprite = SPRITE_BUILDING_HOUSE,
        .icon = SPRITE_BUTTON_ICON_HOUSE,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 2,
        
        .gold_cost = 100,
        .train_duration = 0,
        .max_health = 300,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 6, 32, -8 },
            .builder_positions_y = { 30, 30, 6 },
            .builder_flip_h = { false, true, false },
            .options = 0
        }
    }},
    { ENTITY_SALOON, (EntityData) {
        .name = "Saloon",
        .sprite = SPRITE_BUILDING_SALOON,
        .icon = SPRITE_BUTTON_ICON_SALOON,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 3,
        
        .gold_cost = 150,
        .train_duration = 0,
        .max_health = 600,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 12, 54, 18 },
            .builder_positions_y = { 64, 54, 18 },
            .builder_flip_h = { false, true, false },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_BUNKER, (EntityData) {
        .name = "Bunker",
        .sprite = SPRITE_BUILDING_BUNKER,
        .icon = SPRITE_BUTTON_ICON_BUNKER,
        .death_sound = SOUND_BUNKER_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 2,
        
        .gold_cost = 100,
        .train_duration = 0,
        .max_health = 200,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 4,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 2, 28, 10 },
            .builder_positions_y = { 30, 18, -6 },
            .builder_flip_h = { false, true, false },
            .options = 0
        }
    }},
    { ENTITY_WORKSHOP, (EntityData) {
        .name = "Workshop",
        .sprite = SPRITE_BUILDING_WORKSHOP,
        .icon = SPRITE_BUTTON_ICON_WORKSHOP,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 3,
        
        .gold_cost = 300,
        .train_duration = 0,
        .max_health = 600,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 6, 32, 12 },
            .builder_positions_y = { 58, 26, 14 },
            .builder_flip_h = { false, true, false },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_COOP, (EntityData) {
        .name = "Chicken Coop",
        .sprite = SPRITE_BUILDING_COOP,
        .icon = SPRITE_BUTTON_ICON_COOP,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 3,
        
        .gold_cost = 200,
        .train_duration = 0,
        .max_health = 500,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 18, 54, 52 },
            .builder_positions_y = { 48, 36, 8 },
            .builder_flip_h = { false, true, true },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_SMITH, (EntityData) {
        .name = "Blacksmith",
        .sprite = SPRITE_BUILDING_SMITH,
        .icon = SPRITE_BUTTON_ICON_SMITH,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 3,
        
        .gold_cost = 250,
        .train_duration = 0,
        .max_health = 560,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 20, 56, 56 },
            .builder_positions_y = { 58, 34, 8 },
            .builder_flip_h = { false, true, true },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_BARRACKS, (EntityData) {
        .name = "Barracks",
        .sprite = SPRITE_BUILDING_BARRACKS,
        .icon = SPRITE_BUTTON_ICON_BARRACKS,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 3,
        
        .gold_cost = 300,
        .train_duration = 0,
        .max_health = 600,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 12, 54, 6 },
            .builder_positions_y = { 54, 14, 6 },
            .builder_flip_h = { false, true, false },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_SHERIFFS, (EntityData) {
        .name = "Sheriff's Office",
        .sprite = SPRITE_BUILDING_SHERIFFS,
        .icon = SPRITE_BUTTON_ICON_SHERIFFS,
        .death_sound = SOUND_BUILDING_DESTROY,
        .cell_layer = CELL_LAYER_GROUND,
        .cell_size = 3,
        
        .gold_cost = 250,
        .train_duration = 0,
        .max_health = 560,
        .sight = 9,
        .armor = 1,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 12, 54, 28 },
            .builder_positions_y = { 54, 14, 2 },
            .builder_flip_h = { false, true, false },
            .options = BUILDING_CAN_RALLY
        }
    }},
    { ENTITY_LANDMINE, (EntityData) {
        .name = "Land Mine",
        .sprite = SPRITE_BUILDING_LANDMINE,
        .icon = SPRITE_BUTTON_ICON_LANDMINE,
        .death_sound = SOUND_MINE_DESTROY,
        .cell_layer = CELL_LAYER_UNDERGROUND,
        .cell_size = 1,
        
        .gold_cost = 20,
        .train_duration = 0,
        .max_health = 5,
        .sight = 3,
        .armor = 0,
        .attack_priority = 0,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .building_data = (EntityDataBuilding) {
            .builder_positions_x = { 6, 27, 14 },
            .builder_positions_y = { 27, 7, 1 },
            .builder_flip_h = { false, true, false },
            .options = BUILDING_COSTS_ENERGY
        }
    }}
};

const EntityData& entity_get_data(EntityType type) {
    return ENTITY_DATA.at(type);
}

bool entity_is_unit(EntityType type) {
    return type > ENTITY_GOLDMINE && type < ENTITY_HALL;
}

bool entity_is_building(EntityType type) {
    return type >= ENTITY_HALL && type != ENTITY_TYPE_COUNT;
}

bool entity_is_selectable(const Entity& entity) {
    if (entity.type == ENTITY_GOLDMINE) {
        return true;
    }

    return !(
        entity.health == 0 ||
        entity.mode == MODE_UNIT_BUILD ||
        entity.garrison_id != ID_NULL
    );
}

uint32_t entity_get_elevation(const Entity& entity, const Map& map) {
    uint32_t elevation = map_get_tile(map, entity.cell).elevation;
    int entity_cell_size = entity_get_data(entity.type).cell_size;
    for (int y = entity.cell.y; y < entity.cell.y + entity_cell_size; y++) {
        for (int x = entity.cell.x; x < entity.cell.x + entity_cell_size; x++) {
            elevation = std::max(elevation, map_get_tile(map, ivec2(x, y)).elevation);
        }
    }

    if (entity.mode == MODE_UNIT_MOVE) {
        ivec2 unit_prev_cell = entity.cell - DIRECTION_IVEC2[entity.direction];
        for (int y = unit_prev_cell.y; y < unit_prev_cell.y + entity_cell_size; y++) {
            for (int x = unit_prev_cell.x; x < unit_prev_cell.x + entity_cell_size; x++) {
                elevation = std::max(elevation, map_get_tile(map, ivec2(x, y)).elevation);
            }
        }
    }
    
    return elevation;
}

Rect entity_get_rect(const Entity& entity) {
    const EntityData& entity_data = entity_get_data(entity.type);
    Rect rect = (Rect) {
        .x = entity.position.x.integer_part(),
        .y = entity.position.y.integer_part(),
        .w = entity_data.cell_size * TILE_SIZE,
        .h = entity_data.cell_size * TILE_SIZE
    };
    if (entity_is_unit(entity.type)) {
        rect.x -= rect.w / 2;
        rect.y -= rect.h / 2;
    }
    if (entity.type == ENTITY_BALLOON) {
        rect.y -= 32;
        rect.h += 16;
    }

    return rect;
}

fvec2 entity_get_target_position(const Entity& entity) {
    int unit_size = entity_get_data(entity.type).cell_size * TILE_SIZE;
    return fvec2((entity.cell * TILE_SIZE) + ivec2(unit_size / 2, unit_size / 2));
}

void entity_set_target(Entity& entity, Target target) {
    GOLD_ASSERT(entity.mode != MODE_UNIT_BUILD);
    entity.target = target;
    entity.path.clear();
    entity.goldmine_id = ID_NULL;
    entity.attack_move_cell = target.type == TARGET_ATTACK_CELL ? target.cell : ivec2(-1, -1);
    entity_set_flag(entity, ENTITY_FLAG_HOLD_POSITION, false);
    entity_set_flag(entity, ENTITY_FLAG_ATTACK_SPECIFIC_ENTITY, target.type == TARGET_ATTACK_ENTITY);

    if (entity.mode != MODE_UNIT_MOVE) {
        // Abandon current behavior in favor of new order
        entity.timer = 0;
        entity.pathfind_attempts = 0;
        entity.mode = MODE_UNIT_IDLE;
    }
}

bool entity_check_flag(const Entity& entity, uint32_t flag) {
    return (entity.flags & flag) == flag;
}

void entity_set_flag(Entity& entity, uint32_t flag, bool value) {
    if (value) {
        entity.flags |= flag;
    } else {
        entity.flags &= ~flag;
    }
}

AnimationName entity_get_expected_animation(const Entity& entity) {
    if (entity.type == ENTITY_BALLOON) {
        switch (entity.mode) {
            case MODE_UNIT_MOVE:
                return ANIMATION_BALLOON_MOVE;
            case MODE_UNIT_BALLOON_DEATH_START:
                return ANIMATION_BALLOON_DEATH_START;
            case MODE_UNIT_BALLOON_DEATH:
                return ANIMATION_BALLOON_DEATH;
            default: 
                return ANIMATION_UNIT_IDLE;
        }
    } else if (entity.type == ENTITY_CANNON) {
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
    } else if (entity.type == ENTITY_SMITH) {
        if (entity.queue.empty() && entity.animation.name != ANIMATION_UNIT_IDLE && entity.animation.name != ANIMATION_SMITH_END) {
            return ANIMATION_SMITH_END;
        } else if (entity.animation.name == ANIMATION_SMITH_END && !animation_is_playing(entity.animation)) {
            return ANIMATION_UNIT_IDLE;
        } else if (!entity.queue.empty() && entity.animation.name == ANIMATION_UNIT_IDLE) {
            return ANIMATION_SMITH_BEGIN;
        } else if (entity.animation.name == ANIMATION_SMITH_BEGIN && !animation_is_playing(entity.animation)) {
            return ANIMATION_SMITH_LOOP;
        } else {
            return entity.animation.name;
        }
    }

    switch (entity.mode) {
        case MODE_UNIT_MOVE:
            return ANIMATION_UNIT_MOVE;
        case MODE_UNIT_BUILD:
        case MODE_UNIT_BUILD_ASSIST:
        case MODE_UNIT_REPAIR:
            return ANIMATION_UNIT_BUILD;
        case MODE_UNIT_ATTACK_WINDUP:
        case MODE_UNIT_PYRO_THROW:
            return ANIMATION_UNIT_ATTACK;
        case MODE_UNIT_SOLDIER_RANGED_ATTACK_WINDUP:
            return ANIMATION_SOLDIER_RANGED_ATTACK;
        case MODE_UNIT_SOLDIER_CHARGE:
            return ANIMATION_SOLDIER_CHARGE;
        case MODE_UNIT_DEATH:
            return ANIMATION_UNIT_DEATH;
        case MODE_UNIT_DEATH_FADE:
            return ANIMATION_UNIT_DEATH_FADE;
        case MODE_MINE_PRIME:
            return ANIMATION_MINE_PRIME;
        case MODE_BUILDING_FINISHED: {
            if (entity.type == ENTITY_WORKSHOP && !entity.queue.empty() && !(entity.timer == BUILDING_QUEUE_BLOCKED || entity.timer == BUILDING_QUEUE_EXIT_BLOCKED)) {
                return ANIMATION_WORKSHOP;
            } else {
                return ANIMATION_UNIT_IDLE;
            }
        }
        default:
            return ANIMATION_UNIT_IDLE;
    }
}

ivec2 entity_get_animation_frame(const Entity& entity) {
    if (entity_is_unit(entity.type)) {
        ivec2 frame = entity.animation.frame;

        if (entity.type == ENTITY_BALLOON && entity.mode == MODE_UNIT_DEATH_FADE) {
            // A bit hacky, this will just be an empty frame
            return ivec2(2, 2);
        }

        if (entity.mode == MODE_UNIT_BUILD) {
            frame.y = 2;
        } else if (entity.animation.name == ANIMATION_UNIT_DEATH || entity.animation.name == ANIMATION_UNIT_DEATH_FADE || 
                        entity.animation.name == ANIMATION_CANNON_DEATH || entity.animation.name == ANIMATION_CANNON_DEATH_FADE || 
                        entity.animation.name == ANIMATION_BALLOON_DEATH_START || entity.animation.name == ANIMATION_BALLOON_DEATH) {
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
            return ivec2(0, 0);
        }
        if (entity.mode == MODE_BUILDING_IN_PROGRESS) {
            int max_health = ENTITY_DATA.at(entity.type).max_health;
            return ivec2((3 * (max_health - entity.timer)) / max_health, 0);
        } 
        if (entity.mode == MODE_MINE_PRIME) {
            return entity.animation.frame;
        }
        if (entity.animation.name != ANIMATION_UNIT_IDLE && entity.type != ENTITY_SMITH) {
            return entity.animation.frame;
        }
        // Building finished frame
        return ivec2(3, 0);
    } else {
        // Gold
        if (entity.mode == MODE_GOLDMINE_COLLAPSED) {
            return ivec2(2, 0);
        }
        if (!entity.garrisoned_units.empty()) {
            return ivec2(1, 0);
        }
        return ivec2(0, 0);
    }
}

Rect entity_goldmine_get_block_building_rect(ivec2 cell) {
    return (Rect) { .x = cell.x - 4, .y = cell.y - 4, .w = 11, .h = 11 };
}

bool entity_should_die(const Entity& entity) {
    if (entity.health != 0) {
        return false;
    }

    if (entity_is_unit(entity.type)) {
        if (entity.mode == MODE_UNIT_DEATH || entity.mode == MODE_UNIT_DEATH_FADE || entity.mode == MODE_UNIT_BALLOON_DEATH_START || entity.mode == MODE_UNIT_BALLOON_DEATH) {
            return false;
        }
        if (entity.garrison_id != ID_NULL) {
            return false;
        }

        return true;
    } else if (entity_is_building(entity.type)) {
        return entity.mode != MODE_BUILDING_DESTROYED;
    }

    return false;
}

void entity_on_damage_taken(Entity& entity) {
    entity.taking_damage_counter = 3;
    if (entity.taking_damage_timer == 0) {
        entity.taking_damage_timer = UNIT_TAKING_DAMAGE_FLICKER_DURATION;
    }

    // Health regen timer
    if (entity_is_unit(entity.type)) {
        entity.health_regen_timer = UNIT_HEALTH_REGEN_DURATION + UNIT_HEALTH_REGEN_DELAY;
    }
}

bool entity_is_target_within_min_range(const Entity& entity, const Entity& target) {
    const EntityData& entity_data = entity_get_data(entity.type);
    const EntityData& target_data = entity_get_data(target.type);

    // Min range is ignored when the target is in the sky
    // Min range is also ignored when the attacking entity is garrisoned
    if (entity.garrison_id != ID_NULL || 
            target_data.cell_layer == CELL_LAYER_SKY) {
        return false;
    }

    Rect entity_rect = (Rect) {
        .x = entity.cell.x, .y = entity.cell.y,
        .w = entity_data.cell_size, .h = entity_data.cell_size
    };
    Rect target_rect = (Rect) {
        .x = target.cell.x, .y = target.cell.y,
        .w = target_data.cell_size, .h = target_data.cell_size
    };

    return Rect::euclidean_distance_squared_between(entity_rect, target_rect) < entity_data.unit_data.min_range_squared;
}

// Building queue items

uint32_t building_queue_item_duration(const BuildingQueueItem& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).train_duration * 60;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return upgrade_get_data(item.upgrade).research_duration * 60;
        }
    }
}

uint32_t building_queue_item_cost(const BuildingQueueItem& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).gold_cost;
        }
        case BUILDING_QUEUE_ITEM_UPGRADE: {
            return upgrade_get_data(item.upgrade).gold_cost;
        }
    }
}

uint32_t building_queue_population_cost(const BuildingQueueItem& item) {
    switch (item.type) {
        case BUILDING_QUEUE_ITEM_UNIT: {
            return ENTITY_DATA.at(item.unit_type).unit_data.population_cost;
        }
        default:
            return 0;
    }
}