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

-- BANDIT_SPAWN_LAKE
-- BANDIT_SPAWN_BEAN
-- BANDIT_SPAWN_CLIFF
-- BANDIT_SPAWN_STAIRS
-- BANDIT_SPAWN_WOODS
local ATTACK_SPAWN_CELLS = {
    -- Southern villager
    {
        scenario.constants.BANDIT_SPAWN_LAKE,
        scenario.constants.BANDIT_SPAWN_CLIFF,
        scenario.constants.BANDIT_SPAWN_WOODS
    },
    -- Northwest villager
    {
        scenario.constants.BANDIT_SPAWN_STAIRS,
        scenario.constants.BANDIT_SPAWN_BEAN,
        scenario.constants.BANDIT_SPAWN_WOODS
    },
    -- Northeast villager
    {
        scenario.constants.BANDIT_SPAWN_BEAN,
        scenario.constants.BANDIT_SPAWN_WOODS
    },
    -- Player
    {
        scenario.constants.BANDIT_SPAWN_CLIFF,
        scenario.constants.BANDIT_SPAWN_WOODS
    }
}

local VILLAGERS_PLAYER_ID = 1
local BANDITS_PLAYER_ID = 2

local ATTACK_INTERVAL = 180
local ATTACK_TARGET_PLAYER = #VILLAGER_HALLS + 1

local is_match_over = false
local next_attack_time

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
                duration = 6.0
            })
            actions.camera_pan(villager_hall_cells[index], camera_pan_durations[index])
            scenario.hold_camera()

            -- Slight pause before sending message
            actions.wait(0.2)

            if index == 1 then
                scenario.chat("Bandits have been hitting these settlements hard.")
            elseif index == 2 then
                scenario.chat("If their towns are destroyed, the villagers will have nowhere else to go.")
            elseif index == 3 then
                -- Announce new objective
                scenario.play_sound(scenario.sound.UI_CLICK)
                scenario.chat_prefixed(scenario.CHAT_COLOR_GOLD, "New Objective:", OBJECTIVE_PROTECT_VILLAGERS)
                objectives.current_objective = OBJECTIVE_PROTECT_VILLAGERS
            end
            actions.wait(1.8)
        end

        actions.camera_pan(previous_camera_cell, 1.5)
        actions.wait(1.0)

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

        actions.wait(5.0)
        scenario.hint("You can how hire Wagons, which are fast transport units.")

        actions.wait(60)

        -- Harass each villager
        local spawn_cells = {
            scenario.constants.BANDIT_SPAWN_WOODS,
            scenario.constants.BANDIT_SPAWN_STAIRS,
            scenario.constants.BANDIT_SPAWN_BEAN
        }
        for villager_index=1,#VILLAGER_HALLS do
            squad_util.spawn_harass_squad({
                player_id = BANDITS_PLAYER_ID,
                target_cell = villager_hall_cells[villager_index],
                spawn_cell = spawn_cells[villager_index],
                entity_types = {
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT
                }
            })
        end

        actions.wait(ATTACK_INTERVAL)

        -- Harass player
        if scenario.get_entity_by_id(scenario.constants.PLAYER_HALL, hall) then
            squad_util.spawn_harass_squad({
                player_id = BANDITS_PLAYER_ID,
                target_cell = hall.cell,
                spawn_cell = scenario.constants.BANDIT_SPAWN_CLIFF,
                entity_types = {
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT
                }
            })
        else
            -- This is only a warning since the player could have destroyed their own hall and the game shouldn't crash in that instance
            -- But we still want this to warn us in case the hall ID isn't set properly
            scenario.log("WARN - Player hall ID not found")
        end

        next_attack_time = scenario.get_time() + ATTACK_INTERVAL
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

    -- Spawn harass squads
    if not is_match_over and next_attack_time ~= nil and scenario.get_time() >= next_attack_time then
        -- Determine attack target indexes
        local target_index1 = math.random(1, 4)
        local target_index2 = math.random(1, 4)
        if target_index2 == target_index1 then
            if target_index2 == 4 then
                target_index2 = 1
            else
                target_index2 = target_index2 + 1
            end
        end

        spawn_harass_squad(target_index1)
        spawn_harass_squad(target_index2)
        next_attack_time = next_attack_time + ATTACK_INTERVAL
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

function spawn_harass_squad(target_index)
    -- Determine spawn location
    local spawn_roll = math.random(1, #ATTACK_SPAWN_CELLS[target_index])
    local spawn_cell = ATTACK_SPAWN_CELLS[target_index][spawn_roll]

    -- Determine hall ID
    local hall_id
    if target_index == ATTACK_TARGET_PLAYER then
        hall_id = scenario.constants.PLAYER_HALL
    else
        hall_id = VILLAGER_HALLS[target_index]
    end

    -- Determine attack target based on hall
    local hall = {}
    if not scenario.get_entity_by_id(hall_id, hall) then
        scenario.log("Getting hall ", hall_id, " by ID failed in spawn_harass_squad().")
        return
    end
    local target_cell = hall.cell

    local ATTACK_TYPE_BANDITS = 1
    local ATTACK_TYPE_SAPPER = 2
    local ATTACK_TYPE_PYRO = 3

    local attack_type_roll = math.random()
    local entity_types
    if attack_type_roll < 0.6 then
        attack_type = ATTACK_TYPE_BANDITS
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY
        }
    elseif attack_type_roll < 0.9 then
        attack_type = ATTACK_TYPE_SAPPER
        entity_types = {
            scenario.entity_type.SAPPER,
            scenario.entity_type.SAPPER,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY
        }
    else
        attack_type = ATTACK_TYPE_PYRO
        entity_types = {
            scenario.entity_type.PYRO,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY
        }
    end

    -- Spawn harass squad
    squad_util.spawn_harass_squad({
        player_id = BANDITS_PLAYER_ID,
        target_cell = target_cell,
        spawn_cell = spawn_cell,
        entity_types = entity_types
    })
end