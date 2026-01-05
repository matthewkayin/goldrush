#include "util.h"

bool string_ends_with(const std::string& str, const char* suffix) {
    size_t suffix_length = strlen(suffix);
    return str.size() >= suffix_length + 1 && str.compare(str.size() - suffix_length, suffix_length, suffix) == 0;
}