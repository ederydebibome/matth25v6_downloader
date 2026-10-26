# matth25v6_downloader

Un petit programme Windows en ligne de commande qui télécharge les prédications audio de Philippe Kacou et les cantiques depuis les serveurs de philippekacou.org, et qui range le tout proprement : bon nom de fichier, bons tags MP3, pochette générée avec le numéro et le titre dessus.

J'ai écrit ça au départ parce que j'en avais marre de remplir les tags à la main et de me demander si mes fichiers étaient encore à jour. Le programme se souvient de ce qu'il a déjà téléchargé, ne retélécharge que ce qui a changé côté serveur, et peut tourner autant de fois que tu veux sans rien casser.

## Sommaire

1. [Ce que ça fait, concrètement](#ce-que-ça-fait-concrètement)
2. [Où vont les fichiers](#où-vont-les-fichiers)
3. [Les questions posées au lancement](#les-questions-posées-au-lancement)
4. [Les fichiers du projet, un par un](#les-fichiers-du-projet-un-par-un)
5. [Compiler avec MSYS2](#compiler-avec-msys2-la-méthode-qui-marche)
6. [Compiler avec MSVC](#compiler-avec-msvc-visual-studio--non-testé)
7. [Les détails qui comptent](#les-détails-qui-comptent-si-tu-veux-lire-le-code)
8. [Limites connues](#limites-connues)

## Ce que ça fait, concrètement

Tu lances `pred.exe`. Il te pose quatre ou cinq questions (voir plus bas), puis il fait tout seul :

1. **Il vérifie les bases de données officielles.** Le serveur publie cinq petits fichiers SQLite : `common.db` (cantiques, albums, chanteurs...) et une base par langue (`matth25v6_fr.db`, `_en.db`, `_es.db`, `_pt.db`) qui contient la liste des prédications. Pour chacune, le programme demande à l'API la date de dernière mise à jour et la compare à celle qu'il a gardée en mémoire. Si rien n'a bougé, il passe. Sinon il retélécharge la base.
2. **Il lit les cantiques actifs** dans `common.db` et télécharge ceux qui sont absents ou modifiés.
3. **Il parcourt les langues que tu as choisies**, et pour chaque prédication active :
   - il regarde si le fichier MP3 existe déjà sur le disque ;
   - il compare le `updated_at` du serveur avec celui qu'il a mémorisé au dernier téléchargement ;
   - s'il manque quelque chose, ou si la version a changé, il télécharge ;
   - il génère une pochette à partir d'un gabarit PNG, en écrivant dessus « Kacou 42 : Le titre de la prédication » ;
   - il écrit les tags ID3 (titre, artiste, album, genre, numéro de piste, année, langue) et incruste la pochette dans le MP3.
4. **Il t'affiche un résumé** : combien de prédications ont été téléchargées ou mises à jour, par langue.

Le point sur lequel j'ai passé le plus de temps, c'est de ne jamais casser un fichier valide. Un téléchargement qui échoue ne remplace rien (voir « Les détails qui comptent » plus bas), et la version n'est mémorisée qu'après un succès complet. Si ta connexion lâche au milieu, la prochaine exécution reprend là où il faut.

## Où vont les fichiers

Tout est relatif au dossier de `pred.exe` :

```
pred.exe
net.dll  img.dll  metadata.dll  matth25v6_db.dll
pred_state.db                  <- la mémoire du programme
Ressources\                    <- bases .db, gabarits PNG, police
img\                           <- pochettes (si tu choisis de les garder)
Matthieu25v6\
  Music\
    Predications\              <- (ou Predications\Fr, \En... selon ton choix)
    Cantiques\
```

Les pochettes sont volontairement en dehors du dossier `Matthieu25v6`. Ce dossier est fait pour être copié sur un téléphone, et le téléphone n'a pas besoin des PNG : elles sont déjà dans les MP3.

## Les questions posées au lancement

| Question | Défaut | Ce que ça change |
|---|---|---|
| Conserver les images après traitement ? | `O` | Garde ou supprime les PNG de pochette dans `img\` une fois incrustés. |
| Séparer les prédications par langue ? | `N` | `O` : un sous-dossier par langue. `N` : tout dans `Predications\`. |
| Langues à télécharger | `En,Fr` | Parmi `Fr`, `En`, `Es`, `Pt`. Séparées par virgule, point-virgule ou espace. Les doublons et les valeurs invalides sont ignorés. |
| Couleur du texte sur la pochette | `#30d5c8` | Un nom (`red`, `white`) ou un code hexadécimal. Le `#` est ajouté tout seul si tu écris `30d5c8`. |

Entrée vide = valeur par défaut.

## Les fichiers du projet, un par un

Le programme est découpé en un exécutable et quatre DLL qu'il charge au démarrage avec `LoadLibrary`. Ce n'est pas la façon la plus simple de faire, je le reconnais : c'est une habitude que j'ai gardée de mes autres projets, et ça m'a permis de garder chaque bibliothèque lourde (ImageMagick, TagLib, SQLite) isolée de l'exécutable. Le `main.c` récupère ensuite chaque fonction dont il a besoin avec `GetProcAddress`, et les types de ces fonctions sont déclarés dans `projetc.h`.

Voici le rôle de chaque fichier, dans l'ordre où le programme s'en sert.

### `main.c` : le chef d'orchestre

C'est le seul fichier qui connaît le déroulement complet. Dans l'ordre, `main()` :

1. passe la console en UTF-8 (sinon les accents s'affichent mal) ;
2. charge `net.dll` en premier, parce que tout le reste a besoin de télécharger, y compris les bases de données ;
3. ouvre `pred_state.db` (la mémoire du programme) ;
4. extrait les ressources embarquées (gabarits et police) si elles ne sont pas déjà sur le disque ;
5. synchronise les cinq bases officielles ;
6. pose les questions de configuration ;
7. charge les trois autres DLL (`img.dll`, `metadata.dll`, `matth25v6_db.dll`) et récupère leurs fonctions ;
8. télécharge les cantiques actifs ;
9. boucle sur les langues choisies, lit la liste des prédications de chacune et appelle `process_sermon()` pour chaque prédication ;
10. affiche le résumé et libère les DLL.

Les fonctions importantes de ce fichier :

- `doit_telecharger()` : la décision « faut-il télécharger ? ». Elle répond oui dans trois cas : le MP3 est absent du disque, aucune version n'est mémorisée pour lui, ou la version du serveur est différente de celle mémorisée. Elle renvoie aussi la raison, qui est affichée dans la console.
- `process_sermon()` : tout le traitement d'une prédication (dossier, nom de fichier, téléchargement, pochette, tags, mémorisation de la version).
- `telecharger_cantiques_actifs()` : la même chose pour les cantiques, sans pochette.
- `sanitize_filename()`, `forcer_extension_mp3()`, `year_from_date()` : de petits utilitaires pour nettoyer les noms de fichiers, imposer `.mp3` et extraire l'année d'une date de publication.

### `utils.cpp` / `utils.h` : la boîte à outils

Tout ce qui n'est ni la logique de téléchargement ni la lecture des bases. En vrac :

- **Chemins et fichiers en UTF-8** : `utf8_vers_wide()`, `fichier_existe()`, `creer_dossier_recursif()`, `supprimer_fichier()`, `obtenir_dossier_executable()`. Ce sont des enveloppes autour des API Windows « W » (UTF-16), pour que les accents ne soient jamais abîmés.
- **Ressources embarquées** : `extraire_ressource()` sort un fichier caché dans l'exécutable (gabarit PNG ou police) vers le disque. `initialiser_ressources()` le fait pour les trois ressources, seulement si elles n'existent pas déjà.
- **Dialogue avec l'API** : `telecharger_base_donnees()` demande à l'API l'URL d'une base, la lit dans la réponse JSON, puis télécharge la base. `recuperer_update_langue()` demande la date de dernière mise à jour d'une langue.
- **Synchronisation des bases** : `synchroniser_bases_donnees()` fait le tour des cinq bases (common, fr, en, es, pt), compare la date du serveur avec celle de `pred_state.db`, et ne retélécharge que ce qui a changé. Si l'API est injoignable mais que la base est déjà sur le disque, elle continue avec la base locale au lieu d'échouer.
- **Configuration** : `configurer_application()` pose les questions, fixe les dossiers de destination et normalise les réponses (langues valides, couleur avec `#`). `lire_input()` gère l'entrée vide = valeur par défaut.
- `copier_texte_utf8()` : une copie de chaîne qui ne coupe jamais un caractère accentué en deux quand elle tronque.

### `state_db.cpp` / `state_db.h` : la mémoire du programme

C'est une petite base SQLite locale, `pred_state.db`, créée au premier lancement à côté de l'exécutable. Elle est compilée directement dans `pred.exe` (ce n'est pas une DLL). Elle sert à une seule chose : se souvenir de ce qui a déjà été fait, pour ne pas le refaire.

Elle contient trois tables :

| Table | Contenu | Utilisée ? |
|---|---|---|
| `langues_updates` | La date de mise à jour de chaque base officielle (`common`, `fr`, `en`, `es`, `pt`) au moment où je l'ai téléchargée. | Oui |
| `audio_state` | Pour chaque fichier audio réellement présent sur le disque (`scope` = langue ou `cantiques`, `item_id` = numéro de prédication ou id du cantique), la version du serveur correspondante. | Oui, c'est la table principale |
| `predications_updates` | Un suivi par prédication et par langue. | Prévue dans le schéma, pas utilisée aujourd'hui : le suivi réel passe par `audio_state` |

La règle importante : la version d'un fichier n'est écrite dans `audio_state` qu'**après** un téléchargement réussi. Si quelque chose plante avant, la version n'est pas mémorisée, et le fichier sera retenté à la prochaine exécution.

### `matth25v6_db.cpp` / `matth25v6_db.h` → `matth25v6_db.dll` : le lecteur des bases officielles

C'est le plus gros fichier du projet, et il fait un travail simple : **il lit les bases de données SQLite publiées par le serveur** (`common.db` et les `matth25v6_XX.db`) et rend leur contenu au programme sous forme de structures C, sans que `main.c` ait à écrire une seule requête SQL.

Ces bases sont celles que l'application officielle utilise elle-même, donc elles contiennent bien plus que ce dont ce téléchargeur a besoin. J'ai écrit la DLL pour couvrir l'ensemble, parce que c'est un morceau réutilisable pour d'autres projets. Les bases sont ouvertes en **lecture seule** (`SQLITE_OPEN_READONLY`), avec une seule exception, `LastUpdate_Set`, qui peut écrire une date de mise à jour dans `common.db`.

**Dans `common.db`** (ce qui est commun à toutes les langues) :

| Structure | Contenu | Fonctions |
|---|---|---|
| `M25Album` | Les albums de cantiques (avec un drapeau « prière ») | `Album_GetAll`, `Album_GetById` |
| `M25Sing` | Les cantiques, déjà « enrichis » par une jointure avec l'album et le chanteur : titre, URL audio, nom de fichier, paroles, durée, ordre, nom du chanteur, titre de l'album | `Sing_GetAll`, `Sing_GetByAlbum`, `Sing_GetBySinger`, `Sing_GetById` |
| `M25Singer`, `M25Brother` | Les chanteurs et les frères (nom, téléphone, avatar, réseaux sociaux, e-mail) | `Singer_GetAll`, `Brother_GetAll`, `Brother_GetById` |
| `M25Assembly` | Les assemblées, avec ville et pays | `Assembly_GetAll`, `Assembly_GetByCountry` |
| `M25Langue` | Les langues disponibles, avec leurs initiales et leurs drapeaux (langue africaine, affichage sur la page des prédications...) | `Langue_GetAll`, `Langue_GetActive` |
| `M25LastUpdate` | Les dates de dernière synchronisation par langue | `LastUpdate_GetAll`, `LastUpdate_GetByLangue`, `LastUpdate_Set` |
| `M25Concordance` | Les concordances entre prédications et versets | `Concordance_GetBySermon`, `Concordance_GetByVerse` |
| `M25ImageSermonMeta` | Les images de prédication stockées en BLOB dans la base | `ImageSermon_GetMeta`, `ImageSermon_GetFile` (la DLL alloue le buffer, `M25_Free` le libère) |

**Dans chaque base de langue** (`matth25v6_fr.db`, etc.) :

| Structure | Contenu | Fonctions |
|---|---|---|
| `M25Sermon` | Les prédications : numéro, titre, sous-titre, lien audio du cloud, nom du fichier audio, durée, date de publication, PDF, EPUB, vidéo, légende, `updated_at` | `Sermon_GetAll`, `Sermon_GetByNumber`, `Sermon_GetCount`, `Sermon_Search` (recherche dans titre et sous-titre) |
| `M25Verse` | Les versets de chaque prédication, avec renvois | `Verse_GetByRef`, `Verse_GetBySermon`, `Verse_SearchFirst`, `Verse_SearchAll`, `Verse_GetCount` |
| `M25Photo`, `M25Video`, `M25Event` | Photos, vidéos et événements | `Photo_GetAll`, `Video_GetAll`, `Event_GetAll` |
| `M25Information`, `M25Travel`, `M25Testimony` | Les actualités, voyages et témoignages | `Information_GetAll`, `Travel_GetAll`, `Testimony_GetAll` |
| `M25Book`, `M25Biography` | Les livres (PDF et EPUB) et la biographie | `Book_GetAll`, `Biography_Get` |

**Ce que le téléchargeur utilise vraiment :** seulement trois fonctions, `Sermon_GetCount` et `Sermon_GetAll` (pour la liste des prédications d'une langue) et `Sing_GetAll` (pour les cantiques). Les autres sont là, testées à l'usage, mais le programme ne les appelle pas.

Quelques règles de cette DLL que j'ai gardées partout :
- Les fonctions renvoient un code (`M25_OK`, `M25_ERROR`, `M25_NOT_FOUND`, `M25_INVALID_ARG`) et remplissent un tableau fourni par l'appelant, avec sa capacité, au lieu d'allouer elles-mêmes. Ça évite que l'allocation et la libération se retrouvent de part et d'autre d'une frontière de DLL (sauf pour les BLOB, où `M25_Free` règle le problème).
- Seules les lignes actives (`is_active = 1`) sont renvoyées pour les cantiques, albums, frères et prédications.

### `net_source.cpp` / `net_header.h` → `net.dll` : le téléchargeur

Une seule fonction exportée : `DownloadFile(url, cheminSortie)`. Elle utilise WinINet (la pile HTTP de Windows, donc elle respecte les paramètres de proxy du système). Elle sert à tout : les prédications, les cantiques, les bases de données, et même les petites réponses JSON de l'API.

Elle est écrite pour ne jamais laisser un fichier à moitié téléchargé à la place d'un bon fichier. Détails dans la section « Les détails qui comptent ».

### `metadata_source.cpp` / `metadata_header.h` → `metadata.dll` : les tags et la pochette

Enveloppe autour de TagLib. Deux fonctions exportées :

- `SetTags(chemin, paires, nombre)` : applique une liste de paires clé/valeur (`TITLE`, `ARTIST`, `ALBUM`, `GENRE`, `TRACKNUMBER`, `DATE`, `LANGUAGE`) à un fichier audio. Pour un MP3, elle écrit de l'ID3v2.4 en UTF-8 et supprime l'ID3v1.
- `SetCoverImage(chemin, image)` : incruste une image comme pochette « Front Cover » (frame APIC pour un MP3, bloc PICTURE pour un FLAC).

### `img_source.cpp` / `img_header.h` → `img.dll` : la fabrique de pochettes

Une fonction exportée : `CreateImage(...)`. Elle ouvre un gabarit PNG avec Magick++ (l'interface C++ d'ImageMagick), écrit dessus un texte du genre « Kacou 42 : Titre de la prédication », avec retour automatique à la ligne quand le texte dépasse la largeur disponible, centré horizontalement, puis enregistre le résultat. Tous les réglages (police, taille, couleur, marges, position verticale) sont des paramètres, donc la DLL ne contient rien de spécifique à ce projet.

### `projetc.h` : le contrat entre l'exécutable et les DLL

Il contient les définitions de types de pointeurs de fonction (`create_image_t`, `download_file_t`, `set_tags_t`, `sermon_get_all_t`...) que `main.c` utilise avec `GetProcAddress`. Si tu changes la signature d'une fonction exportée, c'est ici qu'il faut aussi la changer, sinon ça compile mais ça plante à l'exécution.

### `resources.rc` / `resource.h` : les fichiers cachés dans l'exécutable

`resources.rc` dit à `windres` d'embarquer dans `pred.exe` les deux gabarits PNG (`img_FR.png`, `img_EN.png`) et la police `Lora-Bold.ttf`. `resource.h` leur donne des numéros (101, 102, 103) pour que `main.c` puisse les retrouver. Au premier lancement, ils sont extraits dans `Ressources\`, ce qui permet de distribuer un seul fichier `.exe` plus ses DLL, sans dossier d'images à côté.

### `Ressources/` : les fichiers embarqués

- `img_FR.png` : gabarit de pochette utilisé pour le français et l'espagnol.
- `img_EN.png` : gabarit utilisé pour les autres langues.
- `Lora-Bold.ttf` : la police du texte sur la pochette.
- `check.py` : un petit script de debug que j'ai écrit pour vérifier que la colonne `updated_at` est bien remplie dans la base française. Il n'est pas nécessaire pour compiler ni pour utiliser le programme. Son chemin de base de données est écrit en dur pour ma machine : adapte-le si tu veux t'en servir.

### `build.sh` : le script de compilation

Compile les quatre DLL puis l'exécutable pour MSYS2. Détaillé dans la section suivante.

### `.gitignore`

Exclut `build/`, les exécutables, les fichiers objets, les ressources compilées, les fichiers de debug de Visual Studio et le fichier `.env`. Seules les sources sont dans le dépôt.

## Compiler avec MSYS2 (la méthode qui marche)

C'est la méthode que j'utilise, et `build.sh` est fait pour elle.

### 1. Installer MSYS2

Télécharge l'installeur sur [msys2.org](https://www.msys2.org) et installe-le (par défaut dans `C:\msys64`). Ensuite, ouvre le terminal **MSYS2 MINGW64** (pas « MSYS », pas « UCRT64 », pas « CLANG64 »). La différence compte : les paquets de la commande suivante sont ceux de la chaîne MINGW64.

### 2. Installer les dépendances

Dans ce terminal :

```bash
pacman -Syu
```

Il peut te demander de fermer et rouvrir le terminal. Fais-le, relance la commande jusqu'à ce qu'il n'y ait plus rien à mettre à jour. Puis :

```bash
pacman -S --needed git \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-sqlite3 \
    mingw-w64-x86_64-taglib \
    mingw-w64-x86_64-imagemagick
```

Ça fournit le compilateur (`gcc`/`g++`/`windres`), `pkg-config`, SQLite, TagLib et ImageMagick avec Magick++.

### 3. Récupérer le code et compiler

```bash
git clone https://github.com/ederydebibome/matth25v6_downloader.git
cd matth25v6_downloader
bash build.sh
```

Le script fait cinq étapes et te les affiche :

1. `net.dll`, avec `-lwininet`.
2. `matth25v6_db.dll`, lié à SQLite.
3. `metadata.dll`, lié à TagLib.
4. `img.dll`, lié à ImageMagick.
5. `pred.exe`, avec `windres` pour embarquer les ressources, puis `gcc`/`g++` pour le code.

Tout arrive dans le dossier `build/`. À la fin, le script affiche pour chaque fichier la liste des DLL dont il dépend, ce qui est pratique pour vérifier ce qu'il te faut pour le distribuer.

### 4. Lancer

```bash
cd build
./pred.exe
```

Ou double-clique sur `pred.exe` dans l'explorateur. Il faut que les quatre DLL soient dans le même dossier que l'exécutable, sinon il s'arrête au chargement.

### Ce que fait `build.sh` de particulier

- **Le runtime de GCC est lié en statique** (`-static-libgcc -static-libstdc++`), pour que tu n'aies pas à distribuer `libgcc_s_seh-1.dll`, `libstdc++-6.dll` et `libwinpthread-1.dll` à côté.
- **SQLite et TagLib sont liés en statique** dans leur DLL respective.
- **ImageMagick est lié en dynamique**, et c'est la seule exception. J'ai essayé en statique : le lien statique d'ImageMagick fourni par MSYS2 est cassé (problèmes avec libxml2 et fontconfig). `img.dll` a donc besoin des DLL d'ImageMagick au moment de l'exécution. Tant que tu lances le programme depuis le terminal MINGW64, elles sont trouvées. Pour distribuer le programme à quelqu'un qui n'a pas MSYS2, il faudra copier les DLL listées par `objdump` à la fin du script (regarde la ligne `== build/img.dll`).
- Le script vérifie que les trois fichiers de `Ressources/` existent avant de lancer `windres`, parce que l'erreur de `windres` quand un fichier manque n'est pas très parlante.
- `set -e` : au premier échec, tout s'arrête. Lis la dernière ligne avant l'arrêt, c'est presque toujours là qu'est la cause.

### Problèmes classiques

| Symptôme | Cause probable |
|---|---|
| `pkg-config: command not found` | Tu n'es pas dans le terminal MINGW64, ou `mingw-w64-x86_64-pkgconf` n'est pas installé. |
| `Package taglib was not found` | `pacman -S mingw-w64-x86_64-taglib`. |
| Le script s'arrête sur `img.dll` avec une erreur de quantum ou de HDRI | `img_source.cpp` contient des valeurs par défaut (Q16, HDRI désactivé) qui doivent correspondre à l'ImageMagick installé. En temps normal `pkg-config` les fournit lui-même, mais regarde ça si la version d'ImageMagick change. |
| `pred.exe` s'ouvre et se ferme, ou affiche « Erreur net.dll » | Les DLL ne sont pas à côté de l'exe. |
| « Erreur img.dll » alors que le fichier est là | Les DLL d'ImageMagick ne sont pas trouvées : lance depuis le terminal MINGW64, ou copie-les à côté. |

## Compiler avec MSVC (Visual Studio) : non testé

Je n'ai pas encore compilé ce projet avec MSVC, et `build.sh` ne couvre pas ce cas. Voici comment je m'y prendrais, à vérifier. Si tu essaies et que ça marche (ou pas), une issue ou une pull request avec ce qu'il faut corriger sera très bien accueillie.

**Prérequis :** Visual Studio avec la charge de travail « Développement Desktop en C++ », et [vcpkg](https://github.com/microsoft/vcpkg) pour les bibliothèques.

```bat
vcpkg install sqlite3:x64-windows taglib:x64-windows imagemagick:x64-windows
vcpkg integrate install
```

Ouvre ensuite **x64 Native Tools Command Prompt for VS**. Les commandes ci-dessous sont l'équivalent approximatif de `build.sh` ; adapte `VCPKG` à ton installation.

```bat
set VCPKG=C:\vcpkg\installed\x64-windows
mkdir build

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD net_source.cpp /Fe:build\net.dll wininet.lib

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD matth25v6_db.cpp /I%VCPKG%\include ^
   /Fe:build\matth25v6_db.dll /link /LIBPATH:%VCPKG%\lib sqlite3.lib

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD metadata_source.cpp /I%VCPKG%\include ^
   /Fe:build\metadata.dll /link /LIBPATH:%VCPKG%\lib tag.lib

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD img_source.cpp /I%VCPKG%\include\ImageMagick-7 ^
   /Fe:build\img.dll /link /LIBPATH:%VCPKG%\lib CORE_RL_Magick++_.lib CORE_RL_MagickCore_.lib

rc /nologo /fo build\resources.res resources.rc
cl /nologo /O2 /utf-8 /Fe:build\pred.exe main.c utils.cpp state_db.cpp build\resources.res ^
   /I%VCPKG%\include /link /LIBPATH:%VCPKG%\lib sqlite3.lib user32.lib
```

Ce que je sais et ce que je suppose :

- **Les noms des `.lib` d'ImageMagick et de TagLib dépendent de la version installée par vcpkg.** Va voir dans `%VCPKG%\lib` ce qui s'y trouve vraiment et corrige les noms.
- Le code est écrit en UTF-8 avec des accents : l'option `/utf-8` est indispensable avec MSVC, sinon les chaînes sortent déformées.
- Mélanger du C (`main.c`) et du C++ (`utils.cpp`, `state_db.cpp`) dans une seule ligne `cl` est possible, mais si tu as des soucis de liaison, compile chaque fichier en `.obj` séparément.
- Les DLL MSVC ont besoin des DLL de vcpkg (sqlite3, tag, ImageMagick) à côté pour tourner : copie-les dans `build\`.
- Lance `pred.exe` depuis `build\`, avec toutes les DLL à côté.

## Les détails qui comptent (si tu veux lire le code)

Quelques choix que j'ai faits et qu'il vaut mieux connaître avant de modifier quoi que ce soit.

**Tout est en UTF-8.** Chemins, titres, URL. Sous Windows, les API « A » utilisent la page de code ANSI et abîment les accents des titres français, donc partout où j'ai besoin de toucher le système de fichiers, je convertis en UTF-16 et j'appelle la version « W » de l'API. Si tu ajoutes du code, ne mets jamais un `char*` dans un `CreateFileA` ou un `fopen`.

**Les téléchargements sont atomiques.** `net.dll` écrit dans `fichier.mp3.part`, vérifie le code HTTP (un 404 n'est jamais enregistré comme un MP3), compare la taille reçue à `Content-Length`, et seulement ensuite remplace le fichier final avec `MoveFileEx`. Si ça rate, l'ancienne version reste intacte.

**Le nom des fichiers est nettoyé** : les caractères interdits sous Windows (`< > : " / \ | ? *`) et les caractères de contrôle deviennent `_`. Pour les cantiques, l'extension est forcée en `.mp3`.

**Les tags sont en ID3v2.4, texte en UTF-8.** L'ID3v1, limité au Latin-1, est supprimé du fichier et n'est pas recréé. La pochette est enregistrée comme « Front Cover ». Le code gère aussi le FLAC, au cas où.

**Les tags d'une prédication** : titre, artiste (« Matthieu 25v6 » ou « Matthew 25v6 » selon la langue), album (« Le Message de Matthieu 25v6 en audio » ou « The Message of Matthew 25v6 in Audio »), genre « Sermon », numéro de piste, année de publication, langue.

**Les pochettes** sont dessinées sur un gabarit (`img_FR.png` pour le français et l'espagnol, `img_EN.png` pour le reste) avec la police Lora Bold à 35 pt, texte centré et retour à la ligne automatique sur une largeur de 760 px, en commençant à y = 460. Ces valeurs sont écrites dans `main.c` et `img_source.cpp` : elles correspondent à mes gabarits, change-les si tu utilises d'autres images.

**L'API.** Le programme appelle `https://api.philippekacou.org/api/...` pour deux choses : connaître la date de mise à jour d'une langue, et obtenir l'URL de téléchargement d'une base. Les réponses JSON sont lues à la main (recherche de `"download_url":"` et de `"updated_at":"`) plutôt qu'avec une vraie bibliothèque JSON. C'est suffisant pour le format actuel, mais ça casserait si le serveur changeait la forme de ses réponses.

## Limites connues

- Windows uniquement : WinINet, `LoadLibrary`, les API « W »...
- Pas de reprise partielle d'un MP3 interrompu : il est retéléchargé en entier.
- La lecture des réponses JSON est volontairement minimale (voir ci-dessus).
- Les chemins sont limités à `MAX_PATH` (260 caractères). Évite de le mettre dans un dossier avec un chemin très long.
- Une prédication sans nom de fichier audio ou sans lien de téléchargement est ignorée.

## Contribuer

Les issues et les pull requests sont les bienvenues, surtout pour la compilation MSVC, la reprise des téléchargements interrompus, et tout ce qui peut rendre le programme moins dépendant d'ImageMagick en dynamique.

## Remerciements

Ce projet repose sur [SQLite](https://sqlite.org), [TagLib](https://taglib.org) et [ImageMagick](https://imagemagick.org). Les prédications et les cantiques appartiennent à leurs auteurs et sont publiés sur philippekacou.org : ce programme ne fait que les récupérer et les ranger.
