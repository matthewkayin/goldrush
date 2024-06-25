#pragma once

#include <memory>

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_JOIN_IP,
    MENU_MODE_JOIN_CONNECTING,
    MENU_MODE_LOBBY
};

struct menu_state_t;
struct menu_t {
    std::unique_ptr<menu_state_t> state;
    menu_t();
    ~menu_t();
    void show_status(const char* message);
    void set_mode(MenuMode mode);
    void update();
    void render() const;
};