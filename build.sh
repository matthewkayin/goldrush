@ECHO OFF
SET PLATFORM=%1
SET ACTION=%2

REM Game

@ECHO "%PLATFORM%"
SET GAME_SHARED_LFLAGS=-lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_mixer
if "%PLATFORM%" == "win32" (
    @ECHO "hey"
    SET GAME_LFLAGS=%GAME_SHARED_LFLAGS% -luser32 -lenet64 -ldbghelp
) else (
    if "%PLATFORM%" == "osx" (
        SET GAME_LFLAGS=%GAME_SHARED_LFLAGS% -L/opt/homebrew/lib -lenet
    )
)
@ECHO "%GAME_SHARED_LFLAGS%"
@ECHO "%GAME_LFLAGS%"

REM make %ACTION% ASSEMBLY=game ADDITIONAL_LFLAGS="%GAME_LFLAGS%"
REM if %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL%)

REM Crash Reporter

SET REPORTER_SHARED_LFLAGS = ""
if "%PLATFORM%" == "win32" (
    SET REPORTER_LFLAGS = "%REPORTER_SHARED_LFLAGS% -luser32 -llibcurl"
) else (
    if "%PLATFORM%" == "osx" (
        SET REPORTER_LFLAGS = "%REPORTER_SHARED_FLAGS% -L/opt/homebrew/lib -lobjc -lcurl -framework AppKit"
    )
)

REM make %ACTION% ASSEMBLY=reporter ADDITIONAL_LFLAGS="%REPORTER_LFLAGS%"
REM if %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL%)