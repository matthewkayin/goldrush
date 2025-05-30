#include "event.h"

#include <queue>

static std::queue<NetworkEvent> event_queue;

bool network_poll_events(NetworkEvent* event) {
    if (event_queue.empty()) {
        return false;
    }

    *event = event_queue.front();
    event_queue.pop();

    return true;
}

void network_push_event(NetworkEvent event) {
    event_queue.push(event);
}