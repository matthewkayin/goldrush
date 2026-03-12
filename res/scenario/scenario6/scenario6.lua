local objectives = require("objectives")
local actions = require("actions")
local ivec2 = require("ivec2")

local OBJECTIVE_STEAL_IT_BACK = "Steal it Back!"
local objective_crates_index = nil
local objective_miners_survive_index = nil
local objective_counter = 0

local CRATES = {
    {
        id = scenario.constants.CRATE1,
        cell = nil,
        gold_returned = 0
    },
    {
        id = scenario.constants.CRATE2,
        cell = nil,
        gold_returned = 0,
    },
    {
        id = scenario.constants.CRATE3,
        cell = nil,
        gold_returned = 0
    }
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
        objective_miners_survive_index = objectives.add_objective({
            objective = {
                description = "Your miners must survive"
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

    -- Handle match over
    if not is_match_over and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end

    -- Cache crate gold_held
    local crate = {}
    for index = 1,#CRATES do
        if not scenario.get_entity_by_id(CRATES[index].id, crate) then
            scenario.log("ERROR - Crate with ID ", CRATES[index].id, " does not exist.")
            error("Crate does not exist")
        end

        CRATES[index].gold_held = crate.gold_held
        CRATES[index].cell = crate.cell
    end


    -- Handle miners collecting gold
    local miner = {}
    for miner_index = 1,#MINERS do
        if is_match_over then
            break
        end

        if not scenario.get_entity_by_id(MINERS[miner_index].id, miner) then
            goto continue
        end
        if miner.health == 0 then
            is_match_over = true
            actions.run(function ()
                objectives.announce_objectives_failed()
                scenario.set_match_over_defeat()
            end)
            break
        end

        -- Handle miner return gold
        local miner_crate_index = MINERS[miner_index].gold_held_crate_index
        if miner_crate_index ~= nil and miner.gold_held == 0 then
            CRATES[miner_crate_index].gold_returned = CRATES[miner_crate_index].gold_returned + 1
            MINERS[miner_index].gold_held_crate_index = nil

            if CRATES[miner_crate_index].gold_returned == 4 then
                if objective_crates_index == nil or objective_miners_survive_index == nil then
                    error("Objectives index has not yet been assigned")
                end

                objective_counter = objective_counter + 1
                scenario.set_objective_variable_counter(objective_crates_index, objective_counter)
                if objective_counter == #CRATES then
                    scenario.complete_objective(objective_crates_index)
                    scenario.complete_objective(objective_miners_survive_index)
                end
            end
        end

        -- Handle miner picking up gold
        if miner_crate_index == nil and miner.gold_held ~= 0 then
            local nearest_crate_index = 1
            for index = 2,#CRATES do
                if ivec2.manhattan_distance(miner.cell, CRATES[index].cell) <
                        ivec2.manhattan_distance(miner.cell, CRATES[nearest_crate_index].cell) then
                    nearest_crate_index = index
                end
            end

            MINERS[miner_index].gold_held_crate_index = nearest_crate_index
        end

        ::continue::
    end

    actions.update()
end