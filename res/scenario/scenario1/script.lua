local actions = require("actions")
local common = require("common")

local OBJECTIVE_FIND_GOLDMINE = "Find a Goldmine"
local OBJECTIVE_BUILD_HALL = "Build a Town Hall"
local OBJECTIVE_ESTABLISH_BASE = "Establish a Base"

function scenario_init()
    scenario.set_lose_on_buildings_destroyed(false)

    actions.run(function()
        actions.wait(2.0)
        common.announce_new_objective(OBJECTIVE_FIND_GOLDMINE)
        common.add_objective({
            objective = {
                description = "Find a Goldmine"
            }, 
            complete_fn = function()
                if scenario.entity_is_visible_to_player(scenario.constants.FIRST_GOLDMINE) then
                    scenario.highlight_entity(scenario.constants.FIRST_GOLDMINE)
                    return true
                end
                return false
            end
        })
        actions.wait(5.0)
        scenario.hint("Wagons can be used to transport units.")
    end)
end

function scenario_update()
    common.update_objectives()

    if common.current_objective == OBJECTIVE_FIND_GOLDMINE and scenario.are_objectives_complete() then
        actions.run(function()
            common.announce_objectives_complete()
            common.announce_new_objective("Build a Town Hall")
            common.add_objective({
                objective = {
                    description = "Build a Town Hall",
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(0, scenario.entity_type.HALL) >= 1
                end
            })
            actions.wait(5.0)
            scenario.hint("Multiple miners can work together to build buildings more quickly.")
        end)
    elseif common.current_objective == OBJECTIVE_BUILD_HALL and scenario.are_objectives_complete() then
        actions.run(function()
            common.announce_objectives_complete()
            common.announce_new_objective("Establish a Base")
            -- DEBUG: delete this objective
            common.add_objective({
                objective = {
                    description = "Hire 4 Miners",
                    entity_type = scenario.entity_type.MINER,
                    counter_target = 4
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.MINER) >= 4
                end
            })
            --[[
            common.add_objective({
                objective = {
                    description = "Hire 8 Miners",
                    entity_type = scenario.entity_type.MINER,
                    counter_target = 8
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.MINER) >= 8
                end
            })
            common.add_objective({
                objective = {
                    description = "Hire 4 Cowboys",
                    entity_type = scenario.entity_type.COWBOY,
                    counter_target = 4
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.COWBOY) >= 4
                end
            })
            common.add_objective({
                objective = {
                    description = "Build a Bunker"
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.BUNKER) >= 1
                end
            })
            common.add_objective({
                objective = {
                    description = "Garrison into the Bunker"
                },
                complete_fn = function()
                    return scenario.get_player_full_bunker_count(scenario.PLAYER_ID) >= 1
                end
            })
            ]]
        end)
    elseif common.current_objective == OBJECTIVE_ESTABLISH_BASE and scenario.are_objectives_complete() then
        actions.run(function()
            common.announce_objectives_complete()
            local squad_entities = {}
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            local squad_id = scenario.spawn_enemy_squad({
                player_id = 1,
                spawn_cell = scenario.constants.BANDIT_SPAWN_CELL,
                target_cell = scenario.constants.BANDIT_TARGET_CELL,
                entities = squad_entities
            })
            scenario.log("Created squad", squad_id)
            scenario.play_sound(scenario.sound.ALERT_BELL)
            scenario.chat(scenario.CHAT_COLOR_WHITE, "", "It's a bandit attack! Hold steady!")
            scenario.fog_reveal({
                player_id = scenario.PLAYER_ID,
                cell = scenario.constants.BANDIT_SPAWN_CELL,
                cell_size = 1,
                sight = 9,
                duration = 3.0
            })
            scenario.camera_pan(scenario.constants.BANDIT_SPAWN_CELL, 0.5)
            actions.wait(2.0)
            scenario.camera_return()
        end)
    end

    -- This is placed on the bottom so that any actions can begin running on the frame they are kicked off
    actions.update()
end