local actions = require("actions")

local scenario = {}

function scenario.add_objectives(title, objectives)
    gold.play_sound(gold.SOUND_UI_CLICK)
    gold.chat(gold.CHAT_COLOR_GOLD, "New Objective:", title)
    actions.wait(2.0)
    for index,objective in ipairs(objectives) do
        gold.add_objective(objective)
    end
end

return scenario