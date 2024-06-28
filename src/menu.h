#pragma once

enum MenuMode {
    MENU_MODE_MAIN,
    MENU_MODE_JOIN_IP,
    MENU_MODE_JOIN_CONNECTING,
    MENU_MODE_LOBBY,
    MENU_MODE_MATCH_START
};

void menu_init();
MenuMode menu_get_mode();
void menu_update();
void menu_render();