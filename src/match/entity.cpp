#include "entity.h"

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
        .has_detection = false,

        .goldmine_data = {}
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),
            .max_energy = 0,

            .attack_sound = SOUND_GUN,
            .damage = 8,
            .accuracy = 80,
            .evasion = 10,
            .attack_cooldown = 40,
            .range_squared = 16,
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 225),
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

        .unit_data {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 20),
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
        .max_health = 60,
        .sight = 11,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = ENTITY_CANNOT_GARRISON,
        .has_detection = false,

        .unit_data {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(1, 40),
            .max_energy = 0,

            .attack_sound = SOUND_GUN,
            .damage = 10,
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
        
        .gold_cost = 100,
        .train_duration = 25,
        .max_health = 40,
        .sight = 7,
        .armor = 0,
        .attack_priority = 2,

        .garrison_capacity = 0,
        .garrison_size = 1,
        .has_detection = false,

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 225),
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 200),
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 170),
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

        .unit_data {
            .population_cost = 2,
            .speed = fixed::from_int_and_raw_decimal(0, 140),
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 170),
            .max_energy = 80,

            .attack_sound = SOUND_PISTOL_SILENCED,
            .damage = 10,
            .accuracy = 80,
            .evasion = 9,
            .attack_cooldown = 45,
            .range_squared = 25,
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

        .unit_data {
            .population_cost = 1,
            .speed = fixed::from_int_and_raw_decimal(0, 80),
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

        .building_data {
            .builder_positions_x = { 16, 23, 7 },
            .builder_positions_y = { 43, 25, 22 },
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

        .building_data {
            .builder_positions_x = { 3, 16, -4 },
            .builder_positions_y = { 15, 15, 3 },
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

        .building_data {
            .builder_positions_x = { 6, 27, 9 },
            .builder_positions_y = { 32, 27, 9 },
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

        .building_data {
            .builder_positions_x = { 1, 14, 5 },
            .builder_positions_y = { 15, 9, -3 },
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

        .building_data {
            .builder_positions_x = { 3, 16, 6 },
            .builder_positions_y = { 29, 13, 7 },
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

        .building_data {
            .builder_positions_x = { 9, 27, 26 },
            .builder_positions_y = { 24, 18, 4 },
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

        .building_data {
            .builder_positions_x = { 10, 28, 28 },
            .builder_positions_y = { 29, 17, 4 },
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

        .building_data {
            .builder_positions_x = { 6, 27, 3 },
            .builder_positions_y = { 27, 7, 3 },
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

        .building_data {
            .builder_positions_x = { 6, 27, 14 },
            .builder_positions_y = { 27, 7, 1 },
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

        .building_data {
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