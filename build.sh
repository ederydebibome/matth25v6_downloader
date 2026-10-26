#!/bin/bash
# Compilation complète — à lancer dans le shell MSYS2 (MINGW64 recommandé), depuis ce dossier :
#     bash build.sh
# Résultat dans ./build : PKP_Downloader.exe + net.dll + img.dll + metadata.dll + matth25v6_db.dll
set -e
mkdir -p build

CXX="g++ -std=gnu++17 -O2 -Wall"
# Runtime GCC (libgcc, libstdc++, winpthread) embarqué : aucune DLL MinGW à distribuer
STATIC="-static -static-libgcc -static-libstdc++"

echo "[1/5] net.dll";            $CXX -shared $STATIC -o build/net.dll net_source.cpp -lwininet
echo "[2/5] matth25v6_db.dll";   $CXX -shared $STATIC -o build/matth25v6_db.dll matth25v6_db.cpp \
                                     $(pkg-config --cflags sqlite3) $(pkg-config --static --libs sqlite3)
echo "[3/5] metadata.dll";       $CXX -shared $STATIC -o build/metadata.dll metadata_source.cpp \
                                     $(pkg-config --cflags taglib) $(pkg-config --static --libs taglib)

# ImageMagick en lien DYNAMIQUE (le lien statique de MSYS2 est défectueux : libxml2/fontconfig).
# img.dll a donc besoin des DLL d'ImageMagick au moment de l'exécution.
MAGICK=$(pkg-config --list-all | awk '/^Magick\+\+/{print $1; exit}')
echo "[4/5] img.dll ($MAGICK, dynamique)"
$CXX -shared -static-libgcc -static-libstdc++ -o build/img.dll img_source.cpp \
    $(pkg-config --cflags $MAGICK) $(pkg-config --libs $MAGICK)

echo "[5/5] pred.exe"
for f in Ressources/img_FR.png Ressources/img_EN.png Ressources/Lora-Bold.ttf; do
    [ -f "$f" ] || { echo "ERREUR : fichier manquant : $PWD/$f"; exit 1; }
done
windres resources.rc -O coff -o build/resources.o
gcc -O2 -Wall -c main.c -o build/main.o
$CXX -c utils.cpp -o build/utils.o
$CXX -c state_db.cpp -o build/state_db.o $(pkg-config --cflags sqlite3)
g++ -O2 -s $STATIC -o build/pred.exe build/main.o build/utils.o build/state_db.o build/resources.o \
    $(pkg-config --static --libs sqlite3)

echo; echo "OK. DLL dont chaque fichier dépend :"
for f in build/*.exe build/*.dll; do echo "== $f"; objdump -p "$f" | awk '/DLL Name/{print "   " $3}'; done
