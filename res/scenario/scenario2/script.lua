has_sent_message = false

function scenario_init()
    scenario.set_lose_on_buildings_destroyed(false)
    scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Hello Friends")
end

function scenario_update()
    if not has_sent_message and scenario.get_time() > 5.0 then
        scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Waaow")
        has_sent_message = true
    end
end