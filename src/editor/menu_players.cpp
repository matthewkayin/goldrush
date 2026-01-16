#include "menu_players.h"

#ifdef GOLD_DEBUG

#include "util/bitflag.h"

static const Rect MENU_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (300 / 2),
    .y = 32,
    .w = 300,
    .h = 296
};

static const int ICON_ROW_SIZE = 8;

static const UiSliderParams STARTING_GOLD_SLIDER_PARAMS = (UiSliderParams) {
    .display = UI_SLIDER_DISPLAY_RAW_VALUE,
    .size = UI_SLIDER_SIZE_NORMAL,
    .min = 0,
    .max = 1500,
    .step = 50
};

static const std::vector<std::string> YES_NO_DROPDOWN_OPTIONS = { "No", "Yes" };
static const std::vector<std::string> TARGET_BASE_COUNT_DROPDOWN_OPTIONS = { "0", "1", "2", "Match Enemy" };
static const uint32_t TARGET_BASE_COUNT_MATCH_ENEMY_INDEX = 3;

void editor_menu_players_bot_config_dropdown(UI& ui, BotConfig& bot_config, const char* prompt, uint32_t flag);
bool editor_menu_players_is_tech_allowed(const Scenario* scenario, uint32_t player_id, uint32_t tech_index);
void editor_menu_players_set_tech_allowed(Scenario* scenario, uint32_t player_id, uint32_t tech_index, bool value);

EditorMenuPlayers editor_menu_players_open(const Scenario* scenario) {
    EditorMenuPlayers menu;

    menu.mode = EDITOR_MENU_PLAYERS_MODE_PLAYERS;
    menu.selected_player_id = 0;
    menu.player_name_string = std::string(scenario->players[menu.selected_player_id].name);

    return menu;
}

void editor_menu_players_update(EditorMenuPlayers& menu, UI& ui, EditorMenuMode& mode, Scenario* scenario) {
    editor_menu_header(ui, MENU_RECT, "Edit Players");

    ui_begin_column(ui, ivec2(MENU_RECT.x + 8, MENU_RECT.y + 30), 4);
        if (menu.mode == EDITOR_MENU_PLAYERS_MODE_PLAYERS) {
            // Selection dropdown items
            std::vector<std::string> player_dropdown_items;
            for (uint8_t player_id = 0; player_id < MAX_PLAYERS; player_id++) {
                char item_text[64];
                sprintf(item_text, "%u: %s", player_id, scenario->players[player_id].name);
                player_dropdown_items.push_back(item_text);
            }

            // Selection dropdown
            uint32_t previous_selected_player_id = menu.selected_player_id;
            if (editor_menu_dropdown(ui, "Player: ", &menu.selected_player_id, player_dropdown_items, MENU_RECT)) {
                if (input_is_text_input_active()) {
                    input_stop_text_input();
                    strncpy(scenario->players[previous_selected_player_id].name, menu.player_name_string.c_str(), MAX_USERNAME_LENGTH);
                }

                menu.player_name_string = std::string(scenario->players[menu.selected_player_id].name);
            }

            // Name
            if (menu.selected_player_id != 0) {
                ui_text_input(ui, "Name: ", ivec2(MENU_RECT.w - 32, 24), &menu.player_name_string, MAX_USERNAME_LENGTH);
            }

            // Starting gold
            editor_menu_slider(ui, "Starting Gold:", &scenario->players[menu.selected_player_id].starting_gold, STARTING_GOLD_SLIDER_PARAMS, MENU_RECT);

            // Bot config options
            if (menu.selected_player_id != 0) {
                BotConfig& bot_config = scenario->bot_config[menu.selected_player_id - 1];

                editor_menu_players_bot_config_dropdown(ui, bot_config, "Should Attack First:", BOT_CONFIG_SHOULD_ATTACK_FIRST);
                editor_menu_players_bot_config_dropdown(ui, bot_config, "Should Attack:", BOT_CONFIG_SHOULD_ATTACK);
                editor_menu_players_bot_config_dropdown(ui, bot_config, "Should Harass:", BOT_CONFIG_SHOULD_HARASS);
                editor_menu_players_bot_config_dropdown(ui, bot_config, "Should Retreat:", BOT_CONFIG_SHOULD_RETREAT);

                uint32_t target_base_count_selection = bot_config.target_base_count == BOT_TARGET_BASE_COUNT_MATCH_ENEMY
                    ? TARGET_BASE_COUNT_MATCH_ENEMY_INDEX
                    : bot_config.target_base_count;
                if (editor_menu_dropdown(ui, "Target Base Count:", &target_base_count_selection, TARGET_BASE_COUNT_DROPDOWN_OPTIONS, MENU_RECT)) {
                    bot_config.target_base_count = target_base_count_selection == TARGET_BASE_COUNT_MATCH_ENEMY_INDEX
                        ? BOT_TARGET_BASE_COUNT_MATCH_ENEMY
                        : target_base_count_selection;
                }
            }

            // Edit tech button
            if (editor_menu_prompt_and_button(ui, "Allowed Tech:", "Edit", MENU_RECT)) {
                menu.mode = EDITOR_MENU_PLAYERS_MODE_TECH;
            }
        } else if (menu.mode == EDITOR_MENU_PLAYERS_MODE_TECH) {
            // Player name text
            char player_name_text[128];
            sprintf(player_name_text, "Edit Tech for %u: %s", menu.selected_player_id, scenario->players[menu.selected_player_id].name);
            ui_text(ui, FONT_HACK_GOLD, player_name_text);

            // Allowed tech
            const uint32_t TECH_COUNT = ENTITY_TYPE_COUNT + UPGRADE_COUNT;
            uint32_t row_count = TECH_COUNT / ICON_ROW_SIZE;
            if (TECH_COUNT % ICON_ROW_SIZE != 0) {
                row_count++;
            }

            for (uint32_t row = 0; row < row_count; row++) {
                ui_begin_row(ui, ivec2(4, 0), 2);
                    for (uint32_t col = 0; col < ICON_ROW_SIZE; col++) {
                        uint32_t tech_index = col + (row * ICON_ROW_SIZE);
                        if (tech_index >= TECH_COUNT) {
                            continue;
                        }

                        const SpriteName icon = tech_index < ENTITY_TYPE_COUNT
                            ? entity_get_data((EntityType)tech_index).icon
                            : upgrade_get_data(1U << (tech_index - ENTITY_TYPE_COUNT)).icon;
                        bool is_tech_allowed = editor_menu_players_is_tech_allowed(scenario, menu.selected_player_id, tech_index);
                        if (ui_icon_button(ui, icon, is_tech_allowed)) {
                            editor_menu_players_set_tech_allowed(scenario, menu.selected_player_id, tech_index, !is_tech_allowed);
                        }
                    }
                ui_end_container(ui);
            }
        }
    ui_end_container(ui);

    editor_menu_back_save_buttons(ui, MENU_RECT, mode);

    // Handle tech submenu close
    if (menu.mode == EDITOR_MENU_PLAYERS_MODE_TECH && 
            (mode == EDITOR_MENU_MODE_SUBMIT || mode == EDITOR_MENU_MODE_CLOSED)) {
        menu.mode = EDITOR_MENU_PLAYERS_MODE_PLAYERS;
        mode = EDITOR_MENU_MODE_OPEN;
    }

    if (mode == EDITOR_MENU_MODE_SUBMIT) {
        strncpy(scenario->players[menu.selected_player_id].name, menu.player_name_string.c_str(), MAX_USERNAME_LENGTH);
    }
}

void editor_menu_players_bot_config_dropdown(UI& ui, BotConfig& bot_config, const char* prompt, uint32_t flag) {
    uint32_t value = (uint32_t)bitflag_check(bot_config.flags, flag);
    if (editor_menu_dropdown(ui, prompt, &value, YES_NO_DROPDOWN_OPTIONS, MENU_RECT)) {
        bitflag_set(&bot_config.flags, flag, (bool)value);
    }
}

bool editor_menu_players_is_tech_allowed(const Scenario* scenario, uint32_t player_id, uint32_t tech_index) {
    if (tech_index < ENTITY_TYPE_COUNT) {
        return player_id == 0
            ? scenario->player_allowed_entities[tech_index]
            : scenario->bot_config[player_id - 1].is_entity_allowed[tech_index];
    } else {
        uint32_t upgrade_index = tech_index - ENTITY_TYPE_COUNT;
        uint32_t upgrade = (1U << upgrade_index);
        return player_id == 0
            ? (scenario->player_allowed_upgrades & upgrade) == upgrade
            : (scenario->bot_config[player_id - 1].allowed_upgrades & upgrade) == upgrade;
    }
}

void editor_menu_players_set_tech_allowed(Scenario* scenario, uint32_t player_id, uint32_t tech_index, bool value) {
    if (tech_index < ENTITY_TYPE_COUNT) {
        bool* tech_allowed_ptr = player_id == 0
            ? scenario->player_allowed_entities
            : scenario->bot_config[player_id - 1].is_entity_allowed;
        tech_allowed_ptr[tech_index] = value;
    } else {
        uint32_t upgrade_index = tech_index - ENTITY_TYPE_COUNT;
        uint32_t upgrade = (1U << upgrade_index);
        uint32_t* allowed_upgrades_ptr = player_id == 0
            ? &scenario->player_allowed_upgrades
            : &scenario->bot_config[player_id - 1].allowed_upgrades;

        bitflag_set(allowed_upgrades_ptr, upgrade, value);
    }
}

#endif