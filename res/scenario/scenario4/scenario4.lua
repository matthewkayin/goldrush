local objectives = require("objectives")
local actions = require("actions")
local squad_util = require("squad_util")
local entity_util = require("entity_util")

local OBJECTIVE_DEFEND_POSITION = "Defend your Position"

local COUNTDOWN_DURATION = 20 * 60

local ENEMY1 = 1
local ENEMY2 = 2

local SPAWN_INFO = {
    {
        spawn_cell = scenario.constants.SPAWN_NE,
        hall_id = scenario.constants.HALL_NE,
        player_id = ENEMY1
    },
    {
        spawn_cell = scenario.constants.SPAWN_SE,
        hall_id = scenario.constants.HALL_SE,
        player_id = ENEMY2
    },
    {
        spawn_cell = scenario.constants.SPAWN_SW,
        hall_id = scenario.constants.HALL_SW,
        player_id = ENEMY1
    },
    {
        spawn_cell = scenario.constants.SPAWN_NW,
        hall_id = scenario.constants.HALL_NW,
        player_id = ENEMY2
    }
}

local wave_index = 1
local next_wave_spawn_time = nil
local WAVE_INTERVAL = 75
local WAVE_LEVELS = {
    1, 2, 1, 2, 3,
    2, 3, 2, 3, 4,
    3, 4, 3, 5, 5
}

function scenario_init()
    actions.run(function ()
        actions.wait(1)
        scenario.chat("You're surrounded by rivals on all sides!")
        actions.wait(2)

        objectives.announce_new_objective(OBJECTIVE_DEFEND_POSITION)

        local countdown_end_time = scenario.get_time() + COUNTDOWN_DURATION
        objectives.add_objective({
            objective = {
                description = "Defend until the timer runs out"
            },
            complete_fn = function ()
                return scenario.get_time() >= countdown_end_time
            end
        })
        scenario.set_global_objective_counter(scenario.global_objective_counter_type.COUNTDOWN, countdown_end_time)

        actions.wait(15)
        scenario.hint("You can now hire Pyros from the Workshop.")

        next_wave_spawn_time = scenario.get_time() + WAVE_INTERVAL
    end)
end

function scenario_update()
    objectives.update()

    if next_wave_spawn_time ~= nil and scenario.are_objectives_complete() then
        actions.run(function()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        next_wave_spawn_time = nil
    end

    if next_wave_spawn_time ~= nil and wave_index <= #WAVE_LEVELS and scenario.get_time() >= next_wave_spawn_time then
        scenario.log("Spawning wave ", wave_index, " at time ", scenario.get_time())
        spawn_wave(WAVE_LEVELS[wave_index])
        next_wave_spawn_time = next_wave_spawn_time + WAVE_INTERVAL
        wave_index = wave_index + 1
    end

    actions.update()
end

function spawn_wave(level)
    local UNITS_PER_SPAWN_AT_LEVEL = {
        2, 4, 6, 10, 16
    }
    local UNITS_PER_SPAWN = UNITS_PER_SPAWN_AT_LEVEL[level]

    scenario.log("spawn_wave at level", level, "units per spawn", UNITS_PER_SPAWN)

    -- 50/50 cowboy bandit
    -- On levels 3 and up, throw in 20% sapper
    local COWBOY_RATIO
    local BANDIT_RATIO
    if level < 2 then
        COWBOY_RATIO = 0.5
        BANDIT_RATIO = 1.0
    else
        COWBOY_RATIO = 0.4
        BANDIT_RATIO = 0.8
    end

    local hall = {}
    for spawn_info_index=1,#SPAWN_INFO do
        -- Check that the hall is still alive
        local spawn_info = SPAWN_INFO[spawn_info_index]
        local hall_is_alive =
            scenario.get_entity_by_id(spawn_info.hall_id, hall) and
            hall.health ~= 0
        if not hall_is_alive then
            goto continue
        end

        -- Check that the player is not near this spawn cell
        if entity_util.player_has_entity_near_cell(scenario.PLAYER_ID, spawn_info.spawn_cell, 4) then
            goto continue
        end

        -- Determine entity types
        local entity_types = {}
        for index=1,UNITS_PER_SPAWN do
            local roll = math.random()
            if roll < COWBOY_RATIO then
                entity_types[index] = scenario.entity_type.COWBOY
            elseif roll < BANDIT_RATIO then
                entity_types[index] = scenario.entity_type.BANDIT
            else
                entity_types[index] = scenario.entity_type.SAPPER
            end
        end

        -- Spawn squad
        squad_util.spawn_harass_squad({
            player_id = spawn_info.player_id,
            spawn_cell = spawn_info.spawn_cell,
            target_cell = scenario.constants.TARGET,
            entity_types = entity_types
        })

        ::continue::
    end
end