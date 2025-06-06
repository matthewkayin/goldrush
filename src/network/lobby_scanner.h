#pragma once

#include "types.h"
#include <enet/enet.h>
#include <steam/steam_api.h>
#include <vector>

class NetworkLobbyScanner {
public:
    virtual ~NetworkLobbyScanner() = default;
    virtual bool was_create_successful() const = 0;
    virtual void search(const char* query) = 0;
    virtual void service() = 0;

    size_t get_matchlist_size() const;
    const NetworkMatchlistEntry& get_matchlist_entry(size_t index) const;
protected:
    std::vector<NetworkMatchlistEntry> matchlist;
};

class NetworkLanLobbyScanner : public NetworkLobbyScanner {
public:
    NetworkLanLobbyScanner();
    ~NetworkLanLobbyScanner();
    bool was_create_successful() const override;
    void search(const char* query) override;
    void service() override;
private:
    ENetSocket scanner;
    char lobby_name_query[NETWORK_LOBBY_NAME_BUFFER_SIZE];
};

class NetworkSteamLobbyScanner : public NetworkLobbyScanner {
public:
    bool was_create_successful() const override;
    void search(const char* query) override;
    void service() override;
private:
    CCallResult<NetworkSteamLobbyScanner, LobbyMatchList_t> call_result_lobby_matchlist;
    void on_lobby_matchlist(LobbyMatchList_t* lobby_matchlist, bool io_failure);
};