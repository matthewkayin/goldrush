#pragma once

#include "defines.h"
#include "menu/menu.h"
#include "shell/shell.h"
#include "network/network.h"

class IGameState {
public:
    virtual void handle_network_event(const NetworkEvent& event) = 0;
    virtual void update() = 0;
    virtual void render() = 0;
};

class GameStateMenu : public IGameState {
public:
    static GameStateMenu* init();
    ~GameStateMenu();
    void handle_network_event(const NetworkEvent& event) override;
    void update() override;
    void render() override;
private:
    MenuState* menu_state;
};

class GameStateMatch : public IGameState {
public:
    static GameStateMatch* init_match();
    static GameStateMatch* init_replay();
    void handle_network_event(const NetworkEvent& event) override;
    void update() override;
    void render() override;
private:
    MatchShellState* match_shell_state;
};