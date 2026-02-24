#include "shell/shell.h"

#ifdef PLATFORM_MACOS
    #define PLATFORM_CTRL_STR "CMD"
#else
    #define PLATFORM_CTRL_STR "CTRL"
#endif

static const int HELP_MENU_WIDTH = 372;
static const Rect HELP_MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (HELP_MENU_WIDTH / 2),
    .y = 32,
    .w = HELP_MENU_WIDTH,
    .h = 292
};

const uint8_t MENU_HELP_PAGE_COUNT = 17;

void match_shell_help_update(MatchShellState* state) {
    state->ui.input_enabled = true;
    ui_screen_shade(state->ui);
    ui_frame_rect(state->ui, HELP_MENU_RECT);

    // Header text
    const char* header_text = "Help";
    ivec2 text_size = render_get_text_size(FONT_WESTERN8_GOLD, header_text);
    ivec2 text_pos = ivec2(HELP_MENU_RECT.x + (HELP_MENU_RECT.w / 2) - (text_size.x / 2), HELP_MENU_RECT.y + 8);
    ui_element_position(state->ui, text_pos);
    ui_text(state->ui, FONT_WESTERN8_GOLD, header_text);

    const ivec2 column_position = ivec2(HELP_MENU_RECT.x + 8, HELP_MENU_RECT.y + 32);
    ui_begin_column(state->ui, column_position, 4);
        switch (state->menu_help_page) {
            case 0: {
                ui_text(state->ui, FONT_HACK_GOLD, "Welcome to Gold Rush!");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "This help page is here to teach you about the game and its");
                ui_text(state->ui, FONT_HACK_GOLD, "controls.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "The goal of a standard match is to defeat your opponents.");
                ui_text(state->ui, FONT_HACK_GOLD, "You do this by destroying all of your opponent's buildings");
                ui_text(state->ui, FONT_HACK_GOLD, "or by getting them to surrender first.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "To learn more, click the navigation arrows in the");
                ui_text(state->ui, FONT_HACK_GOLD, "bottom right.");
                break;
            }
            case 1: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Camera Movement --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Move the camera by dragging your mouse to the edge of the");
                ui_text(state->ui, FONT_HACK_GOLD, "screen. (You can try this now even from inside the menu!)");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "You can also move the camera by clicking inside the minimap");
                ui_text(state->ui, FONT_HACK_GOLD, "in the bottom-left. The white rectangle shows you where");
                ui_text(state->ui, FONT_HACK_GOLD, "your camera is within the entire map.");
                break;
            }
            case 2: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Unit Selection --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Left click a unit, building, or goldmine to select it.");
                ui_text(state->ui, FONT_HACK_GOLD, "To select multiple units at once, hold left click and");
                ui_text(state->ui, FONT_HACK_GOLD, "drag the mouse to select all units in the rectangle.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Hold shift while selecting units to add the units");
                ui_text(state->ui, FONT_HACK_GOLD, "to your current selection.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Double-click a unit to select all on-screen units of the");
                ui_text(state->ui, FONT_HACK_GOLD, "same type. You can also do this by holding " PLATFORM_CTRL_STR);
                ui_text(state->ui, FONT_HACK_GOLD, "while clicking on the unit.");
                break;
            }
            case 3: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Unit Movement --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Right click on the ground to order selected units to move.");
                ui_text(state->ui, FONT_HACK_GOLD, "This is a \"move command\", which means units given this");
                ui_text(state->ui, FONT_HACK_GOLD, "order will move to their target even if they encounter");
                ui_text(state->ui, FONT_HACK_GOLD, "danger along the way.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Another way to move is by \"attack moving\". To attack-move,");
                ui_text(state->ui, FONT_HACK_GOLD, "click the Attack button in the bottom right, then");
                ui_text(state->ui, FONT_HACK_GOLD, "left click on the area to attack. Attack-moving");
                ui_text(state->ui, FONT_HACK_GOLD, "means that units will automatically attack any enemies");
                ui_text(state->ui, FONT_HACK_GOLD, "they encounter along the way. This is useful for charging");
                ui_text(state->ui, FONT_HACK_GOLD, "into enemy territory.");
                break;
            }
            case 4: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Stop and Hold Position --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Press the Stop button in the bottom-right to order your");
                ui_text(state->ui, FONT_HACK_GOLD, "units to stop what they are doing.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Press the Hold Position button in the bottom-right to");
                ui_text(state->ui, FONT_HACK_GOLD, "order your units to hold their positions. Units given");
                ui_text(state->ui, FONT_HACK_GOLD, "this command will stand their ground even when there");
                ui_text(state->ui, FONT_HACK_GOLD, "are enemy units drawing their attention.");
                break;
            }
            case 5: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Building --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "To build a building, press the Build button, then choose");
                ui_text(state->ui, FONT_HACK_GOLD, "a building, then left click on the place to build it.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "If you have multiple miners selected, they will all");
                ui_text(state->ui, FONT_HACK_GOLD, "build together and the building will finish sooner,");
                ui_text(state->ui, FONT_HACK_GOLD, "though it costs extra gold for you to do this.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "The Advanced Build button has another list of buildings");
                ui_text(state->ui, FONT_HACK_GOLD, "that you can build, though you'll need to unlock them by");
                ui_text(state->ui, FONT_HACK_GOLD, "making their pre-requisite buildings first.");
                break;
            }
            case 6: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- In-Progress Buildings --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "An in-progress building's healthbar will show you how close");
                ui_text(state->ui, FONT_HACK_GOLD, "it is to finishing.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "If a building is damaged while it is in-progress, then it");
                ui_text(state->ui, FONT_HACK_GOLD, "will still finish in the same amount of time, but it will");
                ui_text(state->ui, FONT_HACK_GOLD, "have less health on completion.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Select an in-progress building and press the Cancel button");
                ui_text(state->ui, FONT_HACK_GOLD, "to cancel the building. This will refund some of your");
                ui_text(state->ui, FONT_HACK_GOLD, "gold, but you get less gold back the closer the building");
                ui_text(state->ui, FONT_HACK_GOLD, "was to finishing.");
                break;
            }
            case 7: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Repairing --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "With a miner selected, click the Repair button, then click");
                ui_text(state->ui, FONT_HACK_GOLD, "on a damaged building to order the miner to repair it.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "You can also simply right click the building to repair,");
                ui_text(state->ui, FONT_HACK_GOLD, "but if you do this to a bunker, the miner may go inside the");
                ui_text(state->ui, FONT_HACK_GOLD, "bunker instead, so it's better to use the Repair button");
                ui_text(state->ui, FONT_HACK_GOLD, "when dealing with bunkers.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Repairing a building costs gold.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Repairing an in-progress building makes it finish faster.");
                break;
            }
            case 8: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Building Queues --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "To hire a new unit or research an upgrade, select a");
                ui_text(state->ui, FONT_HACK_GOLD, "building and choose the unit or upgrade from the buttons");
                ui_text(state->ui, FONT_HACK_GOLD, "in the bottom-right. This will add the unit or upgrade to");
                ui_text(state->ui, FONT_HACK_GOLD, "the building's queue. Clicking on a building queue item will");
                ui_text(state->ui, FONT_HACK_GOLD, "cancel the item and give you a full refund.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "If you have multiple buildings selected of the same type,");
                ui_text(state->ui, FONT_HACK_GOLD, "then hiring or researching will automatically add the item");
                ui_text(state->ui, FONT_HACK_GOLD, "to the building which has the shortest queue.");
                break;
            }
            case 9: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Rally Points --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "With one or more unit-producing buildings selected, right");
                ui_text(state->ui, FONT_HACK_GOLD, "click on the ground to set a rally point. New units will");
                ui_text(state->ui, FONT_HACK_GOLD, "walk to the rally point one they are finished.");
                break;
            }
            case 10: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Target Queue --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Hold Shift while giving orders to a unit to add that");
                ui_text(state->ui, FONT_HACK_GOLD, "order to the unit's target queue. Use this to queue");
                ui_text(state->ui, FONT_HACK_GOLD, "up multiple orders in succession.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "For example, if you select a unit, hold shift, and");
                ui_text(state->ui, FONT_HACK_GOLD, "right click on points A, B, and C on the map, then");
                ui_text(state->ui, FONT_HACK_GOLD, "the unit will walk first to A, then to B, then to C.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "This is especially useful for using Pyros to lay landmines.");
                break;
            }
            case 11: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Garrison --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Wagons and Bunkers are two units that can carry other units");
                ui_text(state->ui, FONT_HACK_GOLD, "inside them. Select a unit and right click a Wagon or Bunker");
                ui_text(state->ui, FONT_HACK_GOLD, "to garrison that unit inside the carrier.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Some units, such as the Cannoneer, cannot garrison inside");
                ui_text(state->ui, FONT_HACK_GOLD, "carrier units.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "To ungarrison from a Bunker, select it and click the Unload");
                ui_text(state->ui, FONT_HACK_GOLD, "button. To ungarrison from a Wagon, select the it, click");
                ui_text(state->ui, FONT_HACK_GOLD, "Unload, then left click on the place to unload your units.");
                break;
            }
            case 12: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Accuracy --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Ranged units have an accuracy which determines their chance");
                ui_text(state->ui, FONT_HACK_GOLD, "to hit. If they miss, they will do no damage and you will");
                ui_text(state->ui, FONT_HACK_GOLD, "hear a ricochet sound effect.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Note that if a Cannonneer misses, it will still deal splash");
                ui_text(state->ui, FONT_HACK_GOLD, "damage at the firing location.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Units attacking from the lowground to the highground have");
                ui_text(state->ui, FONT_HACK_GOLD, "a disadvantage. They will shoot at 1/2 of their original");
                ui_text(state->ui, FONT_HACK_GOLD, "accuracy.");
                break;
            }
            case 13: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Control Groups --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "With a group of units selected, press " PLATFORM_CTRL_STR " + a number key");
                ui_text(state->ui, FONT_HACK_GOLD, "to assign the selected units to the control group of that");
                ui_text(state->ui, FONT_HACK_GOLD, "number.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Once a control group is set, press a number key to select");
                ui_text(state->ui, FONT_HACK_GOLD, "the units in that control group. Double tap the number key");
                ui_text(state->ui, FONT_HACK_GOLD, "to center your camera on the units in the control group.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "With a group of units selected, press Shift + a number key");
                ui_text(state->ui, FONT_HACK_GOLD, "to add the selected units to the control group of that");
                ui_text(state->ui, FONT_HACK_GOLD, "number.");
                break;
            }
            case 14: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Camera Hotkeys --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Press Shift + <F1-F6> to assign a camera hotkey. Then press");
                ui_text(state->ui, FONT_HACK_GOLD, "<F1-F6> to snap your camera back to where it was when you");
                ui_text(state->ui, FONT_HACK_GOLD, "originally set the hotkey.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "For example, Pressing Shift + F1 will save your current");
                ui_text(state->ui, FONT_HACK_GOLD, "camera position into F1. You can then press F1 to snap your");
                ui_text(state->ui, FONT_HACK_GOLD, "camera back to the saved position.");
                break;
            }
            case 15: {
                ui_text(state->ui, FONT_HACK_GOLD, "-- Minimap and Alerts --");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "The minimap in the bottom-left corner shows you a mini");
                ui_text(state->ui, FONT_HACK_GOLD, "picture of the entire map. You can use this to view unit");
                ui_text(state->ui, FONT_HACK_GOLD, "movements on a larger scale");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Sometimes the minimap will show you alerts of important");
                ui_text(state->ui, FONT_HACK_GOLD, "events (like a building finishing).");
                ui_text(state->ui, FONT_HACK_GOLD, "Alerts are always accompanied by a sound and appear in the");
                ui_text(state->ui, FONT_HACK_GOLD, "minimap as a colored rectangle that narrows in on a point.");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "Press Space to snap your camera to the most recent alert.");
                break;
            }
            case 16: {
                ui_text(state->ui, FONT_HACK_GOLD, "That's all!");
                ui_text(state->ui, FONT_HACK_GOLD, "");
                ui_text(state->ui, FONT_HACK_GOLD, "I hope you enjoy the game!");
                break;
            }
        }
    ui_end_container(state->ui);

    ui_element_position(state->ui, ui_button_position_frame_bottom_left(HELP_MENU_RECT));
    if (ui_button(state->ui, "Back")) {
        state->mode = MATCH_SHELL_MODE_MENU;
    }

    const SpriteInfo& arrow_sprite_info = render_get_sprite_info(SPRITE_UI_BUTTON_ARROW);
    const ivec2 next_button_position = ivec2(HELP_MENU_RECT.x + HELP_MENU_RECT.w - 16 - arrow_sprite_info.frame_width, HELP_MENU_RECT.y + HELP_MENU_RECT.h - arrow_sprite_info.frame_height - 8);
    ui_element_position(state->ui, next_button_position);
    if (ui_sprite_button(state->ui, SPRITE_UI_BUTTON_ARROW, state->menu_help_page == MENU_HELP_PAGE_COUNT - 1, false)) {
        state->menu_help_page++;
    }

    const ivec2 prev_button_position = next_button_position - ivec2(arrow_sprite_info.frame_width + 4, 0);
    ui_element_position(state->ui, prev_button_position);
    if (ui_sprite_button(state->ui, SPRITE_UI_BUTTON_ARROW, state->menu_help_page == 0, true)) {
        state->menu_help_page--;
    }
}