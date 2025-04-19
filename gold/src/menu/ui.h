#pragma once

#include "math/gmath.h"
#include "core/input.h"
#include "render/sprite.h"
#include "render/font.h"
#include <string>
#include "defines.h"

#define UI_RENDER_TEXT_BUFFER_SIZE 128
#define UI_Z_INDEX_COUNT 2
#define UI_MAIN 0
#define UI_OPTIONS 1
#define UI_COUNT 2

enum UiHotkeyButtonMode {
    // The button can be hovered and clicked
    UI_ICON_BUTTON_ENABLED,
    // The button cannot hovered or clicked
    UI_ICON_BUTTON_DISABLED,
    // The button cannot be hovered or clicked and it is also grayed out
    UI_ICON_BUTTON_GRAYED_OUT
};

enum UiDropdownType {
    UI_DROPDOWN,
    UI_DROPDOWN_MINI
};

/**
 * Clears the UI render list. Should be called every frame
 * @param id ID of the UI to begin
 * @param input_enabled If false, all inputs will be disabled for this UI
 */
void ui_begin(uint32_t id, bool input_enabled);

/**
 * Creates a one-off container. The next UI element will render at the specified position
 * @param position The position to render the next element at
 */
void ui_element_position(ivec2 position);

/**
 * Creates a one-off container. The next UI element will occupy the specified size in its parent container, regardless of its actual size
 * This function can be used to statically define the spacing of row and column elements
 * @param size The size that the next element should occupy
 */
void ui_element_size(ivec2 size);

/**
 * Creates a row container. Elements inside the row will be horizontally aligned
 * @param position Position to begin the row at. The row's first element will be rendered here.
 * @param spacing The horizontal space between each row element
 */
void ui_begin_row(ivec2 position, int spacing);

/**
 * Creates a column container. Elements inside the column will be vertically aligned
 * @param position Position to begin the column at. The column's first element will be rendered here.
 * @param spacing The vertical space between each row element
 */
void ui_begin_column(ivec2 position, int spacing);

/**
 * Ends the current container
 */
void ui_end_container();

/**
 * Creates a button
 * @param text The text on the button
 * @return True if the button has been clicked this frame
 */
bool ui_button(const char* text);

/**
 * Create a sprite button
 * @param sprite The sprite of the button to render. The sprite will render with hframe 0 by default and hframe 1 when hovered.
 * @param disabled If true, the sprite will render with hframe 2 and will not be clickable
 * @param flip_h If true, the sprite will be flipped horizontally when rendered
 * @return True if the button has been clicked this frame
 */
bool ui_sprite_button(SpriteName sprite, bool disabled, bool flip_h);

/**
 * Creates text
 * @param font The font of the text
 * @param text The value of the text
 */
void ui_text(FontName font, const char* text);

/**
 * Gets the size of the text a given frame
 * @param text The value of the text
 * @return The size of the text frame
 */
ivec2 ui_text_frame_size(const char* text);

/**
 * Creates text on top of a parchment frame. Font will always be FONT_HACK
 * @param text The value of the text to render
 * @param disabled If true, the text frame will not act as a button. It will have no mouse hover effect and will not respond to click events.
 * @return True if the text frame is clicked
 */
bool ui_text_frame(const char* text, bool disabled);

/**
 * Creates a UI frame of the specified size
 * @param size The size of the frame
 */
void ui_frame(ivec2 size);

/**
 * Creates a UI frame of the size and position specified by the Rect.
 * Calling this is equivalent to calling: 
 * ui_element_position(ivec2(rect.x, rect.y));
 * ui_frame(ivec2(size.x, size.y));
 * 
 * @param rect The size and position of the frame
 */
void ui_frame_rect(Rect rect);

/**
 * Creates a text input
 * @param prompt Text to be rendered at the beginning of the input
 * @param size The size of the text input element
 * @param value Pointer to the string that will be edited by the text input
 * @param max_length Text entered by the input will not exceed this length
 */
void ui_text_input(const char* prompt, ivec2 size, std::string* value, size_t max_length);

/**
 * Creates a team picker
 * @param value Char to render inside the team picker
 * @param disabled If true, the team picker will not act as a button
 * @return True if the team picker has been clicked this frame
 */
bool ui_team_picker(char value, bool disabled);

/**
 * Creates a dropdown menu
 * @param dropdown_id Needed for state keeping, should be a unique ID per dropdown
 * @param selected_item Pointer to the index of the selected item. Will be modified when user changes the dropdown value.
 * @param items List of items to choose from
 * @param disabled If true, the dropdown will not be clickable.
 * @return True if the dropdown value was changed
 */
bool ui_dropdown(int dropdown_id, UiDropdownType type, uint32_t* selected_item, const char* const* items, size_t item_count, bool disabled);

/**
 * Renders everything in the UI's render list.
 * When a UI element is created it gets added to the render list.
 * Elements remain in the render list unless cleared by ui_begin().
 * @param id ID of the UI to render
 */
void ui_render(uint32_t id);