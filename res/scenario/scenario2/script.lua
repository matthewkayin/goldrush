has_sent_message = false

function scenario_init()
    scenario.set_lose_on_buildings_destroyed(false)
    scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Hello Friends")
    scenario.add_objective_with_entity_counter("Hire 8 Miners", scenario.ENTITY_MINER, 8)
end

function scenario_update()
    if not has_sent_message and scenario.get_time() > 5.0 then
        scenario.chat(scenario.CHAT_COLOR_WHITE, "", "Waaow")
        scenario.play_sound(scenario.SOUND_OBJECTIVE_COMPLETE)
        has_sent_message = true
    end
end