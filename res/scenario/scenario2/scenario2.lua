local actions = require("actions")
local objectives = require("objectives")
local squad_util = require("squad_util")

local OBJECTIVE_MINE_GOLD = "Mine 5000 Gold before your Opponent"

local ENEMY_PLAYER_ID = 1

local TARGET_GOLD_COUNT = 5000
local HARASS_TRIGGER_DISTANCE = 16
local REGULAR_HARASSMENT_INTERVAL = 60.0 * 5.0

local BUILDER_MODE_ORDER_HALL = 0
local BUILDER_MODE_BUILDING_HALL = 1
local BUILDER_MODE_WAIT_GOLD_BUNKER = 2
local BUILDER_MODE_ORDER_BUNKER = 3
local BUILDER_MODE_BUILDING_BUNKER = 4
local BUILDER_MODE_RELEASED = 5

local has_sent_reactive_harass = false
local cached_bot_should_produce = false
local has_finished_opener = false
local is_match_over = false
local builder_state1 = {}
local builder_state2 = {}
local harassable_goldmines = {
    {
        goldmine_id = scenario.constants.GOLDMINE2,
        spawn_cell = scenario.constants.HARASS_SPAWN_CELL2,
        has_harassed_player_at_location = false
    },
    {
        goldmine_id = scenario.constants.GOLDMINE3,
        spawn_cell = scenario.constants.HARASS_SPAWN_CELL3,
        has_harassed_player_at_location = false
    },
    {
        goldmine_id = scenario.constants.GOLDMINE4,
        spawn_cell = scenario.constants.HARASS_SPAWN_CELL4,
        has_harassed_player_at_location = false
    }
}

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
                description = "Mine 5000 Gold",
            },
            complete_fn = function ()
                return scenario.player_get_gold_mined_total(scenario.PLAYER_ID) >= TARGET_GOLD_COUNT
            end
        })
        scenario.set_global_objective_counter(scenario.global_objective_counter_type.GOLD, TARGET_GOLD_COUNT)

        actions.wait(3.0)
        -- Setting this flag triggers bot should produce to be true
        has_finished_opener = true

        actions.wait(5.0)
        scenario.hint("You can now build bunkers to defend your base.")
    end)
end

function scenario_update()
    objectives.update()

    if not is_match_over and objectives.current_objective == OBJECTIVE_MINE_GOLD and scenario.are_objectives_complete() then
        actions.run(function ()
            objectives.announce_objectives_complete()
            scenario.set_match_over_victory()
        end)
        is_match_over = true
    end
    if not is_match_over and scenario.player_get_gold_mined_total(ENEMY_PLAYER_ID) >= TARGET_GOLD_COUNT then
        actions.run(function ()
            objectives.announce_objectives_failed()
            scenario.set_match_over_defeat()
        end)
        is_match_over = true
    end

    -- Reactive harass
    if not has_sent_reactive_harass then
        local player_is_attacking_base1 = scenario.player_has_entity_near_cell(scenario.PLAYER_ID, scenario.constants.HALL_CELL1, HARASS_TRIGGER_DISTANCE)
        local player_is_attacking_base2 = scenario.player_has_entity_near_cell(scenario.PLAYER_ID, scenario.constants.HALL_CELL2, HARASS_TRIGGER_DISTANCE)
        if player_is_attacking_base1 or player_is_attacking_base2 then
            local params = {}
            if player_is_attacking_base1 then
                params.spawn_cell = scenario.constants.HARASS_SPAWN_CELL1_1
            else
                params.spawn_cell = scenario.constants.HARASS_SPAWN_CELL1_2
            end
            params.goldmine_id = scenario.constants.GOLDMINE1
            harass_goldmine(params)
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

    -- Bot should harass
    for harass_index=1,#harassable_goldmines do
        if not harassable_goldmines[harass_index].has_harassed_player_at_location and
                scenario.entity_get_player_who_controls_goldmine(harassable_goldmines[harass_index].goldmine_id) == scenario.PLAYER_ID then
            actions.run(function ()
                actions.wait(60.0 * 1.0)
                harass_goldmine(harassable_goldmines[harass_index])
            end)
            harassable_goldmines[harass_index].has_harassed_player_at_location = true
        end
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
        return
    end

    if builder_state.mode == BUILDER_MODE_ORDER_HALL and builder.mode == scenario.entity_mode.UNIT_BUILD then
        builder_state.mode = BUILDER_MODE_BUILDING_HALL
        builder_state.hall_id = builder.target.id
        return
    end

    if builder_state.mode == BUILDER_MODE_BUILDING_HALL and builder.mode ~= scenario.entity_mode.UNIT_BUILD then
        local hall = scenario.entity_get_info(builder_state.hall_id)
        if not hall or hall.health == 0 then
            builder_state_release(builder_state)
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
        return
    end

    if builder_state.mode == BUILDER_MODE_ORDER_BUNKER and builder.mode == scenario.entity_mode.UNIT_BUILD then
        builder_state.mode = BUILDER_MODE_BUILDING_BUNKER
        builder_state.bunker_id = builder.target.id
        return
    end

    if builder_state.mode == BUILDER_MODE_BUILDING_BUNKER and builder.mode ~= scenario.entity_mode.UNIT_BUILD then
        local bunker = scenario.entity_get_info(builder_state.bunker_id)
        if not bunker or bunker.health == 0 then
            builder_state_release(builder_state)
            return
        end

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
            target_cell = builder.cell,
            entities = squad_entity_ids
        })

        builder_state_release(builder_state)
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

function harass_goldmine(params)
    local goldmine = scenario.entity_get_info(params.goldmine_id)
    if not goldmine then
        error("Goldmine constant does not point to a valid entity")
    end
    squad_util.spawn_harass_squad({
        player_id = ENEMY_PLAYER_ID,
        spawn_cell = params.spawn_cell,
        target_cell = goldmine.cell,
        entity_types = {
            scenario.entity_type.BANDIT,
            scenario.entity_type.BANDIT,
            scenario.entity_type.COWBOY,
            scenario.entity_type.COWBOY
        }
    })
end