local ivec2 = require("ivec2")

local entity_util = {}

-- Returns a list of the IDs of entities which match the passed-in filter function
-- @param filter function
-- @return table
function entity_util.find_entities(filter)
    local entity_list = {}
    local entity = {}
    for entity_index=0,(scenario.get_entity_count() - 1) do
        scenario.get_entity_by_index(entity_index, entity)
        if filter(entity) then
            table.insert(entity_list, scenario.get_entity_id_of(entity_index))
        end
    end

    return entity_list
end

-- Returns the ID of the first entity which matches the passed-in filter function
-- @param filter function
-- @return number | nil
function entity_util.find_entity(filter)
    local entity = {}
    for entity_index = 0,(scenario.get_entity_count() - 1) do
        scenario.get_entity_by_index(entity_index, entity)
        if filter(entity) then
            return scenario.get_entity_id_of(entity_index)
        end
    end

    return nil
end

-- Returns true if a player owned entity is near the cell within the specified distance
-- @param cell ivec2
-- @param distance number
-- @return boolean
function entity_util.player_has_entity_near_cell(cell, distance)
    local entity_near_cell_id = entity_util.find_entity(function (entity)
        return ivec2.manhattan_distance(entity.cell, cell) <= distance
    end)
    return entity_near_cell_id ~= nil
end

return entity_util