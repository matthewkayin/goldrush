#include "platform.h"

#ifdef PLATFORM_OSX
#import <Cocoa/Cocoa.h>
#include <dirent.h>

const char* platform_exe_path() {
    return "./gold";
}

std::vector<std::string> platform_get_report_filenames() {
    std::vector<std::string> filenames;
    filenames.push_back("./logs.log");

    // macos crash reports are saved in ~/Library/Logs/DiagnosticReports in the format of <appname>-YYYY-MM-DD-HHMMSS.extension
    // The extension is "ips" on later versions of macos, but it might be different on older versions
    // This code searches the users DiagnosticReports folder and snags the latest file with the "gold" appname
    DIR* directory;
    dirent* entry;
    char diagnostic_directory_path[128];
    sprintf(diagnostic_directory_path, "%s/Library/Logs/DiagnosticReports", getenv("HOME"));
    directory = opendir(diagnostic_directory_path);
    std::string most_recent_ips = "";
    if (directory) {
        while ((entry = readdir(directory)) != NULL) {
            std::string entry_filename = std::string(entry->d_name);
            if (entry_filename.find("gold") != 0) {
                continue;
            }
            if (most_recent_ips.length() == 0 || entry_filename > most_recent_ips) {
                most_recent_ips = entry_filename;
            }
        }
        closedir(directory);
    }

    if (most_recent_ips.length() != 0) {
        filenames.push_back(std::string(diagnostic_directory_path) + "/" + most_recent_ips);
    }

    return filenames;
}

bool platform_open_report_window() {
    NSAlert* alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"Yes"];
    [alert addButtonWithTitle:@"No"];
    [alert setMessageText:@"Gold Rush has crashed."];
    [alert setInformativeText:@"Would you like to send a crash report to the developer?"];
    [alert setAlertStyle:NSAlertStyleCritical];

    bool response = false;
    if ([alert runModal] == NSAlertFirstButtonReturn) {
        response = true;
    }
    [alert release];

    return response;
}
#endif