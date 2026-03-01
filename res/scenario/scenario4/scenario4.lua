local objectives = require("objectives")
local actions = require("actions")

local OBJECTIVE_DEFEND_POSITION = "Defend your Position"

local COUNTDOWN_DURATION = 15 * 60

function scenario_init()
    actions.run(function ()
        objectives.announce_new_objective(OBJECTIVE_DEFEND_POSITION)
        objectives.add_objective({
            objective = {
                description = "Defend your position until the timer runs out"
            },
            complete_fn = function ()
                return false
            end
        })
        scenario.set_global_objective_counter(scenario.global_objective_counter_type.COUNTDOWN, scenario.get_time() + COUNTDOWN_DURATION)
    end)
end

function scenario_update()
    objectives.update()
    actions.update()
end