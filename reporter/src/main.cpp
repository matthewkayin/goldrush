#include "platform.h"
#include <curl/curl.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

int report_main();
bool report_send(std::vector<std::string> file_paths);

#if defined(PLATFORM_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return report_main();
}
#elif defined(PLATFORM_OSX)
int main() {
    return report_main();
}
#endif

int report_main() {
    int return_code = system(platform_exe_path());
    if (return_code != 0) {
        if (platform_open_report_window()) {
            report_send(platform_get_report_filenames());
        }
    }

    return 0;
}

bool report_send(std::vector<std::string> file_paths) {
    // const char* password = "seaq jsal lbfg vdce ";
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    if (!curl) {
        printf("error initializing curl\n");
        curl_global_cleanup();
        return false;
    }

    curl_slist* recipients = curl_slist_append(NULL, "matthewkayin@gmail.com");

    curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "From: Gold Rush Crash Reports");
    headers = curl_slist_append(headers, "To: Matthew Madden");
    headers = curl_slist_append(headers, "Subject: Crash Report");

    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* content_part = curl_mime_addpart(mime);
    curl_mime_type(content_part, "text/html");
    curl_mime_data(content_part, "<html><body><p>Someone has sent a crash report.</p></body></html>", CURL_ZERO_TERMINATED);

    for (int file_index = 0; file_index < file_paths.size(); file_index++) {
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_filedata(part, file_paths[file_index].c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "goldrushcrashreports@gmail.com");
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_USERPWD, "goldrushcrashreports@gmail.com:seaq jsal lbfg vdce ");
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "error %i %s\n", res, curl_easy_strerror(res));
    }

    curl_slist_free_all(recipients);
    curl_slist_free_all(headers);
    curl_mime_free(mime);

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return res == CURLE_OK;
}