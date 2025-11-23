#pragma once

#include "shell/base.h"

struct ReplayLoadingThreadState {
    MatchState match_state;
    uint32_t turn_counter;
};

class ReplayShell : public Shell {
public:
    ReplayShell();
protected:
    bool is_entity_visible(const Entity& entity) const override;
    bool is_cell_rect_revealed(ivec2 cell, int cell_size) const override;

    void handle_match_event(const MatchEvent& event) override;

    std::vector<EntityId> create_selection(Rect select_rect) const override;
    void on_selection() override;

    bool is_surrender_required_to_leave() const override;
    void leave_match(bool exit_program) override;
private:
    uint32_t replay_fog_index;
    std::vector<std::string> replay_fog_texts;
    std::vector<uint8_t> replay_fog_player_ids;

    std::vector<MatchState> replay_checkpoints;
    std::vector<std::vector<MatchInput>> replay_inputs[MAX_PLAYERS];
    std::vector<ReplayChatMessage> replay_chatlog;

    SDL_Thread* replay_loading_thread;
    MatchState replay_loading_match_state;

    SDL_Mutex* replay_loading_turn_counter_mutex;
    uint32_t replay_loading_turn_counter;
    uint32_t replay_loaded_turn_counter;

    SDL_Mutex* replay_loading_early_exit_mutex;
    bool replay_loading_early_exit;
};