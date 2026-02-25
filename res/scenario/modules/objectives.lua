local actions = require("actions")

local objectives = {}
objectives.current_objective = nil
objectives.objectives = {}

function objectives.announce_new_objective(title)
    scenario.play_sound(scenario.sound.UI_CLICK)
    scenario.chat_prefixed(scenario.CHAT_COLOR_GOLD, "New Objective:", title)
    actions.wait(2.0)
    objectives.current_objective = title
end

function objectives.announce_objectives_complete()
    objectives.current_objective = nil
    scenario.play_sound(scenario.sound.OBJECTIVE_COMPLETE)
    scenario.chat_prefixed(scenario.CHAT_COLOR_GOLD, "Objective Complete", "")
    actions.wait(3.0)
    scenario.clear_objectives()
end

function objectives.announce_objectives_failed()
    objectives.current_objective = nil
    scenario.play_sound(scenario.sound.UI_CLICK)
    scenario.chat_prefixed(scenario.CHAT_COLOR_GOLD, "Objective Failed", "")
    actions.wait(3.0)
    scenario.clear_objectives()
end

function objectives.clear_objectives()
    objectives.current_objective = nil
    scenario.clear_objectives()
end

function objectives.add_objective(params)
    local index = scenario.add_objective(params.objective)
    table.insert(objectives.objectives, {
        index = index,
        complete_fn = params.complete_fn
    })
end

function objectives.update()
    local index = 1
    while index <= #objectives.objectives do
        local objective = objectives.objectives[index]
        if objective.complete_fn() then
            scenario.complete_objective(objective.index)
            table.remove(objectives.objectives, index)
        else
            index = index + 1
        end
    end
end

return objectives