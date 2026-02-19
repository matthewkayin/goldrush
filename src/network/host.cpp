#include "host.h"

bool INetworkHost::poll_events(NetworkHostEvent* event) {
    if (host_events.empty()) {
        return false;
    }

    *event = host_events.front();
    host_events.pop();

    return true;
}