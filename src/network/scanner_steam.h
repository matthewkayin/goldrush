#pragma once

#include "defines.h"

#ifdef GOLD_STEAM

#include "network/scanner.h"
#include <steam/steam_api.h>

class NetworkScannerSteam : public INetworkScanner {
public:
    NetworkScannerSteam() {}

    bool is_initialized_successfully() const override;
    void search_lobbies(const char* query) override;
    void service() override;
private:
    STEAM_CALLBACK(NetworkScannerSteam, on_lobby_matchlist, LobbyMatchList_t);
};

#endif