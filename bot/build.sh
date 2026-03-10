#!/bin/bash

Platform="linux"
Copy="False"

while getopts "p:c" arg; do
  case $arg in
    p) Platform="$OPTARG" ;;
    c) Copy="True" ;;
    *) echo "Usage: $0 [-p platform] [-c]"; exit 2 ;;
  esac
done

# optional: shift processed options away
shift $((OPTIND - 1))


if [ "$Platform" = "linux" ]; then
    echo "Building for Linux..." 
    cmake -S . -B build
    cmake --build build
    if [ "$Copy" = "True" ]; then
        echo "Copying Linux build to Server"
        cp "build/Knilb" "../server/src/Knilb"
    fi
elif [ "$Platform" = "windows" ]; then
    echo "Building for Windows..."

    # 2. Run CMake and force it to use the Windows cross-compiler
    cmake -S . -B build_win -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc

    # 3. Build the project
    cmake --build build_win

    if [ "$Copy" = "True" ]; then
        echo "Copying Windows build to En-croissant"
        # 4. Copy the created file to en-croissant engines folder (WSL path)
        cp "build_win/Knilb.exe" "/mnt/c/Users/orvil/AppData/Roaming/org.encroissant.app/engines/Knilb"
    fi
else
    echo "Unsupported platform: $Platform"
    exit 1
fi
