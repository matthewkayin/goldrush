#include "defines.h"
#include "logger.h"
#include <ctime>
#include <cstdio>

int gold_main(int argc, char** argv);

#if defined PLATFORM_WIN32 && !defined GOLD_DEBUG
#include <windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return gold_main(0, NULL);
}
#else
int main(int argc, char** argv) {
    return gold_main(argc, argv);
}
#endif

int gold_main(int argc, char** argv) {
    char logfile_path[128];
    time_t _time = time(NULL);
    tm _tm = *localtime(&_time);
    sprintf(logfile_path, "./logs/%d-%02d-%02dT%02d%02d%02d.log", _tm.tm_year + 1900, _tm.tm_mon + 1, _tm.tm_mday, _tm.tm_hour, _tm.tm_min, _tm.tm_sec);

    logger_init(logfile_path);
    log_info("hey fren");
    logger_quit();
    
    return 0;
}