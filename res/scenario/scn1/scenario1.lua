local actions = require("actions")
local objectives = require("objectives")

local OBJECTIVE_FIND_GOLDMINE = "Find a Goldmine"
local OBJECTIVE_BUILD_HALL = "Build a Town Hall"
local OBJECTIVE_ESTABLISH_BASE = "Establish a Base"
local OBJECTIVE_DEFEAT_BANDITS = "Destroy the Bandit's Camp"

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
                return scenario.entity_is_visible_to_player(scenario.constants.GOLDMINE2)
            end
        })
    end)
end

function scenario_update()
    objectives.update()

    if scenario.are_objectives_complete() then
        on_objectives_complete()
    end

    -- Handle bandit attack defeated
    if bandit_attack_squad_id ~= nil and not scenario.does_squad_exist(ENEMY_BANDITS_PLAYER_ID, bandit_attack_squad_id) then
        bandit_attack_squad_id = nil
        actions.run(function()
            actions.wait(3.0)
            scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Those bandits must have a camp nearby...")
            actions.wait(1.0)
            objectives.announce_new_objective(OBJECTIVE_DEFEAT_BANDITS)
            objectives.add_objective({
                objective = {
                    description = OBJECTIVE_DEFEAT_BANDITS
                },
                complete_fn = function()
                    return scenario.is_player_defeated(ENEMY_BANDITS_PLAYER_ID)
                end
            })
        end)
    end

    actions.update()
end

function on_objectives_complete()
    if objectives.current_objective == OBJECTIVE_FIND_GOLDMINE then
        actions.run(function()
            scenario.highlight_entity(scenario.constants.GOLDMINE2)
            objectives.announce_objectives_complete()
            objectives.announce_new_objective(OBJECTIVE_BUILD_HALL)
            objectives.add_objective({
                objective = {
                    description = "Build a Town Hall"
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.HALL) >= 1
                end
            })
            actions.wait(5.0)
            scenario.hint("Multiple miners can work together to build buildings more quickly.")
        end)
    elseif objectives.current_objective == OBJECTIVE_BUILD_HALL then
        actions.run(function()
            objectives.announce_objectives_complete()
            objectives.announce_new_objective(OBJECTIVE_ESTABLISH_BASE)
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
                    description = "Build a Saloon"
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.SALOON) >= 1
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Hire 6 Cowboys",
                    entity_type = scenario.entity_type.COWBOY,
                    counter_target = 6
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.COWBOY) >= 4
                end
            })
        end)
    elseif objectives.current_objective == OBJECTIVE_ESTABLISH_BASE then
        actions.run(function()
            objectives.announce_objectives_complete()

            -- Bandit attack
            local squad_entities = {}
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            bandit_attack_squad_id = scenario.spawn_enemy_squad({
                player_id = ENEMY_BANDITS_PLAYER_ID,
                spawn_cell = scenario.constants.BANDIT_ATTACK_SPAWN_CELL,
                target_cell = scenario.constants.BANDIT_ATTACK_TARGET_CELL,
                entities = squad_entities
            })

            scenario.play_sound(scenario.sound.ALERT_BELL)
            scenario.fog_reveal({
                player_id = scenario.PLAYER_ID,
                cell = scenario.constants.BANDIT_ATTACK_SPAWN_CELL,
                cell_size = 1,
                sight = 9,
                duration = 3.0
            })
            scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Bandits are attacking! Defend yourself!")
        end)
    elseif objectives.current_objective == OBJECTIVE_DEFEAT_BANDITS then
        actions.run(function()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end
end