local objectives = require("objectives")
local actions = require("actions")
local squad_util = require("squad_util")

local OBJECTIVE_PROTECT_VILLAGERS = "Protect the Villagers"

local VILLAGER_HALLS = {
    scenario.constants.VILLAGER_HALL1,
    scenario.constants.VILLAGER_HALL2,
    scenario.constants.VILLAGER_HALL3
}
local VILLAGER_MINES = {
    scenario.constants.VILLAGER_MINE1,
    scenario.constants.VILLAGER_MINE2,
    scenario.constants.VILLAGER_MINE3
}
local VILLAGER_MINERS = {
    {
        scenario.constants.VILLAGER1_MINER1,
        scenario.constants.VILLAGER1_MINER2,
        scenario.constants.VILLAGER1_MINER3
    },
    {
        scenario.constants.VILLAGER2_MINER1,
        scenario.constants.VILLAGER2_MINER2,
        scenario.constants.VILLAGER2_MINER3
    },
    {
        scenario.constants.VILLAGER3_MINER1,
        scenario.constants.VILLAGER3_MINER2,
        scenario.constants.VILLAGER3_MINER3
    }
}

local VILLAGERS_PLAYER_ID = 1
local BANDITS_PLAYER_ID = 2

local is_match_over = false

function scenario_init()
    actions.run(function ()
        scenario.bot_set_config_flag(VILLAGERS_PLAYER_ID, scenario.bot_config_flag.SHOULD_PRODUCE, false)

        -- Send villager miners to gold
        for index=1,#VILLAGER_HALLS do
            scenario.queue_match_input({
                player_id = VILLAGERS_PLAYER_ID,
                type = scenario.match_input_type.MOVE_ENTITY,
                target_id = VILLAGER_MINES[index],
                entity_ids = VILLAGER_MINERS[index]
            })
        end

        actions.wait(1.0)
        local previous_camera_cell = scenario.get_camera_centered_cell()

        -- Determine camera pan cells
        local villager_hall_cells = {}
        local hall = {}
        for index=1,#VILLAGER_HALLS do
            local hall_id = VILLAGER_HALLS[index]
            if not scenario.get_entity_by_id(hall_id, hall) then
                scenario.log("Villager hall ", index, " with ID ", hall_id, " not found.")
                error("Villager hall not found")
            end

            table.insert(villager_hall_cells, hall.cell)
        end

        -- Camera pan
        local camera_pan_durations = {
            1.5, 2.0, 1.5
        }
        for index=1,#VILLAGER_HALLS do
            scenario.fog_reveal({
                cell = villager_hall_cells[index],
                cell_size = 4,
                sight = 13,
                duration = 7.0
            })
            actions.camera_pan(villager_hall_cells[index], camera_pan_durations[index])
            scenario.hold_camera()

            if index == 1 then
                scenario.chat("Bandits have been hitting these settlements hard.")
            elseif index == 2 then
                scenario.chat("If their towns are destroyed, the villagers will have nowhere else to go.")
            end
            actions.wait(2.0)
        end

        actions.camera_pan(previous_camera_cell, 1.5)
        actions.wait(1.0)

        objectives.announce_new_objective(OBJECTIVE_PROTECT_VILLAGERS)
        objectives.add_objective({
            objective = {
                description = "Destroy the bandit's base",
            },
            complete_fn = function ()
                return false
            end
        })
        objectives.add_objective({
            objective = {
                description = "The villagers must survive"
            },
            complete_fn = function ()
                return false
            end
        })

        scenario.bot_set_config_flag(VILLAGERS_PLAYER_ID, scenario.bot_config_flag.SHOULD_PRODUCE, true)

        actions.wait(10.0)
        scenario.hint("You can how hire Wagons, which are fast transport units.")

        spawn_harass_squad()
    end)
end

function scenario_update()
    objectives.update()

    -- Check for defeat
    if not is_match_over and a_villager_hall_has_been_defeated() then
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
        is_match_over = true
    end

    -- Check for victory
    if not is_match_over and scenario.is_player_defeated(BANDITS_PLAYER_ID) then
        actions.run(function ()
            scenario.complete_objective(0)
            scenario.complete_objective(1)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end

    actions.update()
end

function a_villager_hall_has_been_defeated()
    local hall = {}
    for index=1,#VILLAGER_HALLS do
        if not scenario.get_entity_by_id(VILLAGER_HALLS[index], hall) then
            return true
        end
        if hall.health == 0 then
            return true
        end
    end

    return false
end

function spawn_harass_squad()
    local hall_id = VILLAGER_HALLS[1]
    local hall = {}
    if not scenario.get_entity_by_id(hall_id, hall) then
        scenario.log("Getting hall ", hall_id, " by ID failed in spawn_harass_squad().")
        return
    end

    squad_util.spawn_harass_squad({
        player_id = BANDITS_PLAYER_ID,
        target_cell = hall.cell,
        spawn_cell = scenario.constants.BANDIT_SPAWN_WOODS,
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT
        }
    })
end