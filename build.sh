#!/bin/bash
# Compilation complète, à lancer dans le shell MSYS2 (MINGW64), depuis ce dossier :
#     bash build.sh
# Résultat dans ./build : pred.exe + net.dll + metadata.dll + matth25v6_db.dll
set -e
mkdir -p build

CXX="g++ -std=gnu++17 -O2 -Wall"
# libgcc, libstdc++ et winpthread dans les binaires : pas de DLL MinGW à distribuer
STATIC="-static -static-libgcc -static-libstdc++"

echo "[1/4] net.dll"
$CXX -shared $STATIC -o build/net.dll net_source.cpp -lwininet

echo "[2/4] matth25v6_db.dll"
$CXX -shared $STATIC -o build/matth25v6_db.dll matth25v6_db.cpp \
    $(pkg-config --cflags sqlite3) $(pkg-config --static --libs sqlite3)

echo "[3/4] metadata.dll"
$CXX -shared $STATIC -o build/metadata.dll metadata_source.cpp \
    $(pkg-config --cflags taglib) $(pkg-config --static --libs taglib)

echo "[4/4] pred.exe"
gcc -O2 -Wall -c main.c -o build/main.o
$CXX -c utils.cpp -o build/utils.o
$CXX -c state_db.cpp -o build/state_db.o $(pkg-config --cflags sqlite3)
g++ -O2 -s $STATIC -o build/pred.exe build/main.o build/utils.o build/state_db.o \
    $(pkg-config --static --libs sqlite3)

echo
echo "OK. DLL dont chaque fichier dépend :"
for f in build/*.exe build/*.dll; do
    echo "== $f"
    objdump -p "$f" | awk '/DLL Name/{print "   " $3}'
done
