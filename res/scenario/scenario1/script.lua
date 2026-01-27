local actions = require("actions")
local common = require("common")

local OBJECTIVE_FIND_GOLDMINE = "Find a Goldmine"
local OBJECTIVE_BUILD_HALL = "Build a Town Hall"
local OBJECTIVE_ESTABLISH_BASE = "Establish a Base"

function scenario_init()
    scenario.set_lose_on_buildings_destroyed(false)

    actions.run(function()
        actions.wait(2.0)
        common.announce_new_objective(OBJECTIVE_FIND_GOLDMINE)
        common.add_objective({
            objective = {
                description = "Find a Goldmine"
            }, 
            complete_fn = function()
                if scenario.entity_is_visible_to_player(scenario.constants.FIRST_GOLDMINE) then
                    scenario.highlight_entity(scenario.constants.FIRST_GOLDMINE)
                    return true
                end
                return false
            end
        })
        actions.wait(5.0)
        scenario.hint("Wagons can be used to transport units.")
    end)
end

function scenario_update()
    common.update_objectives()

    if common.current_objective == OBJECTIVE_FIND_GOLDMINE and scenario.are_objectives_complete() then
        actions.run(function()
            common.announce_objectives_complete()
            common.announce_new_objective("Build a Town Hall")
            common.add_objective({
                objective = {
                    description = "Build a Town Hall",
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(0, entity_type.HALL) >= 1
                end
            })
            actions.wait(5.0)
            scenario.hint("Multiple miners can work together to build buildings more quickly.")
        end)
    elseif common.current_objective == OBJECTIVE_BUILD_HALL and scenario.are_objectives_complete() then
        actions.run(function()
            common.announce_objectives_complete()
            common.announce_new_objective("Establish a Base")
            common.add_objective({
                objective = {
                    description = "Hire 8 Miners",
                    entity_type = scenario.ENTITY_MINER,
                    counter_target = 8
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(0, scenario.ENTITY_MINER) >= 8
                end
            })
            common.add_objective({
                objective = {
                    description = "Hire 4 Cowboys",
                    entity_type = scenario.ENTITY_COWBOY,
                    counter_target = 4
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(0, scenario.ENTITY_COWBOY) >= 4
                end
            })
            common.add_objective({
                objective = {
                    description = "Build a Bunker"
                },
                complete_fn = function()
                    return scenario.get_player_entity_count(0, scenario.ENTITY_BUNKER) >= 1
                end
            })
            common.add_objective({
                objective = {
                    description = "Garrison into the Bunker"
                },
                complete_fn = function()
                    return scenario.get_player_full_bunker_count() >= 1
                end
            })
        end)
    elseif common.current_objective == OBJECTIVE_ESTABLISH_BASE and scenario.are_objectives_complete() then
        actions.run(function()
            common.announce_objectives_complete()
        end)
    end

    -- This is placed on the bottom so that any actions can begin running on the frame they are kicked off
    actions.update()
end