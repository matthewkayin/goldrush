local objectives = require("objectives")
local actions = require("actions")
local squad_util = require("squad_util")
local entity_util = require("entity_util")
local ivec2 = require("ivec2")

local OBJECTIVE_DEFEAT_BANDITS = "Destroy the Bandit's Base"

local ENEMY_PLAYER_ID = 1

local is_match_over = false
local has_sent_counterattack = false

function scenario_init()
    actions.run(function ()
        actions.wait(2.0)

        scenario.chat("Scouts report that there's a bandit camp to the north.")
        actions.wait(2.0)
        scenario.chat("The sheriff wants you to take them out.")
        actions.wait(2.0)

        objectives.announce_new_objective(OBJECTIVE_DEFEAT_BANDITS)
        objectives.add_objective({
            objective = {
                description = OBJECTIVE_DEFEAT_BANDITS
            },
            complete_fn = function ()
                return false
            end
        })

        actions.wait(15.0)
        scenario.hint("You can now build bunkers to defend your base.")

        actions.wait(60.0)

        local goldmine = {}
        if not scenario.get_entity_by_id(scenario.constants.UNCLAIMED_GOLDMINE, goldmine) then
            error("Could not get unclaimed goldmine info")
        end
        scenario.fog_reveal({
            cell = goldmine.cell,
            cell_size = 3,
            sight = 4,
            duration = 5.0
        })
        scenario.highlight_entity(scenario.constants.UNCLAIMED_GOLDMINE)
        scenario.chat("Looks like there's an unclaimed gold mine out there.")
    end)
end

function scenario_update()
    objectives.update()

    -- Handle bandits defeated
    if not is_match_over and scenario.is_player_defeated(ENEMY_PLAYER_ID) then
        actions.run(function ()
            actions.wait(1.0)
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end

    -- Create counterattack the first time they move out on enemy
    if not has_sent_counterattack and player_has_entity_near_counterattack_triggers() then
        spawn_counterattack_squad()
        has_sent_counterattack = true
    end

    actions.update()
end

function player_has_entity_near_counterattack_triggers()
    local entity_near_cell_id = entity_util.find_entity(function (entity)
        return entity.player_id == scenario.PLAYER_ID and
            (ivec2.manhattan_distance(entity.cell, scenario.constants.COUNTERATTACK_TRIGGER1) <= 4 or
            ivec2.manhattan_distance(entity.cell, scenario.constants.COUNTERATTACK_TRIGGER2) <= 4 or
            ivec2.manhattan_distance(entity.cell, scenario.constants.COUNTERATTACK_TRIGGER3) <= 4)
    end)
    return entity_near_cell_id ~= nil
end

function spawn_counterattack_squad()
    local harass_spawn_roll = math.random(0, 1)
    local harass_spawn_cell
    if harass_spawn_roll == 0 then
        harass_spawn_cell = scenario.constants.HARASS1_SPAWN1
    else
        harass_spawn_cell = scenario.constants.HARASS1_SPAWN2
    end

    squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = harass_spawn_cell,
        target_cell = scenario.constants.HARASS1_TARGET,
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY
        }
    })
end

function spawn_harass_squad()
    local hall_id = scenario.get_hall_surrounding_goldmine(scenario.constants.UNCLAIMED_GOLDMINE)
    local hall = {}
    local player_controls_harass2 =
        hall_id ~= nil and
        scenario.get_entity_by_id(hall_id, hall) and
        hall.player_id == scenario.PLAYER_ID

    local harass_number = 1
    if player_controls_harass2 then
        harass_number = math.random(1, 2)
    end

    local harass_spawns
    local harass_target_cell
    if harass_number == 1 then
        harass_target_cell = scenario.constants.HARASS1_TARGET
        harass_spawns = { scenario.constants.HARASS1_SPAWN1, scenario.constants.HARASS1_SPAWN2 }
    else
        harass_target_cell = scenario.constants.HARASS2_TARGET
        harass_spawns = { scenario.constants.HARASS2_SPAWN1, scenario.constants.HARASS2_SPAWN2 }
    end

    local harass_spawn_index = math.random(1, 2)
    local harass_spawn_cell = harass_spawns[harass_spawn_index]

    local harass_squad_size = math.random(3, 4)
    local harass_squad_entity_types = {}
    for i=1,harass_squad_size do
        local harass_entity_type_roll = math.random(0, 1)
        local harass_entity_type
        if harass_entity_type_roll == 0 then
            harass_entity_type = scenario.entity_type.BANDIT
        else
            harass_entity_type = scenario.entity_type.COWBOY
        end
        table.insert(harass_squad_entity_types, harass_entity_type)
    end
    squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = harass_spawn_cell,
        target_cell = harass_target_cell,
        entity_types = harass_squad_entity_types
    })
end