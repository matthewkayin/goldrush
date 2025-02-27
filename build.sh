#!/bin/bash
ACTION="$1"

set echo off

# Gold
make $ACTION ASSEMBLY=gold ADDITIONAL_LFLAGS="-lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_mixer -L/opt/homebrew/lib -lenet"
ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ] 
then
echo "error:"$errorlevel && exit
fi

# Reporter
make $ACTION ASSEMBLY=reporter ADDITIONAL_LFLAGS="-L/opt/homebrew/lib -lobjc -lcurl -framework AppKit"
ERRORLEVEL=$?
if [ $ERRORLEVEL -ne 0 ] 
then
echo "error:"$errorlevel && exit
fi