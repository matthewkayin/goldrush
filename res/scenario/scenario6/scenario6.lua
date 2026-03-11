local objectives = require("objectives")
local actions = require("actions")

local OBJECTIVE_STEAL_IT_BACK = "Steal it Back!"
local objective_crates_index = nil

local CRATES = {
    scenario.constants.CRATE1,
    scenario.constants.CRATE2,
    scenario.constants.CRATE3
}

local MINERS = {
    {
        id = scenario.constants.MINER1,
        gold_held_crate_index = nil
    },
    {
        id = scenario.constants.MINER2,
        gold_held_crate_index = nil
    },
    {
        id = scenario.constants.MINER3,
        gold_held_crate_index = nil
    },
    {
        id = scenario.constants.MINER4,
        gold_held_crate_index = nil
    }
}

local is_match_over = false

function scenario_init()
    -- Validate that all crates have valid IDs
    local crate = {}
    for index = 1,#CRATES do
        if not scenario.get_entity_by_id(CRATES[index], crate) then
            scenario.log("ERROR - Crate with ID ", CRATES[index], " does not exist.")
            error("Crate does not exist")
        end
    end

    -- Validate that all miners have valid IDs
    local miner = {}
    for index = 1,#MINERS do
        if not scenario.get_entity_by_id(MINERS[index].id, miner) then
            scenario.log("ERROR - Miner with ID ", MINERS[index].id, " does not exist.")
            error("Miner does not exist")
        end
    end

    actions.run(function ()
        actions.wait(1.0)

        scenario.chat("Bandits stole all of your gold!")
        actions.wait(2.0)

        objectives.announce_new_objective(OBJECTIVE_STEAL_IT_BACK)
        objective_crates_index = objectives.add_objective({
            objective = {
                description = "Steal back your gold",
                counter_target = #CRATES
            },
            complete_fn = function ()
                return false
            end
        })

        actions.wait(5.0)
        scenario.hint("You can now hire Balloons and Sappers.")
        actions.wait(2.0)
        scenario.chat("Ballons are flying scout units.")
        actions.wait(2.0)
        scenario.chat("They can see invisible units, such as landmines.")
    end)
end

function scenario_update()
    objectives.update()

    if not is_match_over and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end

    actions.update()
end