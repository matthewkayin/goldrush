local squad_util = {}

---@param params { player_id: number, spawn_cell: ivec2, target_cell: ivec2, entity_types: table }
---@return number
function squad_util.spawn_harass_squad(params)
    local entity_ids = {}

    local entity_cells = scenario.find_entity_spawn_cells(params.spawn_cell, params.entity_types)
    for index=1,#params.entity_types do
        if entity_cells[index] == nil then
            break
        end

        local entity_id = scenario.create_entity(params.entity_types[index], entity_cells[index], params.player_id)
        table.insert(entity_ids, entity_id)
    end

    local squad_id = scenario.bot_add_squad({
        player_id = params.player_id,
        type = scenario.bot_squad_type.ATTACK,
        target_cell = params.target_cell,
        entity_list = entity_ids
    })

    scenario.queue_match_input({
        player_id = params.player_id,
        type = scenario.match_input_type.MOVE_ATTACK_CELL,
        target_cell = params.target_cell,
        entity_ids = entity_ids
    })

    return squad_id
end

return squad_util