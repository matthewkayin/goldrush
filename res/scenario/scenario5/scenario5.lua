local objectives = require("objectives")
local actions = require("actions")

local OBJECTIVE_PROTECT_VILLAGERS = "Protect the Villagers"

local VILLAGER_HALLS = {
    scenario.constants.VILLAGER_HALL1,
    scenario.constants.VILLAGER_HALL2,
    scenario.constants.VILLAGER_HALL3,
    scenario.constants.VILLAGER_HALL4
}

local BANDITS_PLAYER_ID = 2

function scenario_init()
    actions.run(function ()
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
        for index=1,#VILLAGER_HALLS do
            scenario.fog_reveal({
                cell = villager_hall_cells[index],
                cell_size = 4,
                sight = 13,
                duration = 7.0
            })
            actions.camera_pan(villager_hall_cells[index], 1.5)
            scenario.hold_camera()

            if index == 1 then
                scenario.chat("Bandits have been hitting these settlements hard.")
            elseif index == 2 then
                scenario.chat("If their towns are destroyed, the villagers will have nowhere else to go.")
            end
            actions.wait(3.0)
        end

        actions.camera_pan(previous_camera_cell, 1.5)
        actions.wait(1.0)

        objectives.announce_new_objective(OBJECTIVE_PROTECT_VILLAGERS)
        objectives.add_objective({
            objective = {
                description = "Destroy the bandit's base",
            },
            complete_fn = function ()
                return scenario.is_player_defeated(BANDITS_PLAYER_ID)
            end
        })
        objectives.add_objective({
            objective = {
                description = "The villagers must survive"
            }
        })
    end)
end

function scenario_update()
    objectives.update()

    actions.update()
end