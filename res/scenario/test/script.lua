local is_match_over = false

function scenario_init()
end

function scenario_update()
    if not is_match_over and scenario.is_player_defeated(1) then
        is_match_over = true
        scenario.set_match_over_victory()
    end

    if not is_match_over and (is_bandit_dead(scenario.constants.BANDIT1) or is_bandit_dead(scenario.constants.BANDIT2)) then
        is_match_over = true
        scenario.set_match_over_defeat()
    end
end

function is_bandit_dead(bandit_id)
    local bandit = {}
    if not scenario.get_entity_by_id(bandit_id, bandit) then
        return true
    end
    return bandit.health == 0
end