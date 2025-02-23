#include "report.h"

#include <curl/curl.h>

bool report_send(const char* logfile_path, const char* dmpfile_path) {
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        return false;
    }

    curl_slist* headers = NULL;
    curl_slist_append(headers, "Content-Type: multipart/form-data");

    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* content_part = curl_mime_addpart(mime);
    curl_mime_name(content_part, "content");
    curl_mime_data(content_part, "Received crash report.", CURL_ZERO_TERMINATED);

    curl_mimepart* dmpfile_part = curl_mime_addpart(mime);
    curl_mime_name(dmpfile_part, "file[0]");
    curl_mime_filedata(dmpfile_part, dmpfile_path);

    curl_mimepart* logfile_part = curl_mime_addpart(mime);
    curl_mime_name(logfile_part, "file[1]");
    curl_mime_filedata(logfile_part, logfile_path);

    curl_easy_setopt(curl, CURLOPT_URL, "https://discord.com/api/webhooks/1342861844083118193/aqnZf9O0VSmhazXBz1-I2XtQg5Vu2RUCZr8b_Oo_aQkceViDAVLHOBumRHXGzyJGzGXZ");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return res == CURLE_OK;
}