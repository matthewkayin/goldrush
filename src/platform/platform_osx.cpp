#include "platform.h"

#ifdef PLATFORM_OSX

#include <cstdio>
#include <mach/mach_time.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <string>
#include "../util.h"

struct PlatformState {
    mach_timebase_info_data_t clock_timebase;
    uint64_t clock_start_time;
    std::vector<std::string> replay_files;
};
static PlatformState state;

void platform_clock_init() {
    mach_timebase_info(&state.clock_timebase);
    state.clock_start_time = mach_absolute_time();
}

double platform_get_absolute_time() {
    uint64_t mach_absolute = mach_absolute_time();
    uint64_t nanos = (double)((mach_absolute - state.clock_start_time) * (uint64_t)state.clock_timebase.numer) / (double)state.clock_timebase.denom;
    return nanos / 1.0e9;
}

void platform_console_write(const char* message, int log_level) {
    #ifdef GOLD_DEBUG
        static const char* TEXT_COLORS[4] = { 
            "31", // ERROR
            "33", // WARN
            "32", // INFO
            "0"  // TRACE
        };
        if (log_level == 0) {
            fprintf(stderr, "\x1B[%sm%s", TEXT_COLORS[log_level], message);
        } else {
            printf("\x1B[%sm%s", TEXT_COLORS[log_level], message);
        }
    #endif
}

void platform_get_resource_path(char* buffer, const char* subpath) {
    #ifdef GOLD_DEBUG
        sprintf(buffer, "../res/%s", subpath);
    #else
        CFBundleRef main_bundle = CFBundleGetMainBundle();
        CFStringEncoding url_encoding = CFStringGetSystemEncoding();

        CFStringRef subpath_str = CFStringCreateWithCString(NULL, subpath, url_encoding);
        CFURLRef url = CFBundleCopyResourceURL(main_bundle, subpath_str, NULL, NULL);
        CFStringRef url_str = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);

        strcpy(buffer, CFStringGetCStringPtr(url_str, url_encoding));
        const char* path = CFStringGetCStringPtr(url_str, url_encoding);

        CFRelease(subpath_str);
        CFRelease(url_str);
    #endif
}

void platform_get_datafile_path(char* buffer, const char* filename) {
    #ifdef GOLD_DEBUG
        sprintf(buffer, "./%s", filename);
    #else
        char logfile_folder_path[128];
        sprintf(logfile_folder_path, "%s/Library/Application Support/goldrush", getenv("HOME"));
        mkdir(logfile_folder_path, 0777);
        sprintf(buffer, "%s/%s", logfile_folder_path, filename);
    #endif
}

void platform_get_replay_path(char* buffer, const char* filename) {
    char replay_folder_path[256];
    platform_get_datafile_path(replay_folder_path, "replays");
    mkdir(replay_folder_path, 0777);
    sprintf(buffer, "%s/%s", replay_folder_path, filename);
}

void platform_search_replays_folder(const char* search_query) {
    state.replay_files.clear();

    char replay_folder_path[256];
    platform_get_datafile_path(replay_folder_path, "replays");

    DIR* directory = opendir(replay_folder_path);
    if (directory == NULL) {
        return;
    }

    int cwd = open(".", O_RDONLY);
    chdir(replay_folder_path);

    dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        std::string filename = std::string(entry->d_name);
        if (!filename_ends_in_rep(filename)) {
            continue;
        }
        if (strstr(filename.c_str(), search_query) == NULL) {
            continue;
        }
        state.replay_files.push_back(filename);
    }

    fchdir(cwd);
    close(cwd);
    closedir(directory);
}

const char* platform_get_replay_file_name(size_t index) {
    return state.replay_files[index].c_str();
}

size_t platform_get_replay_file_count() {
    return state.replay_files.size();
}

#endif