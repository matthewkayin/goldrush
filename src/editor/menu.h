#pragma once

#include "defines.h"

#ifdef GOLD_DEBUG

#include "core/ui.h"

enum EditorMenuMode {
    EDITOR_MENU_MODE_CLOSED,
    EDITOR_MENU_MODE_OPEN,
    EDITOR_MENU_MODE_SUBMIT
};

bool editor_menu_dropdown(UI& ui, const char* prompt, uint32_t* selection, const std::vector<std::string>& items, const Rect& rect);
bool editor_menu_slider(UI& ui, const char* prompt, uint32_t* value, const UiSliderParams& params, const Rect& rect);
bool editor_menu_prompt_and_button(UI& ui, const char* prompt, const char* button, Rect rect);
void editor_menu_header(UI& ui, Rect rect, const char* header_text);
void editor_menu_back_save_buttons(UI& ui, Rect rect, EditorMenuMode& mode);

#endif