local objectives = require("objectives")
local actions = require("actions")
local squad_util = require("squad_util")
local entity_util = require("entity_util")
local ivec2 = require("ivec2")

local OBJECTIVE_SECURE_GOLDMINE = "Secure the Gold Mine"
local OBJECTIVE_DEFEAT_BANDITS = "Destroy the Bandit's Base"

local ENEMY_PLAYER_ID = 1

local is_match_over = false
local first_harass_squad_id = nil
local second_hall_id = nil
local has_sent_counterattack = false

function scenario_init()
    actions.run(function ()
        actions.wait(2.0)

        -- Camera pan to goldmine
        scenario.fog_reveal({
            cell = scenario.constants.HARASS_TARGET2,
            cell_size = 1,
            sight = 13,
            duration = 7.0
        })
        local original_camera_pos = scenario.get_camera_centered_cell()
        actions.camera_pan(scenario.constants.HARASS_TARGET2, 1.5)
        scenario.hold_camera()

        scenario.highlight_entity(scenario.constants.UNCLAIMED_GOLDMINE)
        scenario.chat("Seems this here gold mine is unclaimed.")
        actions.wait(2.0)
        scenario.chat("What's say you take it for yourself?")
        actions.wait(2.0)

        -- Return camera
        actions.camera_pan(original_camera_pos, 1.5)
        scenario.release_camera()

        actions.wait(1.0)
        objectives.announce_new_objective(OBJECTIVE_SECURE_GOLDMINE)
        objectives.add_objective({
            objective = {
                description = "Build a Town Hall near the Gold Mine"
            },
            complete_fn = function ()
                local hall_id = scenario.get_hall_surrounding_goldmine(scenario.constants.UNCLAIMED_GOLDMINE)
                if not hall_id then
                    return false
                end

                local hall = {}
                scenario.get_entity_by_id(hall_id, hall)
                local obj_complete = hall.player_id == scenario.PLAYER_ID and hall.mode == scenario.entity_mode.BUILDING_FINISHED
                if obj_complete then
                    second_hall_id = hall_id
                end
                return obj_complete
            end
        })

        actions.wait(15.0)
        scenario.hint("You can now build bunkers to defend your base.")
    end)
end

function scenario_update()
    objectives.update()

    if objectives.current_objective == OBJECTIVE_SECURE_GOLDMINE and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            actions.wait(5.0)

            first_harass_squad_id = squad_util.spawn_harass_squad({
                player_id = ENEMY_PLAYER_ID,
                spawn_cell = scenario.constants.HARASS_SPAWN2,
                target_cell = scenario.constants.HARASS_TARGET2,
                entity_types = {
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT
                }
            })
        end)
    end

    -- Create counterattack the first time they move out on enemy
    if not has_sent_counterattack and player_has_entity_near_counterattack_triggers() then
        spawn_counterattack_squad()
        has_sent_counterattack = true
    end

    -- Handle first attack squad defeat
    if first_harass_squad_id ~= nil and not scenario.bot_squad_exists(ENEMY_PLAYER_ID, first_harass_squad_id) then
        first_harass_squad_id = nil

        actions.run(function ()
            actions.wait(3.0)
            scenario.chat("Those bandits are trying to steal your gold!")
            actions.wait(3.0)

            objectives.announce_new_objective(OBJECTIVE_DEFEAT_BANDITS)
            objectives.add_objective({
                objective = {
                    description = "Destroy the Bandit's Base"
                },
                complete_fn = function ()
                    return scenario.is_player_defeated(ENEMY_PLAYER_ID)
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Both of your Bases must Survive"
                },
                complete_fn = function ()
                    return scenario.is_player_defeated(ENEMY_PLAYER_ID)
                end
            })
        end)

        actions.run(function ()
            while not is_match_over do
                actions.wait(120)
                spawn_harass_squad()
            end
        end)
    end

    -- Check for hall death
    if not is_match_over and objectives.current_objective == OBJECTIVE_DEFEAT_BANDITS and not are_player_halls_alive() then
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
        is_match_over = true
    end

    -- Check for victory
    if not is_match_over and objectives.current_objective == OBJECTIVE_DEFEAT_BANDITS and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_defeat()
        end)
        is_match_over = true
    end

    actions.update()
end

function are_player_halls_alive()
    local hall = {}
    if not scenario.get_entity_by_id(scenario.constants.FIRST_HALL_ID, hall) then
        return false
    end

    if hall.mode == scenario.entity_mode.BUILDING_DESTROYED then
        return false
    end

    if second_hall_id == nil then
        error("Second hall ID should not be nil in are_player_halls_alive()")
    end

    if not scenario.get_entity_by_id(second_hall_id, hall) then
        return false
    end

    if hall.mode == scenario.entity_mode.BUILDING_DESTROYED then
        return false
    end

    return true
end

function player_has_entity_near_counterattack_triggers()
    local entity_near_cell_id = entity_util.find_entity(function (entity)
        return entity.player_id == scenario.PLAYER_ID and
            (ivec2.manhattan_distance(entity.cell, scenario.constants.COUNTERATTACK_TRIGGER1) <= 4 or
            ivec2.manhattan_distance(entity.cell, scenario.constants.COUNTERATTACK_TRIGGER2) <= 4 or
            ivec2.manhattan_distance(entity.cell, scenario.constants.COUNTERATTACK_TRIGGER3) <= 4)
    end)
    return entity_near_cell_id ~= nil
end

function spawn_counterattack_squad()
    local harass_spawn_roll = math.random(0, 1)
    local harass_spawn_cell
    if harass_spawn_roll == 0 then
        harass_spawn_cell = scenario.constants.HARASS_SPAWN3
    else
        harass_spawn_cell = scenario.constants.HARASS_SPAWN4
    end

    squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = harass_spawn_cell,
        target_cell = scenario.constants.HARASS_TARGET1,
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY
        }
    })
end

function spawn_harass_squad()
    local harass_target_roll = math.random(0, 1)
    local harass_spawn_roll = math.random(0, 1)
    local harass_target_cell
    local harass_spawn_cell
    if harass_target_roll == 0 then
        harass_target_cell = scenario.constants.HARASS_TARGET1
        if harass_spawn_roll == 0 then
            harass_spawn_cell = scenario.constants.HARASS_SPAWN3
        else
            harass_spawn_cell = scenario.constants.HARASS_SPAWN4
        end
    else
        harass_target_cell = scenario.constants.HARASS_TARGET2
        if harass_spawn_roll == 0 then
            harass_spawn_cell = scenario.constants.HARASS_SPAWN1
        else
            harass_spawn_cell = scenario.constants.HARASS_SPAWN2
        end
    end

    local harass_squad_size = math.random(2, 4)
    local harass_squad_entity_types = {}
    for i=1,harass_squad_size do
        local harass_entity_type_roll = math.random(0, 1)
        local harass_entity_type
        if harass_entity_type_roll == 0 then
            harass_entity_type = scenario.entity_type.BANDIT
        else
            harass_entity_type = scenario.entity_type.COWBOY
        end
        table.insert(harass_squad_entity_types, harass_entity_type)
    end
    squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = harass_spawn_cell,
        target_cell = harass_target_cell,
        entity_types = harass_squad_entity_types
    })
end