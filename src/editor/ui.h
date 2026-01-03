#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"

bool editor_menu_dropdown(UI& ui, const char* prompt, uint32_t* selection, const std::vector<std::string>& items, const Rect& rect);
void editor_menu_slider(UI& ui, const char* prompt, uint32_t* value, const UiSliderParams& params, const Rect& rect);

#endif