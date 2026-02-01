local actions = require("actions")

local ENEMY_PLAYER_ID = 1

function scenario_init()
    scenario.set_bot_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PAUSE, true)
    actions.run(function ()
        actions.wait(1.0)
        scenario.fog_reveal({
            cell = scenario.constants.HALL_CELL1,
            cell_size = 4,
            sight = 13,
            duration = 15.0
        })
        actions.camera_pan(scenario.constants.HALL_CELL1, 1.0)
        scenario.hold_camera()
        scenario.match_input_build({
            building_type = scenario.entity_type.HALL,
            building_cell = scenario.constants.HALL_CELL1,
            builder_id = scenario.constants.HALL_BUILDER1
        })
        scenario.release_camera()
        -- scenario.set_bot_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PAUSE, false)
    end)
end

function scenario_update()
    actions.update()
end