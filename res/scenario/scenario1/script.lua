local actions = require("actions")
local objectives = require("objectives")

local OBJECTIVE_FIND_GOLDMINE = "Find a Goldmine"
local OBJECTIVE_BUILD_HALL = "Build a Town Hall"
local OBJECTIVE_ESTABLISH_BASE = "Establish a Base"
local OBJECTIVE_DEFEAT_BANDITS = "Destroy the Bandit's Base"

local ENEMY_BANDITS_PLAYER_ID = 1
local bandit_attack_squad_id = nil

function scenario_init()
    actions.run(function()
        actions.wait(2.0)
        objectives.announce_new_objective(OBJECTIVE_FIND_GOLDMINE)
        objectives.add_objective({
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
    objectives.update()

    if objectives.current_objective == OBJECTIVE_FIND_GOLDMINE and scenario.are_objectives_complete() then
        actions.run(function()
            objectives.announce_objectives_complete()
            objectives.announce_new_objective("Build a Town Hall")
            objectives.add_objective({
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
    elseif objectives.current_objective == OBJECTIVE_BUILD_HALL and scenario.are_objectives_complete() then
        actions.run(function()
            objectives.announce_objectives_complete()
            objectives.announce_new_objective("Establish a Base")
            objectives.add_objective({
                objective = {
                    description = "Hire 8 Miners",
                    entity_type = scenario.entity_type.MINER,
                    counter_target = 8
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.MINER) >= 8
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Hire 4 Cowboys",
                    entity_type = scenario.entity_type.COWBOY,
                    counter_target = 4
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.COWBOY) >= 4
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Build a Bunker"
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.BUNKER) >= 1
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Garrison into the Bunker"
                },
                complete_fn = function()
                    return scenario.get_player_full_bunker_count(scenario.PLAYER_ID) >= 1
                end
            })
        end)
    elseif objectives.current_objective == OBJECTIVE_ESTABLISH_BASE and scenario.are_objectives_complete() then
        actions.run(function()
            objectives.announce_objectives_complete()
            local squad_entities = {}
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            bandit_attack_squad_id = scenario.spawn_enemy_squad({
                player_id = ENEMY_BANDITS_PLAYER_ID,
                spawn_cell = scenario.constants.BANDIT_SPAWN_CELL,
                target_cell = scenario.constants.BANDIT_TARGET_CELL,
                entities = squad_entities
            })
            scenario.play_sound(scenario.sound.ALERT_BELL)
            scenario.fog_reveal({
                player_id = scenario.PLAYER_ID,
                cell = scenario.constants.BANDIT_SPAWN_CELL,
                cell_size = 1,
                sight = 9,
                duration = 3.0
            })
            actions.camera_pan(scenario.constants.BANDIT_SPAWN_CELL, 0.75)
            scenario.chat(scenario.CHAT_COLOR_WHITE, "", "It's a bandit attack! Hold steady!")
            actions.wait(2.0)
            scenario.begin_camera_return(0.75)
        end)
    elseif objectives.current_objective == OBJECTIVE_DEFEAT_BANDITS and scenario.are_objectives_complete() then
        actions.run(function()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    -- Handle bandit attack defeated
    if bandit_attack_squad_id ~= nil and not scenario.does_squad_exist(ENEMY_BANDITS_PLAYER_ID, bandit_attack_squad_id) then
        actions.run(function()
            actions.wait(2.0)
            scenario.fog_reveal({
                player_id = scenario.PLAYER_ID,
                cell = scenario.constants.BANDIT_BASE_CELL_1,
                cell_size = 1,
                sight = 13,
                duration = 5.0
            })
            actions.camera_pan(scenario.constants.BANDIT_BASE_CELL_1, 1.5)
            objectives.announce_new_objective(OBJECTIVE_DEFEAT_BANDITS)
            objectives.add_objective({
                objective = {
                    description = OBJECTIVE_DEFEAT_BANDITS
                },
                complete_fn = function()
                    return scenario.is_player_defeated(ENEMY_BANDITS_PLAYER_ID)
                end
            })
            scenario.free_camera()
            scenario.fog_reveal({
                player_id = scenario.PLAYER_ID,
                cell = scenario.constants.BANDIT_BASE_CELL_2,
                cell_size = 1,
                sight = 13,
                duration = 5.0
            })
            actions.camera_pan(scenario.constants.BANDIT_BASE_CELL_2, 1.5)
            actions.wait(2.0)
            actions.camera_return(2.0)
        end)
        bandit_attack_squad_id = nil
    end

    -- This is placed on the bottom so that any actions can begin running on the frame they are kicked off
    actions.update()
end