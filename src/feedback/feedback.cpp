#include "feedback.h"

#include "token.h"
#include "core/logger.h"
#include "core/filesystem.h"
#include "util.h"
#include <curl/curl.h>
#include <unordered_map>
#include <string>

#ifdef PLATFORM_WIN32
#include <windows.h>
#include <DbgHelp.h>

LONG WINAPI feedback_on_crash(EXCEPTION_POINTERS* exception_pointers);
#endif

static const char* TRELLO_KEY = "2644097bec308192331862ebca64ce13";
static const char* TRELLO_URL = "https://api.trello.com/1/cards";
static const char* TRELLO_LIST_ID = "684c9d51f095781d10148d44";

size_t feedback_response_write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
bool feedback_upload_attachment(const char* card_id, const char* filepath);

void feedback_init() {
    curl_global_init(CURL_GLOBAL_ALL);
    #ifdef PLATFORM_WIN32
        SetUnhandledExceptionFilter(feedback_on_crash);
    #endif
}

void feedback_quit() {
    curl_global_cleanup();
}

#ifdef PLATFORM_WIN32
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
    feedback_send((FeedbackTicket) {
        .name = "Crash Report",
        .description = "Gold Rush caught an unhandled exception.",
        .attachments = { std::string(dumpfile_fullpath), logger_get_path() }
    });

    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

bool feedback_send(FeedbackTicket ticket) {
    /* Send POST to create card */
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
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
    body["name"] = ticket.name;
    body["desc"] = ticket.description;

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
        return false;
    }

    // Check for error
    if (response_status != 200) {
        log_error("POST card failed. status: %u body: %s", response_status, response.c_str());
        return false;
    }

    // Parse id out of response
    size_t id_key_index = response.find("\"id\"");
    size_t id_value_index = id_key_index + strlen("\"id\":\"");
    size_t id_value_end_quote_index = response.find("\"", id_value_index);
    std::string card_id = response.substr(id_value_index, id_value_end_quote_index - id_value_index);
    log_trace("POST card success. ID: %s", card_id.c_str());

    for (std::string attachment_path : ticket.attachments) {
        feedback_upload_attachment(card_id.c_str(), attachment_path.c_str());
    }

    return true;
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