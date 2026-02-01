local actions = require("actions")
local objectives = require("objectives")

local ENEMY_PLAYER_ID = 1

function scenario_init()
    scenario.set_bot_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PAUSE, true)
    actions.run(function ()
        actions.wait(1.0)
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

        objectives.announce_new_objective("Mine 10,000 gold before your opponent")
        objectives.add_objective({
            objective = {
                description = "Mine 10,000 gold",
            },
            complete_fn = function ()
                return false
            end
        })
        scenario.set_global_objective_counter(scenario.global_objective_counter_type.GOLD)

        actions.wait(3.0)

        scenario.set_bot_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PAUSE, false)
    end)
end

function scenario_update()
    actions.update()
end