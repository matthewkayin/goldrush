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
    curl_global_init(CURL_GLOBAL_ALL);
    CURL* curl = curl_easy_init();
    if (!curl) {
        printf("error initializing curl\n");
        curl_global_cleanup();
        return false;
    }

    curl_slist* headers = NULL;
    curl_slist_append(headers, "Content-Type: multipart/form-data");

    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* content_part = curl_mime_addpart(mime);
    curl_mime_name(content_part, "content");
    curl_mime_data(content_part, "Received crash report.", CURL_ZERO_TERMINATED);

    for (int file_index = 0; file_index < file_paths.size(); file_index++) {
        char part_name[16];
        sprintf(part_name, "file[%i]", file_index);
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, part_name);
        curl_mime_filedata(part, file_paths[file_index].c_str());
    }

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