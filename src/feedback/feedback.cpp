#include "feedback.h"

#include "token.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "core/input.h"
#include "core/options.h"
#include "util.h"
#include "menu/ui.h"
#include "render/render.h"
#include "math/gmath.h"
#include <curl/curl.h>
#include <unordered_map>
#include <string>
#include <SDL3/SDL.h>

enum FeedbackMode {
    FEEDBACK_CLOSED,
    FEEDBACK_WELCOME,
    FEEDBACK_OPEN,
    FEEDBACK_PREVIEW_SCREENSHOT,
    FEEDBACK_SENDING,
    FEEDBACK_SUCCESS,
    FEEDBACK_ERROR,
    FEEDBACK_VALIDATION
};

enum FeedbackType {
    FEEDBACK_TYPE_FEEDBACK,
    FEEDBACK_TYPE_BUG
};

struct FeedbackReport {
    std::string name;
    std::string description;
    std::vector<std::string> attachments;
};

static const char* TRELLO_KEY = "2644097bec308192331862ebca64ce13";
static const char* TRELLO_URL = "https://api.trello.com/1/cards";
static const char* TRELLO_LIST_ID = "684c9d51f095781d10148d44";

static const Rect WELCOME_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (336 / 2),
    .y = (SCREEN_HEIGHT / 2) - 72,
    .w = 336, .h = 112
};

static const Rect FEEDBACK_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (364 / 2),
    .y = 16,
    .w = 364, .h = 324
};

static const int SCREENSHOT_PREVIEW_WIDTH = 480;
static const int SCREENSHOT_PREVIEW_HEIGHT = 270;
static const int FEEDBACK_PREVIEW_WIDTH = SCREENSHOT_PREVIEW_WIDTH + 32;
static const int FEEDBACK_PREVIEW_HEIGHT = SCREENSHOT_PREVIEW_HEIGHT + 56;
static const Rect FEEDBACK_PREVIEW_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (FEEDBACK_PREVIEW_WIDTH / 2),
    .y = (SCREEN_HEIGHT / 2) - (FEEDBACK_PREVIEW_HEIGHT / 2),
    .w = FEEDBACK_PREVIEW_WIDTH,
    .h = FEEDBACK_PREVIEW_HEIGHT
};
static const ivec2 SCREENSHOT_PREVIEW_POSITION = ivec2(FEEDBACK_PREVIEW_RECT.x + 16, FEEDBACK_PREVIEW_RECT.y + 22);

static const std::vector<std::string> FEEDBACK_TYPE_STRS = { "Feedback", "Bug" };
static const std::vector<std::string> FEEDBACK_YES_NO_STRS = { "No", "Yes" };
static const size_t FEEDBACK_NAME_MAX = 42;
static const size_t FEEDBACK_DESC_MAX = 268;
static const ivec2 SCREENSHOT_MINI_SIZE = ivec2(SCREEN_WIDTH / 8, SCREEN_HEIGHT / 8);
static const ivec2 SCREENSHOT_MINI_FRAME_SIZE = SCREENSHOT_MINI_SIZE + ivec2(2, 2);

static const Rect SUCCESS_RECT = (Rect) {
    .x = (SCREEN_WIDTH / 2) - (224 / 2),
    .y = 128,
    .w = 224, .h = 96
};

struct FeedbackState {
    FeedbackMode mode;
    UI ui;
    uint32_t feedback_type;
    uint32_t include_replay;
    uint32_t include_screenshot;
    std::string feedback_name;
    std::string feedback_desc;
    char screenshot_path[256];
    bool screenshot_successful;
    ivec2 screenshot_position;
    std::string replay_path;
    bool has_replay_path;
    FeedbackReport report;
    SDL_Thread* send_report_thread;
};
static FeedbackState state;

void feedback_ui_yes_no(UI& ui, const char* prompt, uint32_t* value, bool disabled);
void feedback_ui_retake_message(UI& ui);
void feedback_begin_send();
const char* feedback_get_dialog_header_text(FeedbackMode mode);

static int feedback_report_send(void* ptr);
bool feedback_upload_attachment(const char* card_id, const char* filepath);
size_t feedback_response_write_callback(void* contents, size_t size, size_t nmemb, std::string* response);

#ifdef PLATFORM_WIN32
    #include <windows.h>
    #include <DbgHelp.h>

    LONG WINAPI feedback_on_crash(EXCEPTION_POINTERS* exception_pointers) {
        // Build dumpfile path
        char dumpfile_path[128];
        char* dumpfile_ptr = dumpfile_path;
        dumpfile_ptr += sprintf(dumpfile_ptr, "logs/");
        dumpfile_ptr += sprintf_timestamp(dumpfile_ptr);
        dumpfile_ptr += sprintf(dumpfile_ptr, ".dmp");
        char dumpfile_fullpath[256];
        filesystem_get_data_path(dumpfile_fullpath, dumpfile_path);

        // Save dumpfile
        HANDLE dump_file = CreateFileA((LPCSTR)dumpfile_fullpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (dump_file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION dump_info;
            dump_info.ExceptionPointers = exception_pointers;
            dump_info.ThreadId = GetCurrentThreadId();
            dump_info.ClientPointers = TRUE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), dump_file, MiniDumpNormal, &dump_info, NULL, NULL);
            CloseHandle(dump_file);
        }

        // Save logs
        logger_quit();

        // Send crash report
        FeedbackReport report = (FeedbackReport) {
            .name = "Crash Report",
            .description = "Gold Rush caught an unhandled exception.",
            .attachments = { std::string(dumpfile_fullpath), logger_get_path() }
        };
        feedback_report_send(&report);

        return EXCEPTION_EXECUTE_HANDLER;
    }
#endif

void feedback_init() {
    curl_global_init(CURL_GLOBAL_ALL);
    #ifdef PLATFORM_WIN32
        SetUnhandledExceptionFilter(feedback_on_crash);
    #endif

    state.ui = ui_init();
    state.feedback_type = FEEDBACK_TYPE_BUG;
    state.mode = FEEDBACK_WELCOME;
    state.include_replay = 0;
    state.include_screenshot = 0;
    state.has_replay_path = false;
}

void feedback_quit() {
    curl_global_cleanup();
}

bool feedback_is_open() {
    return state.mode != FEEDBACK_CLOSED;
}

void feedback_set_replay_path(const char* path) {
    state.replay_path = std::string(path);
    state.has_replay_path = true;
    state.include_replay = 1;
}

void feedback_update() {
    if (input_is_action_just_pressed(INPUT_ACTION_FEEDBACK_MENU)) {
        if (state.mode == FEEDBACK_CLOSED || state.mode == FEEDBACK_WELCOME) {
            char screenshot_subpath[128];
            char* screenshot_path_ptr = screenshot_subpath;
            screenshot_path_ptr += sprintf(screenshot_path_ptr, "screenshots/");
            screenshot_path_ptr += sprintf_timestamp(screenshot_path_ptr);
            screenshot_path_ptr += sprintf(screenshot_path_ptr, ".png");
            filesystem_get_data_path(state.screenshot_path, screenshot_subpath);
            state.screenshot_successful = render_take_screenshot(state.screenshot_path);
            state.include_screenshot = (uint32_t)state.screenshot_successful;

            state.mode = FEEDBACK_OPEN;
        } else {
            state.mode = FEEDBACK_CLOSED;
        }
    }
    if (state.mode == FEEDBACK_CLOSED) {
        return;
    }

    ui_begin(state.ui);
    ui_screen_shade(state.ui);

    if (state.mode == FEEDBACK_WELCOME) {
        ui_frame_rect(state.ui, WELCOME_RECT);

        ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Gold Rush Beta");
        ui_element_position(state.ui, ivec2(WELCOME_RECT.x + (WELCOME_RECT.w / 2) - (header_text_size.x / 2), WELCOME_RECT.y + 6));
        ui_text(state.ui, FONT_HACK_GOLD, "Gold Rush Beta");

        ui_begin_column(state.ui, ivec2(WELCOME_RECT.x + 8, WELCOME_RECT.y + 26), 4);
            ui_text(state.ui, FONT_HACK_GOLD, "Welcome to the Gold Rush Beta!");
            ui_text(state.ui, FONT_HACK_GOLD, "If you find any bugs or just want to provide feedback,");
            ui_text(state.ui, FONT_HACK_GOLD, "you can press F9 to open the feedback menu.");
            ui_element_position(state.ui, ivec2((WELCOME_RECT.w - 16) / 2, 4));
            if (ui_button(state.ui, "Ok", ivec2(-1, -1), true)) {
                state.mode = FEEDBACK_CLOSED;
            }
        ui_end_container(state.ui);
    } else if (state.mode == FEEDBACK_OPEN) {
        ui_frame_rect(state.ui, FEEDBACK_RECT);

        const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);

        ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Submit Feedback");
        ui_element_position(state.ui, ivec2(FEEDBACK_RECT.x + (FEEDBACK_RECT.w / 2) - (header_text_size.x / 2), FEEDBACK_RECT.y + 6));
        ui_text(state.ui, FONT_HACK_GOLD, "Submit Feedback");

        ui_begin_column(state.ui, ivec2(FEEDBACK_RECT.x + 8, FEEDBACK_RECT.y + 26), 4);
            ui_element_size(state.ui, ivec2(0, dropdown_sprite_info.frame_height));
            ui_begin_row(state.ui, ivec2(0, 0), 0);
                ui_element_position(state.ui, ivec2(0, 2));
                ui_text(state.ui, FONT_HACK_GOLD, "What type of feedback is this?");

                ui_element_position(state.ui, ivec2(FEEDBACK_RECT.w - 16 - dropdown_sprite_info.frame_width, 0));
                ui_dropdown(state.ui, UI_DROPDOWN_MINI, &state.feedback_type, FEEDBACK_TYPE_STRS, false);
            ui_end_container(state.ui);

            ui_text_input(state.ui, "Feedback Name: ", ivec2(FEEDBACK_RECT.w - 16, 24), &state.feedback_name, FEEDBACK_NAME_MAX);
            ui_text_input(state.ui, "Description: ", ivec2(FEEDBACK_RECT.w - 16, 92), &state.feedback_desc, FEEDBACK_DESC_MAX, true);

            feedback_ui_yes_no(state.ui, "Include last replay?", &state.include_replay, !state.has_replay_path);
            feedback_ui_yes_no(state.ui, "Include screenshot?", &state.include_screenshot, !state.screenshot_successful);
            if (state.screenshot_successful) {
                ui_begin_row(state.ui, ivec2(0, 0), 0);
                    ui_element_size(state.ui, ivec2(0, SCREENSHOT_MINI_SIZE.y));
                    ui_begin_column(state.ui, ivec2(0, 0), 4);
                        ui_text(state.ui, FONT_HACK_WHITE, "Click the screenshot to preview.");

                        feedback_ui_retake_message(state.ui);
                    ui_end_container(state.ui);

                    ui_element_position(state.ui, ivec2(FEEDBACK_RECT.w - 16 - (SCREENSHOT_MINI_SIZE.x + 1), 0));
                    state.screenshot_position = ui_get_container_origin(state.ui) + ivec2(1, 1);
                    if (ui_screenshot_frame(state.ui, SCREENSHOT_MINI_FRAME_SIZE)) {
                        state.mode = FEEDBACK_PREVIEW_SCREENSHOT;
                    }
                ui_end_container(state.ui);
            } else {
                ui_text(state.ui, FONT_HACK_WHITE, "Error: could not save screenshot.");
                feedback_ui_retake_message(state.ui);
            }
        ui_end_container(state.ui);

        ui_element_position(state.ui, ui_button_position_frame_bottom_left(FEEDBACK_RECT));
        if (ui_button(state.ui, "Back")) {
            state.mode = FEEDBACK_CLOSED;
        }
        ui_element_position(state.ui, ui_button_position_frame_bottom_right(FEEDBACK_RECT, "Send"));
        if (ui_button(state.ui, "Send")) {
            feedback_begin_send();
        }
    } else if (state.mode == FEEDBACK_PREVIEW_SCREENSHOT) {
        state.ui.input_enabled = true;
        ui_frame_rect(state.ui, FEEDBACK_PREVIEW_RECT);

        ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, "Screenshot Preview");
        ui_element_position(state.ui, ivec2(FEEDBACK_PREVIEW_RECT.x + (FEEDBACK_PREVIEW_RECT.w / 2) - (header_text_size.x / 2), FEEDBACK_PREVIEW_RECT.y + 6));
        ui_text(state.ui, FONT_HACK_GOLD, "Screenshot Preview");

        ivec2 button_size = ui_button_size("Back");
        ui_element_position(state.ui, ivec2(FEEDBACK_PREVIEW_RECT.x + (FEEDBACK_PREVIEW_RECT.w / 2) - (button_size.x / 2), 
                                            FEEDBACK_PREVIEW_RECT.y + FEEDBACK_PREVIEW_RECT.h - button_size.y - 8));
        if (ui_button(state.ui, "Back")) {
            state.mode = FEEDBACK_OPEN;
        }
    } else if (state.mode == FEEDBACK_SUCCESS || state.mode == FEEDBACK_ERROR || state.mode == FEEDBACK_VALIDATION || state.mode == FEEDBACK_SENDING) {
        ui_frame_rect(state.ui, SUCCESS_RECT);

        const char* header_text = feedback_get_dialog_header_text(state.mode);
        ivec2 header_text_size = render_get_text_size(FONT_HACK_GOLD, header_text);
        ui_element_position(state.ui, ivec2(SUCCESS_RECT.x + (SUCCESS_RECT.w / 2) - (header_text_size.x / 2), SUCCESS_RECT.y + 6));
        ui_text(state.ui, FONT_HACK_GOLD, header_text);

        ui_begin_column(state.ui, ivec2(SUCCESS_RECT.x + (SUCCESS_RECT.w / 2), SUCCESS_RECT.y + 26), 4);
            if (state.mode == FEEDBACK_SUCCESS) {
                ui_text(state.ui, FONT_HACK_GOLD, "Your feedback has been sent.", true);
                ui_text(state.ui, FONT_HACK_GOLD, "Thank you!", true);
            } else if (state.mode == FEEDBACK_ERROR) {
                ui_text(state.ui, FONT_HACK_GOLD, "Your feedback failed to send.", true);
                ui_text(state.ui, FONT_HACK_GOLD, "Maybe try again?", true);
            } else if (state.mode == FEEDBACK_VALIDATION) {
                ui_text(state.ui, FONT_HACK_GOLD, "Name and Description are required.", true);
                ui_text(state.ui, FONT_HACK_GOLD, "Please fill them out and try again.", true);
            } else if (state.mode == FEEDBACK_SENDING) {
                ui_text(state.ui, FONT_HACK_GOLD, "Your feedback is being sent...", true);
            }
        ui_end_container(state.ui);

        if (state.mode != FEEDBACK_SENDING) {
            const char* button_text = state.mode == FEEDBACK_VALIDATION ? "Back" : "Close";
            ivec2 button_size = ui_button_size(button_text);
            ui_element_position(state.ui, ivec2(SUCCESS_RECT.x + (SUCCESS_RECT.w / 2) - (button_size.x / 2), 
                                                SUCCESS_RECT.y + SUCCESS_RECT.h - button_size.y - 8));
            if (ui_button(state.ui, button_text)) {
                if (state.mode == FEEDBACK_VALIDATION) {
                    state.mode = FEEDBACK_OPEN;
                } else {
                    state.mode = FEEDBACK_CLOSED;
                }
            }
        }

        if (state.mode == FEEDBACK_SENDING) {
            if (SDL_GetThreadState(state.send_report_thread) == SDL_THREAD_COMPLETE) {
                int thread_error;
                SDL_WaitThread(state.send_report_thread, &thread_error);
                state.mode = thread_error == 0 ? FEEDBACK_SUCCESS : FEEDBACK_ERROR;
            }
        }
    } 
}

void feedback_ui_yes_no(UI& ui, const char* prompt, uint32_t* value, bool disabled) {
    const SpriteInfo& dropdown_sprite_info = render_get_sprite_info(SPRITE_UI_DROPDOWN_MINI);
    ui_element_size(state.ui, ivec2(0, dropdown_sprite_info.frame_height));
    ui_begin_row(state.ui, ivec2(0, 0), 0);
        ui_element_position(state.ui, ivec2(0, 2));
        ui_text(state.ui, FONT_HACK_GOLD, prompt);

        ui_element_position(state.ui, ivec2(FEEDBACK_RECT.w - 16 - dropdown_sprite_info.frame_width, 0));
        ui_dropdown(state.ui, UI_DROPDOWN_MINI, value, FEEDBACK_YES_NO_STRS, disabled);
    ui_end_container(state.ui);
}

void feedback_ui_retake_message(UI& ui) {
    ui_insert_padding(state.ui, ivec2(0, 8));
    ui_text(state.ui, FONT_HACK_WHITE, "You can re-take the screenshot by");
    ui_text(state.ui, FONT_HACK_WHITE, "closing and re-opening this menu.");
    ui_text(state.ui, FONT_HACK_WHITE, "Your work will be saved.");
}

void feedback_begin_send() {
    if (state.feedback_name.empty() || state.feedback_desc.empty()) {
        state.mode = FEEDBACK_VALIDATION;
        return;
    }

    state.report.name = (state.feedback_type == FEEDBACK_TYPE_BUG ? "Bug: " : "Feedback: ") + state.feedback_name;
    state.report.description = state.feedback_desc;
    state.report.attachments.clear();
    state.report.attachments.push_back(logger_get_path());
    if (state.include_replay) {
        state.report.attachments.push_back(state.replay_path);
    }
    if (state.include_screenshot) {
        state.report.attachments.push_back(state.screenshot_path);
    }
    state.send_report_thread = SDL_CreateThread(feedback_report_send, "send_report_thread", &state.report);
    if (state.send_report_thread == NULL) {
        log_error("Failed to create send report thread: %s", SDL_GetError());
        state.mode = FEEDBACK_ERROR;
        return;
    }

    state.feedback_type = FEEDBACK_TYPE_BUG;
    state.feedback_name.clear();
    state.feedback_desc.clear();
    state.include_replay = state.has_replay_path;
    state.include_screenshot = 0;

    state.mode = FEEDBACK_SENDING;
}

const char* feedback_get_dialog_header_text(FeedbackMode mode) {
    switch (mode) {
        case FEEDBACK_SUCCESS:
            return "Success";
        case FEEDBACK_ERROR:
            return "Error";
        case FEEDBACK_SENDING:
            return "Sending Feedback";
        case FEEDBACK_VALIDATION:
            return "Feedback Not Sent";
        default:
            return "";
    }
}

void feedback_render() {
    if (state.mode == FEEDBACK_CLOSED) {
        return;
    }

    ui_render(state.ui);
    if (state.mode == FEEDBACK_PREVIEW_SCREENSHOT) {
        render_draw_rect((Rect) { 
            .x = SCREENSHOT_PREVIEW_POSITION.x - 1, 
            .y = SCREENSHOT_PREVIEW_POSITION.y - 1, 
            .w = SCREENSHOT_PREVIEW_WIDTH + 2, 
            .h = SCREENSHOT_PREVIEW_HEIGHT + 2 }, RENDER_COLOR_GOLD);
    }
    render_sprite_batch();

    if (state.screenshot_successful) {
        if (state.mode == FEEDBACK_OPEN) {
            render_screenshot(state.screenshot_position, SCREENSHOT_MINI_SIZE);
        } else if (state.mode == FEEDBACK_PREVIEW_SCREENSHOT) {
            render_screenshot(SCREENSHOT_PREVIEW_POSITION, ivec2(SCREENSHOT_PREVIEW_WIDTH, SCREENSHOT_PREVIEW_HEIGHT));
        }
    }
}

static int feedback_report_send(void* ptr) {
    FeedbackReport* report = (FeedbackReport*)ptr;

    /* Send POST to create card */
    CURL* curl = curl_easy_init();
    if (!curl) {
        return 1;
    }

    // Generate headers
    curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    // Generate body
    std::unordered_map<std::string, std::string> body;
    body["key"] = TRELLO_KEY;
    body["token"] = TRELLO_TOKEN;
    body["idList"] = TRELLO_LIST_ID;
    body["name"] = report->name;
    body["desc"] = report->description;

    // Stringify body
    std::string json_body_str = "{";
    for (auto it : body) {
        json_body_str += "\"" + it.first + "\":\"" + it.second + "\",";
    }
    json_body_str.pop_back(); // remove trailing comma from the final key/value
    json_body_str += "}";
    log_trace("Generated request body: %s", json_body_str.c_str());

    // Set request options
    std::string response;
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, feedback_response_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_URL, TRELLO_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body_str.c_str());

    // Perform request
    long response_status;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    // Check for curl error
    if (res != CURLE_OK) {
        log_error("Feedback send failed: %s", curl_easy_strerror(res));
        return 1;
    }

    // Check for error
    if (response_status != 200) {
        log_error("POST card failed. status: %u body: %s", response_status, response.c_str());
        return 1;
    }

    // Parse id out of response
    size_t id_key_index = response.find("\"id\"");
    size_t id_value_index = id_key_index + strlen("\"id\":\"");
    size_t id_value_end_quote_index = response.find("\"", id_value_index);
    std::string card_id = response.substr(id_value_index, id_value_end_quote_index - id_value_index);
    log_trace("POST card success. ID: %s", card_id.c_str());

    for (std::string attachment_path : report->attachments) {
        feedback_upload_attachment(card_id.c_str(), attachment_path.c_str());
    }

    return 0;
}

size_t feedback_response_write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t new_length = size * nmemb;
    response->append((char*)contents, new_length);
    return new_length;
}

bool feedback_upload_attachment(const char* card_id, const char* filepath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: multipart/form-data");

    curl_mime* mime = curl_mime_init(curl);

    curl_mimepart* key_part = curl_mime_addpart(mime);
    curl_mime_name(key_part, "key");
    curl_mime_data(key_part, TRELLO_KEY, CURL_ZERO_TERMINATED);

    curl_mimepart* token_part = curl_mime_addpart(mime);
    curl_mime_name(token_part, "token");
    curl_mime_data(token_part, TRELLO_TOKEN, CURL_ZERO_TERMINATED);

    curl_mimepart* file_part = curl_mime_addpart(mime);
    curl_mime_name(file_part, "file");
    curl_mime_filedata(file_part, filepath);

    char url[256];
    sprintf(url, "%s/%s/attachments", TRELLO_URL, card_id);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}