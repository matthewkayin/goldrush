local actions = require("actions")
local objectives = require("objectives")

local OBJECTIVE_MINE_GOLD = "Mine 10,000 Gold before your Opponent"

local ENEMY_PLAYER_ID = 1

local TARGET_GOLD_COUNT = 10000
local HARASS_TRIGGER_DISTANCE = 16

local has_given_bunker_hint = false
local has_sent_reactive_harass = false

function scenario_init()
    scenario.set_bot_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PAUSE, true)
    actions.run(function ()
        actions.wait(5.0)
        local previous_camera_cell = scenario.get_camera_centered_cell()

        -- Camera pan 1
        scenario.fog_reveal({
            cell = scenario.constants.HALL_CELL1,
            cell_size = 4,
            sight = 13,
            duration = 7.0
        })
        actions.camera_pan(scenario.constants.HALL_CELL1, 1.5)
        scenario.hold_camera()
        scenario.play_sound(scenario.sound.UI_CLICK)
        scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Those greedy prospectors are taking all the gold!")
        actions.wait(1.0)
        scenario.match_input_build({
            building_type = scenario.entity_type.HALL,
            building_cell = scenario.constants.HALL_CELL1,
            builder_id = scenario.constants.HALL_BUILDER1
        })
        actions.wait(3.0)

        -- Camera pan 2
        scenario.fog_reveal({
            cell = scenario.constants.HALL_CELL2,
            cell_size = 4,
            sight = 13,
            duration = 6.0
        })
        actions.camera_pan(scenario.constants.HALL_CELL2, 1.5)
        scenario.hold_camera()
        scenario.match_input_build({
            building_type = scenario.entity_type.HALL,
            building_cell = scenario.constants.HALL_CELL2,
            builder_id = scenario.constants.HALL_BUILDER2
        })
        actions.wait(3.0)

        -- Camera return
        actions.camera_pan(previous_camera_cell, 1.5)
        scenario.release_camera()

        actions.wait(1.0)

        objectives.announce_new_objective(OBJECTIVE_MINE_GOLD)
        objectives.add_objective({
            objective = {
                description = "Mine 10,000 Gold",
            },
            complete_fn = function ()
                return scenario.get_player_gold_mined_total(scenario.PLAYER_ID) >= TARGET_GOLD_COUNT
            end
        })
        scenario.set_global_objective_counter(scenario.global_objective_counter_type.GOLD)

        actions.wait(3.0)

        scenario.set_bot_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PAUSE, false)

        actions.wait(5.0)
        scenario.hint("You can now build bunkers to defend your base.")
    end)
end

function scenario_update()
    objectives.update()

    if objectives.current_objective == OBJECTIVE_MINE_GOLD and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    elseif scenario.get_player_gold_mined_total(ENEMY_PLAYER_ID) >= TARGET_GOLD_COUNT then
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
    end

    if not has_given_bunker_hint and scenario.get_player_entity_count(scenario.PLAYER_ID, scenario.entity_type.BUNKER) >= 1 then
        has_given_bunker_hint = true
        actions.run(function ()
            actions.wait(3.0)
            scenario.hint("Garrison cowboys inside your new bunker.")
        end)
    end

    if not has_sent_reactive_harass then
        local player_is_attacking_base1 = scenario.player_has_entity_near_cell(scenario.PLAYER_ID, scenario.constants.HALL_CELL1, HARASS_TRIGGER_DISTANCE)
        local player_is_attacking_base2 = scenario.player_has_entity_near_cell(scenario.PLAYER_ID, scenario.constants.HALL_CELL2, HARASS_TRIGGER_DISTANCE)
        if player_is_attacking_base1 or player_is_attacking_base2 then

            local squad_entities = {}
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.BANDIT)
            table.insert(squad_entities, scenario.entity_type.COWBOY)
            table.insert(squad_entities, scenario.entity_type.COWBOY)

            scenario.spawn_enemy_squad({
                player_id = ENEMY_PLAYER_ID,
                target_cell = scenario.constants.HARASS_TARGET_CELL,
                spawn_cell = scenario.constants.HARASS_SPAWN_CELL,
                entities = squad_entities
            })
            has_sent_reactive_harass = true
        end
    end

    actions.update()
end