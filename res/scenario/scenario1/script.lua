local actions = require("actions")
local scenario = require("scenario")

local STATE_FIND_GOLDMINE = 0
local STATE_ESTABLISH_BASE = 1
local state = STATE_FIND_GOLDMINE

function scenario_init()
    gold.set_lose_on_buildings_destroyed(false)

    local objectives = {}
    table.insert(objectives, {
        description = "Find a Goldmine"
    })
    actions.run(function()
        actions.wait(2.0)
        scenario.set_objectives("Find a Goldmine", objectives)
        actions.wait(5.0)
        gold.chat_hint("Wagons can be used to transport units.")
    end)
end

function scenario_update()
    if state == STATE_FIND_GOLDMINE then
        if gold.is_entity_visible_to_player(gold.SCENARIO_FIRST_ENTITY) then
            actions.run(function()
                scenario.objectives_complete()

                local objectives = {}
                table.insert(objectives, {
                    description = "Hire 8 Miners",
                    entity_type = gold.ENTITY_MINER,
                    counter_target = 8
                })
                table.insert(objectives, {
                    description = "Hire 4 Cowboys",
                    entity_type = gold.ENTITY_COWBOY,
                    counter_target = 4
                })
                table.insert(objectives, {
                    description = "Build a Bunker"
                })
                table.insert(objectives, {
                    description = "Garrison into the Bunker"
                })
                scenario.set_objectives("Establish a Base", objectives)
            end)
            state = STATE_ESTABLISH_BASE
        end
    end

    -- This is placed on the bottom so that any actions can begin running on the frame they are kicked off
    actions.update()
end