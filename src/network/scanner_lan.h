#pragma once

#include "network/scanner.h"
#include <enet/enet.h>

class NetworkScannerLan : public INetworkScanner {
public:
    NetworkScannerLan();
    ~NetworkScannerLan();

    bool is_initialized_successfully() const override;
    void search_lobbies(const char* query) override;
    void service() override;
private:
    ENetSocket scanner_socket;
};