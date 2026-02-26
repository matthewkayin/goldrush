local objectives = require("objectives")
local actions = require("actions")

local OBJECTIVE_SECURE_GOLDMINE = "Secure the Gold Mine"

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

        actions.wait(1.0)
        scenario.chat("Our scouts have discovered an unclaimed gold mine to the east.")
        actions.wait(2.0)
        scenario.chat("What's say we take it for ourselves?")
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
                return scenario.get_player_who_controls_goldmine(scenario.constants.UNCLAIMED_GOLDMINE) == scenario.PLAYER_ID
            end
        })
        objectives.add_objective({
            objective = {
                description = "(Optional) Build a Bunker for Defense"
            },
            complete_fn = function ()
                return false
            end
        })
    end)
end

function scenario_update()
    objectives.update()

    actions.update()
end