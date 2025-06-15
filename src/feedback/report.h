#pragma once

#include <string>
#include <vector>

struct FeedbackReport {
    std::string name;
    std::string description;
    std::vector<std::string> attachments;
};

bool feedback_report_send(FeedbackReport ticket);
