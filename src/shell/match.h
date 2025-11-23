#pragma once

#include "shell/base.h"

#ifdef GOLD_DEBUG
    enum DebugFog {
        DEBUG_FOG_ENABLED,
        DEBUG_FOG_BOT_VISION,
        DEBUG_FOG_DISABLED
    };
#endif

class MatchShell : public Shell {
public:
    MatchShell();
protected:
    bool is_entity_visible(const Entity& entity) const override;
    bool is_cell_rect_revealed(ivec2 cell, int cell_size) const override;

    void handle_match_event(const MatchEvent& event) override;

    std::vector<EntityId> create_selection(Rect select_rect) const override;
    void on_selection() override;

    bool is_surrender_required_to_leave() const override;
    void leave_match(bool exit_program) override;
private:
    #ifdef GOLD_DEBUG
        DebugFog debug_fog;
    #endif
};