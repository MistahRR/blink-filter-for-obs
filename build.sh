#!/bin/sh
# Compilation croisée Linux -> Windows x64 avec llvm-mingw (en-têtes libobs d'obs-studio 31.x)
set -e
OBS=${OBS:-../ext/obs-studio/libobs}
CXX=x86_64-w64-mingw32-clang++
B=build; mkdir -p $B/inc
printf '#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n' > $B/inc/obsconfig.h
$CXX -c -O2 -std=c++17 -DNDEBUG -I$B/inc -I$OBS src/blink-filter.cpp -o $B/blink-filter.o
llvm-nm -u $B/blink-filter.o | awk '{print $2}' | grep -E "^(obs_|gs_|blog|bfree|os_|text_lookup|vec)" | sort -u > $B/syms.txt
{ echo "LIBRARY obs.dll"; echo "EXPORTS"; cat $B/syms.txt; } > $B/obs.def
llvm-dlltool -m i386:x86-64 -d $B/obs.def -l $B/libobs.a -D obs.dll
$CXX -shared -static -O2 -s -o $B/obs-blink-filter.dll $B/blink-filter.o $B/libobs.a src/exports.def
echo "OK -> $B/obs-blink-filter.dll"
