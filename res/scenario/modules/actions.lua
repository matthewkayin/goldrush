local actions = {}
actions.active_coroutines = {}

function actions.add(action)
    table.insert(actions.active_coroutines, coroutine.create(action))
end

function actions.update()
    local index = 1
    while index <= #actions.active_coroutines do
        local co = actions.active_coroutines[index]
        local success, error_message = coroutine.resume(co)

        if not success then
            error(error_message)
        elseif coroutine.status(co) == "dead" then
            table.remove(actions.active_coroutines, index)
        else
            index = index + 1
        end
    end
end

return actions