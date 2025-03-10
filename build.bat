@ECHO OFF
SET ACTION=%1

REM Game

make %ACTION% ASSEMBLY=gold ADDITIONAL_LFLAGS="-lSDL2 -lSDL2_ttf -lSDL2_mixer -luser32 -lws2_32 -lwinmm -lenet64 -ldbghelp"
if %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL%)

REM Crash Reporter

make %ACTION% ASSEMBLY=reporter ADDITIONAL_LFLAGS="-luser32 -llibcurl"
if %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL%)