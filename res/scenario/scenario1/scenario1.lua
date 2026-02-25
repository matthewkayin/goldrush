local actions = require("actions")
local objectives = require("objectives")
local squad_util = require("squad_util")
local entity_util = require("entity_util")
local ivec2 = require("ivec2")

local OBJECTIVE_FIND_GOLDMINE = "Find a Goldmine"
local OBJECTIVE_ESTABLISH_BASE = "Establish a Base"
local OBJECTIVE_DEFEAT_BANDITS = "Destroy the Bandit's Base"

local ENEMY_BANDITS_PLAYER_ID = 1

local BANDIT_ATTACK_WARNING_NOT_GIVEN = 0
local BANDIT_ATTACK_WARNING_GIVEN = 1
local BANDIT_ATTACK_SENT = 2

local bandits_near_goldmine_squad_id = nil
local has_highlighted_goldmine = false

local bandit_attack_state = BANDIT_ATTACK_WARNING_NOT_GIVEN
local bandit_attack_squad_id = nil

local has_given_two_saloon_hint = false
local has_given_bunker_hint = false
local has_handled_bandits_defeated = false

function scenario_init()
    bandits_near_goldmine_squad_id = scenario.bot_get_entity_squad_id(scenario.constants.BANDIT_SQUATTER)
    if bandits_near_goldmine_squad_id == nil then
        error("Could not find bandit squatter squad.")
    end

    actions.run(function ()
        actions.wait(2.0)
        objectives.announce_new_objective(OBJECTIVE_FIND_GOLDMINE)
        objectives.add_objective({
            objective = {
                description = "Find a Goldmine"
            },
            complete_fn = function ()
                return has_highlighted_goldmine and
                    not scenario.bot_squad_exists(ENEMY_BANDITS_PLAYER_ID, bandits_near_goldmine_squad_id)
            end
        })
    end)
end

function scenario_update()
    objectives.update()

    -- Highlight goldmine
    if not has_highlighted_goldmine and scenario.is_entity_visible_to_player(scenario.constants.GOLDMINE) then
        scenario.highlight_entity(scenario.constants.GOLDMINE)
        has_highlighted_goldmine = true
    end

    if objectives.current_objective ~= OBJECTIVE_DEFEAT_BANDITS and
            scenario.is_player_defeated(ENEMY_BANDITS_PLAYER_ID) and
            not has_handled_bandits_defeated then
        has_handled_bandits_defeated = true
        actions.run(function ()
            scenario.clear_objectives()
            actions.wait(3.0)
            scenario.chat("Not much for following directions, are you?")
            actions.wait(3.0)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    end

    if scenario.are_objectives_complete() then
        on_objectives_complete()
    end

    -- Bandit atttack management
    if bandit_attack_state ~= BANDIT_ATTACK_SENT then
        local entities_out_of_line = entity_util.find_entities(function (entity)
            return entity.player_id == scenario.PLAYER_ID and
                ivec2.manhattan_distance(entity.cell, scenario.constants.HARASS_SPAWN_CELL) <= 3
        end)
        if #entities_out_of_line ~= 0 then
            scenario.log("Stopping units", entities_out_of_line)
            scenario.queue_match_input({
                player_id = scenario.PLAYER_ID,
                type = scenario.match_input_type.STOP,
                entity_ids = entities_out_of_line
            })
            actions.run(function ()
                actions.camera_pan(scenario.constants.HARASS_TRIGGER_CELL)
                scenario.chat("Careful now, partner. You best not leave your base undefended.")
                actions.hold_camera(2.0)
            end)
        end
    end
    if not has_bandit_attacked and
            objectives.current_objective == OBJECTIVE_ESTABLISH_BASE and
            entity_util.player_has_entity_near_cell(scenario.constants.HARASS_TRIGGER_CELL, 2) then
        has_bandit_attacked = true
        objectives.clear_objectives()
        actions.run(function ()
            bandit_attack_at_cell(scenario.constants.HARASS_SPAWN_CELL2)
        end)
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

    -- Bunker hint
    if not has_given_bunker_hint and
            objectives.current_objective == OBJECTIVE_ESTABLISH_BASE and
            scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.BUNKER) >= 1 then
        actions.run(function ()
            actions.wait(1.0)
            scenario.hint("Garrison your cowboys inside the bunker for better defense.")
        end)
        has_given_bunker_hint = true
    end

    actions.update()
end

function on_objectives_complete()
    if objectives.current_objective == OBJECTIVE_FIND_GOLDMINE then
        actions.run(function ()
            objectives.announce_objectives_complete()
            actions.wait(2.0)
            scenario.chat("Seems there's bandits in these parts.")
            actions.wait(2.0)
            scenario.chat("You best establish a base. They may attack again soon.")
            actions.wait(3.0)
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
                    description = "Hire 6 Cowboys",
                    entity_type = scenario.entity_type.COWBOY,
                    counter_target = 6
                },
                complete_fn = function ()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.COWBOY) >= 6
                end
            })
            objectives.add_objective({
                objective = {
                    description = "Build a Bunker"
                },
                complete_fn = function ()
                    return scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.BUNKER) >= 1
                end
            })
        end)
    elseif objectives.current_objective == OBJECTIVE_ESTABLISH_BASE then
        actions.run(function ()
            objectives.announce_objectives_complete()
            bandit_attack_at_cell(scenario.constants.HARASS_SPAWN_CELL)
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

    local previous_camera_cell = scenario.get_camera_centered_cell()
    actions.camera_pan(cell, 0.75)
    scenario.chat("Bandits are attacking! Defend yourself!")
    actions.hold_camera(2.0)
    actions.camera_pan(previous_camera_cell, 0.75)
end