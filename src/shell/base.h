#pragma once

#include "match/state.h"
#include "render/render.h"
#include "core/ui.h"
#include "menu/options_menu.h"
#include <string>

enum ShellSelectionType {
    SHELL_SELECTION_NONE,
    SHELL_SELECTION_UNITS,
    SHELL_SELECTION_ENEMY_UNIT,
    SHELL_SELECTION_BUILDINGS,
    SHELL_SELECTION_ENEMY_BUILDING,
    SHELL_SELECTION_GOLD
};

enum ShellMode {
    SHELL_MODE_NOT_STARTED,
    SHELL_MODE_NONE,
    SHELL_MODE_BUILD,
    SHELL_MODE_BUILD2,
    SHELL_MODE_BUILDING_PLACE,
    SHELL_MODE_TARGET_ATTACK,
    SHELL_MODE_TARGET_UNLOAD,
    SHELL_MODE_TARGET_REPAIR,
    SHELL_MODE_TARGET_MOLOTOV,
    SHELL_MODE_CHAT,
    SHELL_MODE_MENU,
    SHELL_MODE_MENU_SURRENDER,
    SHELL_MODE_MENU_SURRENDER_TO_DESKTOP,
    SHELL_MODE_MATCH_OVER_VICTORY,
    SHELL_MODE_MATCH_OVER_DEFEAT,
    SHELL_MODE_LEAVE_MATCH,
    SHELL_MODE_EXIT_PROGRAM
};

struct Alert {
    MinimapPixel pixel;
    ivec2 cell;
    int cell_size;
    uint32_t timer;
};

struct ChatMessage {
    std::string message;
    uint32_t timer;
    uint8_t player_id;
};

enum FireCellRender {
    FIRE_CELL_DO_NOT_RENDER,
    FIRE_CELL_RENDER_BELOW,
    FIRE_CELL_RENDER_ABOVE
};

class Shell {
public:
    Shell();

    void base_update();
    void render() const;
protected:
    virtual bool is_entity_visible(const Entity& entity) const = 0;
    virtual bool is_cell_rect_revealed(ivec2 cell, int cell_size) const = 0;

    virtual void handle_match_event(const MatchEvent& event) = 0;

    virtual std::vector<EntityId> create_selection(Rect select_rect) const = 0;
    virtual void on_selection() = 0;

    virtual bool is_surrender_required_to_leave() const = 0;
    virtual void leave_match(bool exit_program) = 0;

    void clamp_camera();
    void center_camera_on_cell(ivec2 cell);
    void show_status(const char* message);
    void set_selection(std::vector<EntityId>& selection);

    bool is_selecting() const;
    bool is_in_menu() const;
    bool is_in_submenu() const;
    bool is_targeting() const;

    ShellMode mode;
    MatchState match_state;
    UI ui;
    OptionsMenuState options_menu;
    bool is_paused;

    ivec2 camera_offset;
    bool is_minimap_dragging;
    ivec2 select_origin;
    uint32_t double_click_timer;

    std::vector<EntityId> selection;

    std::string status_message;
    uint32_t status_timer;

    std::string chat_message;
    std::vector<ChatMessage> chat;
    uint32_t chat_cursor_blink_timer;
    bool chat_cursor_visible;

    ivec2 latest_alert_cell;
    std::vector<Alert> alerts;

    Animation rally_flag_animation;
    Animation building_fire_animation;
    Animation move_animation;
    ivec2 move_animation_position;
    EntityId move_animation_entity_id;

    uint32_t sound_cooldown_timers[SOUND_COUNT];
    uint32_t displayed_gold_amounts[MAX_PLAYERS];
};

const char* shell_get_menu_header_text(ShellMode mode);
bool shell_is_mouse_in_ui();