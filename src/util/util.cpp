#include "util.h"

#include "core/logger.h"
#include "core/asserts.h"
#include <cstring>

bool string_ends_with(const std::string& str, const char* suffix) {
    size_t suffix_length = strlen(suffix);
    return str.size() >= suffix_length + 1 && str.compare(str.size() - suffix_length, suffix_length, suffix) == 0;
}

uint32_t enum_from_str(const char* str, EnumToStrFn enum_to_str, uint32_t enum_count, uint32_t fallback) {
    for (uint32_t value = 0; value < enum_count; value++) {
        if (strcmp(enum_to_str(value), str) == 0) {
            return value;
        }
    }

    if (fallback == ENUM_TO_STR_NO_FALLBACK) {
        log_warn("enum_from_str could not find a value for str %s. Falling back to max value.", str);
        return enum_count;
    }

    log_warn("enum_from_str could not find a value for str %s. Falling back to %s.", str, enum_to_str(fallback));
    return fallback;
}

void strcpy_to_upper(char* dest, const char* src) {
    size_t index = 0;
    while (src[index] != '\0') {
        if (src[index] == ' ') {
            dest[index] = '_';
        } else {
            dest[index] = toupper(src[index]);
        }
        index++;
    }
    dest[index] = '\0';
}