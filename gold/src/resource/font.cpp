#include "font.h"

#include <unordered_map>

static const std::unordered_map<font_name, font_params_t> FONT_PARAMS = {
    { FONT_HACK, (font_params_t) { .path = "hack.ttf", .size = 10 }}
};

font_params_t resource_get_font_params(font_name name) {
    return FONT_PARAMS.at(name);
}