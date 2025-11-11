#pragma once

#include "defines.h"

enum FontName {
    FONT_HACK,
    FONT_WESTERN8,
    FONT_M3X6,
    FONT_COUNT
};

enum FontImportStrategy {
    FONT_IMPORT_DEFAULT,
    FONT_IMPORT_IGNORE_BEARING,
    FONT_IMPORT_NUMERIC_ONLY
};

struct FontParams {
    FontImportStrategy strategy;
    const char* path;
    int size;
};

const FontParams& resource_get_font_params(FontName name);
