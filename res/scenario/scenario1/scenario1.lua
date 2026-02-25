local actions = require("actions")
local objectives = require("objectives")
local squad_util = require("squad_util")
local entity_util = require("entity_util")
local ivec2 = require("ivec2")

local OBJECTIVE_FIND_GOLDMINE = "Find a Gold Mine"
local OBJECTIVE_BUILD_HALL = "Build a Town Hall"
local OBJECTIVE_ESTABLISH_BASE = "Establish a Base"
local OBJECTIVE_DEFEAT_BANDITS = "Destroy the Bandit's Base"

local ENEMY_BANDITS_PLAYER_ID = 1

local bandit_attack_squad_id = nil

local has_given_two_saloon_hint = false
local has_handled_bandits_defeated = false

function scenario_init()
    actions.run(function ()
        actions.wait(2.0)
        objectives.announce_new_objective(OBJECTIVE_FIND_GOLDMINE)
        objectives.add_objective({
            objective = {
                description = "Find a Gold Mine"
            },
            complete_fn = function ()
                return scenario.is_entity_visible_to_player(scenario.constants.GOLDMINE)
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
    if bandit_attack_squad_id ~= nil and not scenario.bot_squad_exists(ENEMY_BANDITS_PLAYER_ID, bandit_attack_squad_id) then
        bandit_attack_squad_id = nil
        actions.run(function ()
            actions.wait(3.0)
            scenario.chat("Those bandits must have a hideout nearby...")
            actions.wait(3.0)
            objectives.announce_new_objective(OBJECTIVE_DEFEAT_BANDITS)
            objectives.add_objective({
                objective = {
                    description = OBJECTIVE_DEFEAT_BANDITS
                },
                complete_fn = function ()
                    return scenario.is_player_defeated(ENEMY_BANDITS_PLAYER_ID)
                end
            })
        end)
    end

    -- Two saloons hint
    if not has_given_two_saloon_hint and
            objectives.current_objective == OBJECTIVE_ESTABLISH_BASE and
            scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.COWBOY) >= 1 then
        actions.run(function ()
            actions.wait(1.0)
            scenario.hint("You can build more saloons to hire more cowboys at once.")
        end)
        has_given_two_saloon_hint = true
    end

    -- Player defeats bandits out of order
    if objectives.current_objective ~= OBJECTIVE_DEFEAT_BANDITS and
            not has_handled_bandits_defeated and
            scenario.is_player_defeated(ENEMY_BANDITS_PLAYER_ID) then
        actions.run(function ()
            objectives.clear_objectives()
            actions.wait(2.0)
            scenario.chat("Not much for following orders, are ya?")
            actions.wait(2.0)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        has_handled_bandits_defeated = true
    end

    actions.update()
end

function on_objectives_complete()
    if objectives.current_objective == OBJECTIVE_FIND_GOLDMINE then
        actions.run(function ()
            scenario.highlight_entity(scenario.constants.GOLDMINE)
            objectives.announce_objectives_complete()
            objectives.announce_new_objective(OBJECTIVE_BUILD_HALL)
            objectives.add_objective({
                objective = {
                    description = "Build a Town Hall"
                },
                complete_fn = function ()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.HALL) >= 1
                end
            })
            actions.wait(5.0)
            scenario.hint("Multiple miners can work together to build buildings more quickly.")
        end)
    elseif objectives.current_objective == OBJECTIVE_BUILD_HALL then
        actions.run(function ()
            objectives.announce_objectives_complete()
            objectives.announce_new_objective(OBJECTIVE_ESTABLISH_BASE)
            objectives.add_objective({
                objective = {
                    description = "Hire 8 Miners",
                    entity_type = scenario.entity_type.MINER,
                    counter_target = 8
                },
                complete_fn = function ()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.MINER) >= 8
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Build a Saloon"
                },
                complete_fn = function ()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.SALOON) >= 1
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Hire 6 Cowboys",
                    entity_type = scenario.entity_type.COWBOY,
                    counter_target = 6
                },
                complete_fn = function ()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.COWBOY) >= 6
                end
            })
        end)
    elseif objectives.current_objective == OBJECTIVE_ESTABLISH_BASE then
        actions.run(function ()
            objectives.announce_objectives_complete()
            local bandit_attack_cell
            if entity_util.player_has_entity_near_cell(scenario.constants.HARASS_SPAWN_CELL, 4) then
                bandit_attack_cell = scenario.constants.HARASS_SPAWN_CELL2
            else
                bandit_attack_cell = scenario.constants.HARASS_SPAWN_CELL
            end
            bandit_attack_at_cell(bandit_attack_cell)
        end)
    elseif objectives.current_objective == OBJECTIVE_DEFEAT_BANDITS and not has_handled_bandits_defeated then
        has_handled_bandits_defeated = true
        actions.run(function ()
            actions.wait(1.0)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end
end

function bandit_attack_at_cell(cell)
    bandit_attack_squad_id = squad_util.spawn_harass_squad({
        player_id = ENEMY_BANDITS_PLAYER_ID,
        spawn_cell = cell,
        target_cell = scenario.constants.HARASS_TARGET_CELL,
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT
        }
    })

    scenario.play_sound(scenario.sound.ALERT_BELL)
    scenario.fog_reveal({
        cell = cell,
        cell_size = 1,
        sight = 9,
        duration = 3.0
    })

    actions.run(function ()
        local previous_camera_cell = scenario.get_camera_centered_cell()
        actions.camera_pan(cell, 0.75)
        scenario.chat("Bandits are attacking! Defend yourself!")
        actions.hold_camera(2.0)
        actions.camera_pan(previous_camera_cell, 0.75)
    end)
end