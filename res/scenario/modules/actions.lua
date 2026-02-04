local actions = {}
actions.active_coroutines = {}

function actions.run(action)
    table.insert(actions.active_coroutines, coroutine.create(action))
end

function actions.update()
    local index = 1
    while index <= #actions.active_coroutines do
        local co = actions.active_coroutines[index]
        local success, error_message = coroutine.resume(co)

        if not success then
            error(debug.traceback(co, error_message))
        elseif coroutine.status(co) == "dead" then
            table.remove(actions.active_coroutines, index)
        else
            index = index + 1
        end
    end
end

function actions.wait(seconds)
    local resume_time = scenario.get_time() + seconds
    repeat
        coroutine.yield()
    until scenario.get_time() >= resume_time
end

function actions.camera_pan(cell, duration)
    scenario.begin_camera_pan(cell, duration)
    repeat
        coroutine.yield()
    until scenario.get_camera_mode() == scenario.CAMERA_MODE_FREE
end

function actions.hold_camera(duration)
    scenario.hold_camera()
    actions.wait(duration)
    scenario.release_camera()
end

return actions