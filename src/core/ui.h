#pragma once

#include "defines.h"
#include "math/gmath.h"
#include "core/input.h"
#include "render/sprite.h"
#include "render/font.h"
#include "render/render.h"
#include <vector>
#include <string>

#define UI_RENDER_TEXT_BUFFER_SIZE 128
#define UI_Z_INDEX_COUNT 2
#define UI_ICON_BUTTON_EMPTY SPRITE_COUNT

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

enum UiSliderDisplay {
    UI_SLIDER_DISPLAY_RAW_VALUE,
    UI_SLIDER_DISPLAY_PERCENT,
    UI_SLIDER_DISPLAY_NO_VALUE
};

enum UiSliderSize {
    UI_SLIDER_SIZE_NORMAL,
    UI_SLIDER_SIZE_MINI
};

struct UiSliderParams {
    UiSliderDisplay display;
    UiSliderSize size;
    uint32_t min;
    uint32_t max;
    uint32_t step;
};

enum UiRenderType {
    UI_RENDER_TEXT,
    UI_RENDER_SPRITE,
    UI_RENDER_NINEPATCH,
    UI_RENDER_FILL_RECT,
    UI_RENDER_DRAW_RECT
};

struct UiRenderText {
    FontName font;
    char text[UI_RENDER_TEXT_BUFFER_SIZE];
    ivec2 position;
};

struct UiRenderSprite {
    SpriteName sprite;
    ivec2 frame;
    ivec2 position;
    bool flip_h;
};

struct UiRenderNinepatch {
    SpriteName sprite;
    Rect rect;
};

struct UiRenderRect {
    Rect rect;
    RenderColor color;
};

struct UiRender {
    UiRenderType type;
    union {
        UiRenderText text;
        UiRenderSprite sprite;
        UiRenderNinepatch ninepatch;
        UiRenderRect rect;
    };
};

enum UiContainerType {
    UI_CONTAINER_ROW,
    UI_CONTAINER_COLUMN,
    UI_CONTAINER_ELEMENT_POSITION,
    UI_CONTAINER_ELEMENT_SIZE
};

struct UiContainer {
    UiContainerType type;
    ivec2 origin;
    int spacing;
    ivec2 size;
};

struct UI {
    bool input_enabled;
    bool text_input_show_cursor;
    uint32_t text_input_cursor_blink_timer;
    std::vector<UiContainer> container_stack;
    std::vector<UiRender> render_queue[UI_Z_INDEX_COUNT]; 
    int next_element_id;
    int element_selected;
    int element_selected_future;
    int text_input_selected;
    int next_text_input_id;
    int toolbar_column_selected;
};

UI ui_init();

/**
 * Marks the beginning of a new sub-UI. 
 */
void ui_begin(UI& state);

/**
 * Creates a one-off container. The next UI element will render at the specified position
 * @param position The position to render the next element at
 */
void ui_element_position(UI& state, ivec2 position);

/**
 * Creates a one-off container. The next UI element will occupy the specified size in its parent container, regardless of its actual size
 * This function can be used to statically define the spacing of row and column elements
 * @param size The size that the next element should occupy
 */
void ui_element_size(UI& state, ivec2 size);

/**
 * @param state
 * @param size
 */
void ui_insert_padding(UI& state, ivec2 size);

/**
 * Creates a row container. Elements inside the row will be horizontally aligned
 * @param position Position to begin the row at. The row's first element will be rendered here.
 * @param spacing The horizontal space between each row element
 */
void ui_begin_row(UI& state, ivec2 position, int spacing);

/**
 * Creates a column container. Elements inside the column will be vertically aligned
 * @param position Position to begin the column at. The column's first element will be rendered here.
 * @param spacing The vertical space between each row element
 */
void ui_begin_column(UI& state, ivec2 position, int spacing);

/**
 * Ends the current container
 */
void ui_end_container(UI& state);

/**
 * Returns the position that the next element in the current container will be placed at
 * @param state
 */
ivec2 ui_get_container_origin(const UI& state);

/**
 * Gets the size of a button
 * @param text The text on the button
 * @return The size of the button
 */
ivec2 ui_button_size(const char* text);


/*
 * Returns the position a button so that the button is in the bottom left of the frame rect
 * @param rect
 */
ivec2 ui_button_position_frame_bottom_left(Rect rect);

/*
 * Returns the position a button so that the button is in the bottom right of the frame rect
 * @param rect
 * @param text
 */
ivec2 ui_button_position_frame_bottom_right(Rect rect, const char* text);

/**
 * Creates a button
 * @param text The text on the button
 * @param size If overriden, will be used as the size of the button, the text will go in the center
 * @param center_horizontally If true, the button will be centered horizontally about its render position
 * @return True if the button has been clicked this frame
 */
bool ui_button(UI& state, const char* text, ivec2 size = ivec2(-1, -1), bool center_horizontally = false);

/**
 * Creates a button, uses UI_DROPDOWN_MINI as the frame
 * @param text The text on the button
 * @param size If overriden, will be used as the size of the button, the text will go in the center
 * @param center_horizontally If true, the button will be centered horizontally about its render position
 * @return True if the button has been clicked this frame
 */
bool ui_slim_button(UI& state, const char* text);

/**
 * Create a sprite button
 * @param sprite The sprite of the button to render. The sprite will render with hframe 0 by default and hframe 1 when hovered.
 * @param disabled If true, the sprite will render with hframe 2 and will not be clickable
 * @param flip_h If true, the sprite will be flipped horizontally when rendered
 * @return True if the button has been clicked this frame
 */
bool ui_sprite_button(UI& state, SpriteName sprite, bool disabled, bool flip_h);

/**
 * Create an icon button, which has an icon frame with an icon on top of it
 * @param sprite The sprite to render on top of the button. Passing in UI_ICON_BUTTON_EMPTY will render the button without an icon
 * @param selected If true, the button will appear brighter than normal
 * @return True if the button was clicked this frame
 */
bool ui_icon_button(UI& state, SpriteName sprite, bool selected);

/**
 * Creates text
 * @param font The font of the text
 * @param text The value of the text
 */
void ui_text(UI& state, FontName font, const char* text, bool center_horizontally = false);

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
bool ui_text_frame(UI& state, const char* text, bool disabled);

/**
 * Creates a UI frame of the specified size
 * @param size The size of the frame
 */
void ui_frame(UI& state, ivec2 size);

/**
 * Creates a UI frame of the size and position specified by the Rect.
 * Calling this is equivalent to calling: 
 * ui_element_position(ivec2(rect.x, rect.y));
 * ui_frame(ivec2(size.x, size.y));
 * 
 * @param rect The size and position of the frame
 */
void ui_frame_rect(UI& state, Rect rect);

/*
 * Like UI frame rect, but small
 */
void ui_small_frame_rect(UI& state, Rect rect);

/**
 * Reners an offblack transparent rect across the entire screen
 */
void ui_screen_shade(UI& state);

/**
 * Creates a text input
 * @param element_id Needed for state keeping
 * @param prompt Text to be rendered at the beginning of the input
 * @param size The size of the text input element
 * @param value Pointer to the string that will be edited by the text input
 * @param max_length Text entered by the input will not exceed this length
 */
void ui_text_input(UI& state, const char* prompt, ivec2 size, std::string* value, size_t max_length);

/**
 * Creates a team picker
 * @param value Char to render inside the team picker
 * @param disabled If true, the team picker will not act as a button
 * @return True if the team picker has been clicked this frame
 */
bool ui_team_picker(UI& state, char value, bool disabled);

/**
 * Creates a dropdown menu
 * @param dropdown_id Needed for state keeping, should be a unique ID per dropdown
 * @param selected_item Pointer to the index of the selected item. Will be modified when user changes the dropdown value.
 * @param items List of items to choose from
 * @param disabled If true, the dropdown will not be clickable.
 * @return True if the dropdown value was changed
 */
bool ui_dropdown(UI& state, UiDropdownType type, uint32_t* selected_item, const std::vector<std::string>& items, bool disabled);

/**
 * Creates a toolbar
 * @param column Will be set equal to the column of the selected action
 * @param action Will be set equal to the selected action
 * @param items List of items to appear in the toolbar
 * @param spacing Distance between each column
 * @return True if a toolbar item was clicked
 */
bool ui_toolbar(UI& state, std::string* column, std::string* action, const std::vector<std::vector<std::string>>& items, int spacing);

/**
 * Creates a slider
 * @param slider_id Needed for state keeping, should be a unique ID per slider
 * @param value Pointer to the value. Will be modified when user changes the value.
 * @param buffered_value Pointer to the buffered value. Can be NULL, in which case no buffered value will be rendered. The buffered value is used to display the white bar on the slider that shows how much of a replay has been loaded.
 * @param params Parameters customizing the slider
 * @return True if the slider value has changed
 */
bool ui_slider(UI& state, uint32_t* value, uint32_t* buffered_value, UiSliderParams params);

/**
 * Creates a screenshot frame
 * @param state
 * @param size
 */
bool ui_screenshot_frame(UI& state, ivec2 size);

/**
 * Renders everything in the UI's render list.
 * When a UI element is created it gets added to the render list.
 * Elements remain in the render list unless cleared by ui_begin().
 */
void ui_render(const UI& state);