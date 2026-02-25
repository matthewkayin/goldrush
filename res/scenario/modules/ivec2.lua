local ivec2 = {}

-- Returns the manhattan distance between two cells
-- @param cell_a ivec2
-- @param cell_b ivec2
-- @return number
function ivec2.manhattan_distance(cell_a, cell_b)
    return math.abs(cell_a.x - cell_b.x) + math.abs(cell_a.y - cell_b.y)
end

return ivec2