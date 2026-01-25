local actions = require("actions")
local scenario = require("scenario")

local STATE_FIND_GOLDMINE = 0

function scenario_init()
    gold.set_lose_on_buildings_destroyed(false)

    local objectives = {}
    table.insert(objectives, {
        description = "Find a Goldmine"
    })
    actions.run(function()
        actions.wait(2.0)
        scenario.add_objectives("Find a Goldmine", objectives)
        actions.wait(5.0)
        gold.chat_hint("Wagons can be used to transport units.")
    end)
end

function scenario_update()
    actions.update()
end