local actions = require("actions")
local common = require("common")

local STATE_FIND_GOLDMINE = 0
local STATE_ESTABLISH_BASE = 1

local state

function scenario_init()
    scenario.set_lose_on_buildings_destroyed(false)

    actions.run(function()
        actions.wait(2.0)
        common.announce_new_objective("Find a Goldmine")
        common.add_objective(
            {
                description = "Find a Goldmine"
            }, 
            function()
                if scenario.entity_is_visible_to_player(scenario.constants.FIRST_GOLDMINE) then
                    scenario.highlight_entity(scenario.constants.FIRST_GOLDMINE)
                    return true
                end
                return false
            end
        )
        actions.wait(5.0)
        scenario.hint("Wagons can be used to transport units.")
    end)
end

function scenario_update()
    common.update_objectives()

    if state == STATE_FIND_GOLDMINE then
        if scenario.are_objectives_complete() then
            actions.run(function()
                common.announce_objectives_complete()
                common.announce_new_objective("Establish a Base")
                common.add_objective(
                    {
                        description = "Hire 8 Miners",
                        entity_type = scenario.ENTITY_MINER,
                        counter_target = 8
                    },
                    function()
                        return scenario.get_player_entity_count(0, scenario.ENTITY_MINER) >= 8
                    end
                )
                common.add_objective(
                    {
                        description = "Hire 4 Cowboys",
                        entity_type = scenario.ENTITY_COWBOY,
                        counter_target = 4
                    },
                    function()
                        return scenario.get_player_entity_count(0, scenario.ENTITY_COWBOY) >= 4
                    end
                )
                common.add_objective(
                    {
                        description = "Build a Bunker"
                    },
                    function()
                        return scenario.get_player_entity_count(0, scenario.ENTITY_BUNKER) >= 1
                    end
                )
                common.add_objective(
                    {
                        description = "Garrison into the Bunker"
                    },
                    function()
                        return scenario.get_player_full_bunker_count() >= 1
                    end
                )
            end)
            state = STATE_ESTABLISH_BASE
        end
    elseif state == STATE_ESTABLISH_BASE then
        if scenario.are_objectives_complete() then
            common.announce_objectives_complete()
        end
    end

    -- This is placed on the bottom so that any actions can begin running on the frame they are kicked off
    actions.update()
end