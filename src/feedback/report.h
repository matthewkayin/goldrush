#pragma once

#include <string>
#include <vector>

struct FeedbackTicket {
    std::string name;
    std::string description;
    std::vector<std::string> attachments;
};

bool feedback_report_send(FeedbackTicket ticket);
