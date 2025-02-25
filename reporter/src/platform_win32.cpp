#include "platform.h"

#ifdef PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

const char* platform_exe_path() {
    return "gold.exe";
}

std::vector<std::string> platform_get_report_filenames() {
    std::vector<std::string> filenames;
    filenames.push_back("./logs.log");
    filenames.push_back("./crash.dmp");
    return filenames;
}

bool platform_open_report_window() {
    // Create prompt window
    int messagebox_id = MessageBox(
        NULL, 
        "Gold Rush has crashed.\nWould you like to send a crash report to the developer?", 
        "Crash Reporter", 
        MB_ICONERROR | MB_YESNO | MB_DEFBUTTON2);
    switch (messagebox_id) {
        case IDYES: {
            return true;
            break;
        }
        default: {
            break;
        }
    }

    return false;
}
#endif