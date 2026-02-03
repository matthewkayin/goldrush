local actions = require("actions")
local objectives = require("objectives")
local squad_util = require("squad_util")

local OBJECTIVE_MINE_GOLD = "Mine 10,000 Gold before your Opponent"

local ENEMY_PLAYER_ID = 1

local TARGET_GOLD_COUNT = 10000
local HARASS_TRIGGER_DISTANCE = 16

local BUILDER_MODE_ORDER_HALL = 0
local BUILDER_MODE_BUILDING_HALL = 1
local BUILDER_MODE_WAIT_GOLD_BUNKER = 2
local BUILDER_MODE_ORDER_BUNKER = 3
local BUILDER_MODE_BUILDING_BUNKER = 4
local BUILDER_MODE_RELEASED = 5

local has_given_bunker_hint = false
local has_sent_reactive_harass = false
local cached_bot_should_produce = false
local has_finished_opener = false
local builder_state1 = {}
local builder_state2 = {}

function scenario_init()
    builder_state1 = builder_state_init(scenario.constants.HALL_BUILDER1, scenario.constants.BUNKER_CELL1)
    builder_state2 = builder_state_init(scenario.constants.HALL_BUILDER2, scenario.constants.BUNKER_CELL2)
    set_bot_should_produce(false)

    actions.run(function ()
        actions.wait(1.0)
        local previous_camera_cell = scenario.get_camera_centered_cell()

        -- Camera pan 1
        scenario.fog_reveal({
            cell = scenario.constants.HALL_CELL1,
            cell_size = 4,
            sight = 13,
            duration = 7.0
        })
        actions.camera_pan(scenario.constants.HALL_CELL1, 1.5)
        scenario.hold_camera()
        scenario.play_sound(scenario.sound.UI_CLICK)
        scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Those greedy prospectors are taking all the gold!")
        actions.wait(1.0)
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.BUILD,
            building_type = scenario.entity_type.HALL,
            building_cell = scenario.constants.HALL_CELL1,
            entity_id = scenario.constants.HALL_BUILDER1
        })
        actions.wait(3.0)

        -- Camera pan 2
        scenario.fog_reveal({
            cell = scenario.constants.HALL_CELL2,
            cell_size = 4,
            sight = 13,
            duration = 6.0
        })
        actions.camera_pan(scenario.constants.HALL_CELL2, 1.5)
        scenario.hold_camera()
        scenario.queue_match_input({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.match_input_type.BUILD,
            building_type = scenario.entity_type.HALL,
            building_cell = scenario.constants.HALL_CELL2,
            entity_id = scenario.constants.HALL_BUILDER2
        })
        actions.wait(3.0)

        -- Camera return
        actions.camera_pan(previous_camera_cell, 1.5)
        scenario.release_camera()

        actions.wait(1.0)

        objectives.announce_new_objective(OBJECTIVE_MINE_GOLD)
        objectives.add_objective({
            objective = {
                description = "Mine 10,000 Gold",
            },
            complete_fn = function ()
                return scenario.player_get_gold_mined_total(scenario.PLAYER_ID) >= TARGET_GOLD_COUNT
            end
        })
        scenario.set_global_objective_counter(scenario.global_objective_counter_type.GOLD)

        actions.wait(3.0)
        -- Setting this flag triggers bot should produce to be true
        has_finished_opener = true

        actions.wait(5.0)
        scenario.hint("You can now build bunkers to defend your base.")
    end)
end

function scenario_update()
    objectives.update()

    if objectives.current_objective == OBJECTIVE_MINE_GOLD and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
    elseif scenario.player_get_gold_mined_total(ENEMY_PLAYER_ID) >= TARGET_GOLD_COUNT then
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
    end

    if not has_given_bunker_hint and scenario.player_get_entity_count(scenario.PLAYER_ID, scenario.entity_type.BUNKER) >= 1 then
        has_given_bunker_hint = true
        actions.run(function ()
            actions.wait(3.0)
            scenario.hint("Garrison cowboys inside your new bunker.")
        end)
    end

    -- Reactive harass
    if not has_sent_reactive_harass then
        local player_is_attacking_base1 = scenario.player_has_entity_near_cell(scenario.PLAYER_ID, scenario.constants.HALL_CELL1, HARASS_TRIGGER_DISTANCE)
        local player_is_attacking_base2 = scenario.player_has_entity_near_cell(scenario.PLAYER_ID, scenario.constants.HALL_CELL2, HARASS_TRIGGER_DISTANCE)
        if player_is_attacking_base1 or player_is_attacking_base2 then
            squad_util.spawn_harass_squad({
                player_id = ENEMY_PLAYER_ID,
                spawn_cell = scenario.constants.HARASS_SPAWN_CELL,
                target_cell = scenario.constants.HARASS_TARGET_CELL,
                entity_types = {
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.BANDIT,
                    scenario.entity_type.COWBOY,
                    scenario.entity_type.COWBOY
                }
            })

            has_sent_reactive_harass = true
        end

        if player_is_attacking_base1 then
            builder_state_release(builder_state1)
        end
        if player_is_attacking_base2 then
            builder_state_release(builder_state2)
        end
    end

    -- Builder state update
    builder_state_update(builder_state1)
    builder_state_update(builder_state2)

    -- Bot should produce
    if cached_bot_should_produce ~= should_bot_produce() then
        set_bot_should_produce(should_bot_produce())
    end

    actions.update()
end

function builder_state_init(builder_id, bunker_cell)
    scenario.bot_reserve_entity(ENEMY_PLAYER_ID, builder_id)
    return {
        builder_id = builder_id,
        bunker_cell = bunker_cell,
        mode = BUILDER_MODE_ORDER_HALL
    }
end

function builder_state_release(builder_state)
    scenario.bot_release_entity(ENEMY_PLAYER_ID, builder_state.builder_id)
    builder_state.mode = BUILDER_MODE_RELEASED
end

function builder_state_update(builder_state)
    if builder_state.mode == BUILDER_MODE_RELEASED then
        return
    end

    local builder = scenario.entity_get_info(builder_state.builder_id)
    if not builder or builder.health == 0 then
        builder_state_release(builder_state)
        scenario.log("BUILDER_STATE / Builder does not exist, builder_id:", builder_state.builder_id)
        return
    end

    if builder_state.mode == BUILDER_MODE_ORDER_HALL and builder.mode == scenario.entity_mode.UNIT_BUILD then
        builder_state.mode = BUILDER_MODE_BUILDING_HALL
        builder_state.hall_id = builder.target.id
        scenario.log("BUILDER_STATE / set mode to BUILDING_HALL, hall_id:", builder_state.hall_id)
        return
    end

    if builder_state.mode == BUILDER_MODE_BUILDING_HALL and builder.mode ~= scenario.entity_mode.UNIT_BUILD then
        local hall = scenario.entity_get_info(builder_state.hall_id)
        if not hall or hall.health == 0 then
            builder_state_release(builder_state)
            scenario.log("BUILDER_STATE / hall does not exist:", builder_state.hall_id)
            return
        end

        builder_state.mode = BUILDER_MODE_WAIT_GOLD_BUNKER
        actions.run(function ()
            while scenario.player_get_gold(ENEMY_PLAYER_ID) < scenario.entity_get_gold_cost(scenario.entity_type.BUNKER) do
                coroutine.yield()
            end
            scenario.queue_match_input({
                player_id = ENEMY_PLAYER_ID,
                type = scenario.match_input_type.BUILD,
                building_type = scenario.entity_type.BUNKER,
                building_cell = builder_state.bunker_cell,
                entity_id = builder_state.builder_id
            })
            builder_state.mode = BUILDER_MODE_ORDER_BUNKER
        end)
        scenario.log("BUILDER_STATE / ordered build bunker:", builder_state.builder_id)
        return
    end

    if builder_state.mode == BUILDER_MODE_ORDER_BUNKER and builder.mode == scenario.entity_mode.UNIT_BUILD then
        builder_state.mode = BUILDER_MODE_BUILDING_BUNKER
        builder_state.bunker_id = builder.target.id
        scenario.log("BUILDER_STATE / building bunker:", builder_state.bunker_id)
        return
    end

    if builder_state.mode == BUILDER_MODE_BUILDING_BUNKER and builder.mode ~= scenario.entity_mode.UNIT_BUILD then
        local bunker = scenario.entity_get_info(builder_state.bunker_id)
        if not bunker or bunker.health == 0 then
            builder_state_release(builder_state)
            scenario.log("BUILDER_STATE / bunker does not exist:", builder_state.bunker_id)
            return
        end

        -- TODO: garrison defense squad into bunker
        local squad_entity_ids = {}
        table.insert(squad_entity_ids, builder_state.bunker_id)
        for index=1,4 do
            local entity_cell = scenario.entity_find_spawn_cell(scenario.entity_type.COWBOY, bunker.cell)
            if not entity_cell then
                break
            end
            local entity_id = scenario.entity_create(scenario.entity_type.COWBOY, entity_cell, ENEMY_PLAYER_ID)
            table.insert(squad_entity_ids, entity_id)
        end
        scenario.bot_add_squad({
            player_id = ENEMY_PLAYER_ID,
            type = scenario.bot_squad_type.DEFEND,
            entities = squad_entity_ids
        })

        builder_state_release(builder_state)
        scenario.log("BUILDER_STATE / bunker finished:", builder_state.bunker_id)
    end
end

function set_bot_should_produce(value)
    cached_bot_should_produce = value
    scenario.bot_set_config_flag(ENEMY_PLAYER_ID, scenario.bot_config_flag.SHOULD_PRODUCE, value)
end

function should_bot_produce()
    return builder_state1.mode ~= BUILDER_MODE_WAIT_GOLD_BUNKER and
        builder_state2.mode ~= BUILDER_MODE_WAIT_GOLD_BUNKER and
        has_finished_opener
end