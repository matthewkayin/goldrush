#pragma once

#include <cstdint>
#include <string>

using EnumToStrFn = const char* (*)(uint32_t);
const uint32_t ENUM_TO_STR_NO_FALLBACK = UINT32_MAX;

bool string_ends_with(const std::string& str, const char* suffix);
uint32_t enum_from_str(const char* str, EnumToStrFn enum_to_str, uint32_t enum_count, uint32_t fallback = ENUM_TO_STR_NO_FALLBACK);
void strcpy_to_upper(char* dest, const char* src);