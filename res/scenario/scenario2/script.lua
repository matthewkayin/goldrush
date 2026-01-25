local actions = require("actions")

waow_time = 0.0
active_coroutines = {}

function wait(seconds)
    local resume_time = scenario.get_time() + seconds
    while scenario.get_time() < resume_time do
        coroutine.yield()
    end
end

function scenario_init()
    scenario.set_lose_on_buildings_destroyed(false)
    actions.add(function ()
        wait(5.0)
        scenario.chat(scenario.CHAT_COLOR_WHITE, "", "huzzah!")
        scenario.play_sound(scenario.SOUND_OBJECTIVE_COMPLETE)
    end)
end

function scenario_update()
    actions.update()
    if scenario.get_time() > waow_time then
        scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Waaow")
        scenario.play_sound(scenario.SOUND_UI_CLICK)
        waow_time = waow_time + 1.0
    end
end