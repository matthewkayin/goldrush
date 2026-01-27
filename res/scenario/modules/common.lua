local actions = require("actions")

local common = {}
common.current_objective = nil
common.objectives = {}

function common.announce_new_objective(title)
    scenario.play_sound(scenario.sound.UI_CLICK)
    scenario.chat(scenario.CHAT_COLOR_GOLD, "New Objective:", title)
    actions.wait(2.0)
    common.current_objective = title
end

function common.announce_objectives_complete()
    common.current_objective = nil
    scenario.play_sound(scenario.sound.OBJECTIVE_COMPLETE)
    scenario.chat(scenario.CHAT_COLOR_GOLD, "Objective Complete", "")
    actions.wait(5.0)
    scenario.clear_objectives()
end

function common.add_objective(params)
    local index = scenario.add_objective(params.objective)
    table.insert(common.objectives, {
        index = index,
        complete_fn = params.complete_fn
    })
end

function common.update_objectives()
    local index = 1
    while index <= #common.objectives do
        local objective = common.objectives[index]
        if objective.complete_fn() then
            scenario.complete_objective(objective.index)
            table.remove(common.objectives, index)
        else
            index = index + 1
        end
    end
end

return common