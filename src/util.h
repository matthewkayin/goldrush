#pragma once

#include <string>

inline bool filename_ends_in_rep(const std::string& filename) {
    return filename.size() >= 5 &&
            filename.compare(filename.size() - 4, 4, ".rep") == 0;
}