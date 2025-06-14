#pragma once

#include <string>
#include <vector>

struct FeedbackTicket {
    std::string name;
    std::string description;
    std::vector<std::string> attachments;
};

void feedback_init();
void feedback_quit();

bool feedback_send(FeedbackTicket ticket);