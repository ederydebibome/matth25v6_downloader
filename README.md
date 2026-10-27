# matth25v6_downloader (branche `toutes-langues`)

Un petit programme console Windows qui télécharge les prédications audio du Prophète Philippe Kacou, et si tu veux les cantiques, depuis les serveurs de philippekacou.org. Il range le tout proprement : bon nom de fichier, bons tags MP3.

J'ai écrit ça au départ parce que j'en avais marre de remplir les tags à la main et de me demander si mes fichiers étaient encore à jour. Le programme se souvient de ce qu'il a déjà téléchargé, ne retélécharge que ce qui a changé côté serveur, et peut tourner autant de fois que tu veux sans rien casser.

## Cette branche, et ce qui change par rapport à `main`

La branche `main` ne gère que quatre langues (fr, en, es, pt) et fabrique une pochette pour chaque prédication avec ImageMagick. Celle-ci fait deux choses de plus ou de moins :

| | `main` | `toutes-langues` |
|---|---|---|
| Langues | 4, écrites dans le code | toutes celles que le serveur publie (plus de 140), lues dans `common.db` |
| Choix des langues | tapé à la main (`Fr,En`) | liste numérotée affichée au lancement, tu tapes les numéros |
| Langue par défaut | `En,Fr` | aucune, il faut choisir |
| Cantiques | toujours téléchargés | une question, et `N` par défaut |
| Bases des langues | les 5 bases d'office | seulement celles des langues choisies |
| Pochettes | générées avec ImageMagick | supprimées |
| Ressources embarquées | gabarits PNG et police dans l'exe | supprimées |
| `img.dll` et ImageMagick | nécessaires | plus nécessaires |

Le code des pochettes n'est pas perdu, il reste dans `main`.

## Ce que ça fait, concrètement

Tu lances `pred.exe`. Dans l'ordre :

1. **Il télécharge `common.db` tout seul.** C'est la base commune du serveur, et c'est elle qui contient la liste des langues : on ne peut pas te proposer de choix avant de l'avoir. Le programme demande d'abord à l'API la date de dernière mise à jour, la compare à celle qu'il a gardée, et ne retélécharge que si elle a changé.
2. **Il te pose ses questions** (voir plus bas) et affiche la liste des langues actives, numérotées.
3. **Il télécharge les bases des langues que tu as choisies**, une par une, avec la même vérification de version. Si le serveur n'a pas de base pour l'une d'elles, il te le dit tout de suite et l'ignore, avant d'avoir perdu du temps sur des MP3.
4. **Il télécharge les cantiques**, seulement si tu as répondu `O`.
5. **Il parcourt les langues**, et pour chaque prédication active :
   - il regarde si le MP3 existe déjà sur le disque ;
   - il compare le `updated_at` du serveur avec celui qu'il a mémorisé au dernier téléchargement ;
   - s'il manque quelque chose, ou si la version a changé, il télécharge ;
   - il écrit les tags ID3 (titre, artiste, album, genre, numéro de piste, année, langue).
6. **Il affiche un résumé** : combien de prédications ont été téléchargées ou mises à jour, par langue.

Le point sur lequel j'ai passé le plus de temps, c'est de ne jamais casser un fichier valide. Un téléchargement qui échoue ne remplace rien (voir « Les détails qui comptent »), et la version n'est mémorisée qu'après un succès complet. Si ta connexion lâche au milieu, la prochaine exécution reprend là où il faut.

## Les questions posées au lancement

| Question | Défaut | Ce que ça change |
|---|---|---|
| Séparer les prédications par langue ? | `N` | `O` : un sous-dossier par langue. `N` : tout dans `Predications\`. |
| Télécharger aussi les cantiques ? | `N` | `O` : les cantiques actifs de `common.db` sont téléchargés. Entrée les saute. |
| Langues à télécharger | aucun | Les numéros de la liste, séparés par des virgules ou des espaces. `all` pour tout (avec confirmation). |

Pour les deux premières, une entrée vide prend la valeur par défaut. Pour les langues, une entrée vide ne passe pas : il faut choisir, le programme redemande (cinq essais, puis il s'arrête).

### Choisir ses langues

La liste ressemble à ça :

```
Langues disponibles (141) :
    1. ao-cjk    Chokwe (Angola)
    2. ao-kg     Kikongo (Angola)
  ...
   48. cm-bum    Bulu (Cameroun)
```

Tu tapes les numéros, par exemple `2, 15, 37`. Trois choses à savoir :

- **Les numéros sont ceux de la liste affichée.** Ce n'est pas l'`id` de la table `langues` de `common.db`. La liste est triée par code (les langues d'un même pays se suivent), puis numérotée de 1 à N. Si le serveur ajoute ou retire une langue, les numéros bougent : relis toujours la liste avant de taper.
- **Le code passe en premier** dans l'affichage, parce que certains noms (arabe, hindi, chinois...) s'affichent mal dans une console Windows. Le pays entre parenthèses permet de distinguer les langues qui portent le même nom : le Kikongo existe trois fois (`ao-kg`, `cd-kgdc`, `cg-kgcb`), ce sont trois versions différentes.
- **Seules les langues actives sont proposées** (`is_active = 1` dans `common.db`). Une langue présente sur le serveur mais désactivée n'apparaît pas.

`all` télécharge toutes les langues avec toutes leurs prédications. C'est très long et très lourd, donc le programme demande une confirmation, dont le défaut est `N`.

## Les codes de langue et les noms de fichiers

Le serveur identifie chaque langue par un code de la forme `pays-langue` : `fr-fr`, `en-en`, `ci-aba`, `cd-kgdc`. C'est la colonne `initial` de la table `langues`, et c'est ce code, tel quel, qui est utilisé dans les appels à l'API.

Pour nos fichiers et la base d'état, j'utilise une clé, qui vient du code :

| Code API | Clé | Base locale |
|---|---|---|
| `fr-fr` | `fr` | `matth25v6_fr.db` |
| `en-en` | `en` | `matth25v6_en.db` |
| `ci-aba` | `ci-aba` | `matth25v6_ci-aba.db` |
| `cd-kgdc` | `cd-kgdc` | `matth25v6_cd-kgdc.db` |
| `ao-kg` | `ao-kg` | `matth25v6_ao-kg.db` |

La règle est simple : si les deux moitiés du code sont identiques (`fr-fr`, `en-en`, `de-de`...), la clé est raccourcie, sinon c'est le code complet. Sans ça, `ci-aba`, `ci-abu` et `ci-abi` donneraient toutes la même clé `ab` et s'écraseraient entre elles.

Le nom du fichier local est choisi par le programme, pas par le serveur. Les quatre langues historiques gardent leurs noms d'avant, donc si tu viens de `main` tes bases et ta mémoire dans `pred_state.db` sont retrouvées : rien n'est retéléchargé.

## Où vont les fichiers

Tout est relatif au dossier de `pred.exe` :

```
pred.exe
net.dll  metadata.dll  matth25v6_db.dll
pred_state.db                  <- la mémoire du programme
Ressources\                    <- common.db et les bases des langues choisies
Matthieu25v6\
  Music\
    Predications\              <- (ou Predications\fr, \ci-aba... selon ton choix)
    Cantiques\
```

Le dossier `Ressources\` créé à côté de l'exécutable n'a plus rien à voir avec des ressources embarquées : il garde son nom pour que les bases déjà téléchargées chez moi soient retrouvées. Il ne contient que des fichiers `.db`.

## Les fichiers du projet, un par un

Le programme est découpé en un exécutable et trois DLL qu'il charge au démarrage avec `LoadLibrary`. Ce n'est pas la façon la plus simple de faire, je le reconnais : c'est une habitude de mes autres projets, et ça garde chaque bibliothèque lourde (SQLite, TagLib) isolée de l'exécutable. `main.c` récupère ensuite chaque fonction avec `GetProcAddress`, et les types de ces fonctions sont déclarés dans `projetc.h`.

### `main.c` : le chef d'orchestre

C'est le seul fichier qui connaît le déroulement complet. Dans l'ordre, `main()` :

1. passe la console en UTF-8 (sinon les accents s'affichent mal) ;
2. charge `net.dll` en premier, parce que tout le reste a besoin de télécharger ;
3. ouvre `pred_state.db` ;
4. télécharge `common.db`, seul ;
5. charge `metadata.dll` et `matth25v6_db.dll` et récupère leurs fonctions ;
6. lit la liste des langues actives dans `common.db` ;
7. lance la configuration (les questions et le choix des langues) ;
8. télécharge les bases des langues choisies ;
9. télécharge les cantiques si demandé ;
10. boucle sur les langues dont la base est bien là, et appelle `process_sermon()` pour chaque prédication ;
11. affiche le résumé et libère les DLL.

Les fonctions importantes :

- `doit_telecharger()` : la décision « faut-il télécharger ? ». Elle répond oui dans trois cas : le MP3 est absent du disque, aucune version n'est mémorisée pour lui, ou la version du serveur est différente de celle mémorisée. Elle renvoie aussi la raison, affichée dans la console.
- `process_sermon()` : tout le traitement d'une prédication (dossier, nom de fichier, téléchargement, tags, mémorisation de la version).
- `telecharger_cantiques_actifs()` : la même chose pour les cantiques.
- `sanitize_filename()`, `forcer_extension_mp3()`, `year_from_date()` : petits utilitaires pour nettoyer les noms de fichiers, imposer `.mp3` et extraire l'année d'une date.

### `utils.cpp` / `utils.h` : la boîte à outils

- **Chemins et fichiers en UTF-8** : `utf8_vers_wide()`, `fichier_existe()`, `creer_dossier_recursif()`, `supprimer_fichier()`, `obtenir_dossier_executable()`. Ce sont des enveloppes autour des API Windows « W » (UTF-16), pour que les accents ne soient jamais abîmés.
- **Codes de langue** : `cle_langue()` (le raccourci `fr-fr` -> `fr` quand c'est possible), `code_langue_valide()` (un code ne contient que des lettres, des chiffres, `-` et `_`, puisqu'il finit dans une URL et un nom de fichier), `nom_fichier_base()`.
- **Dialogue avec l'API** : `telecharger_base_donnees()` demande à l'API l'URL d'une base, la lit dans la réponse JSON, puis télécharge la base. `recuperer_update_langue()` demande la date de dernière mise à jour.
- **Synchronisation** : `synchroniser_base()` vérifie une base auprès de l'API et ne la retélécharge que si elle a changé. Si l'API est injoignable mais que la base est déjà sur le disque, elle continue avec la base locale.
- **Configuration** : `configurer_application()` pose les questions, filtre et trie les langues, affiche la liste numérotée et lit le choix.

### `state_db.cpp` / `state_db.h` : la mémoire du programme

Une petite base SQLite locale, `pred_state.db`, créée au premier lancement à côté de l'exécutable. Elle est compilée directement dans `pred.exe`. Elle sert à une seule chose : se souvenir de ce qui a déjà été fait.

| Table | Contenu | Utilisée ? |
|---|---|---|
| `langues_updates` | La date de mise à jour de chaque base (`common`, `fr`, `ci-aba`...), indexée par la clé de la langue, au moment où elle a été téléchargée. | Oui |
| `audio_state` | Pour chaque fichier audio présent sur le disque (`scope` = clé de la langue ou `cantiques`, `item_id` = numéro de prédication ou id du cantique), la version du serveur correspondante. | Oui, c'est la table principale |
| `predications_updates` | Un suivi par prédication et par langue. | Prévue dans le schéma, pas utilisée aujourd'hui : le vrai suivi passe par `audio_state` |

La règle importante : la version d'un fichier n'est écrite dans `audio_state` qu'**après** un téléchargement réussi. Si quelque chose plante avant, le fichier sera retenté à la prochaine exécution.

### `matth25v6_db.cpp` / `matth25v6_db.h` → `matth25v6_db.dll` : le lecteur des bases officielles

Le plus gros fichier du projet, et il fait un travail simple : **il lit les bases SQLite publiées par le serveur** (`common.db` et les `matth25v6_XX.db`) et rend leur contenu sous forme de structures C, sans que `main.c` écrive une seule requête SQL.

Ces bases sont celles que l'application officielle utilise elle-même, donc elles contiennent bien plus que ce dont ce téléchargeur a besoin. J'ai écrit la DLL pour couvrir l'ensemble, parce que c'est un morceau réutilisable. Les bases sont ouvertes en **lecture seule**, avec une seule exception, `LastUpdate_Set`.

**Dans `common.db`** (commun à toutes les langues) :

| Structure | Contenu | Fonctions |
|---|---|---|
| `M25Langue` | Les langues : libellé, code (`initial`), pays, langue africaine ou non, affichage sur la page des prédications... | `Langue_GetAll`, `Langue_GetActive` |
| `M25Album` | Les albums de cantiques | `Album_GetAll`, `Album_GetById` |
| `M25Sing` | Les cantiques, avec jointure sur l'album et le chanteur | `Sing_GetAll`, `Sing_GetByAlbum`, `Sing_GetBySinger`, `Sing_GetById` |
| `M25Singer`, `M25Brother` | Les chanteurs et les frères | `Singer_GetAll`, `Brother_GetAll`, `Brother_GetById` |
| `M25Assembly` | Les assemblées, avec ville et pays | `Assembly_GetAll`, `Assembly_GetByCountry` |
| `M25LastUpdate` | Les dates de dernière synchronisation par langue | `LastUpdate_GetAll`, `LastUpdate_GetByLangue`, `LastUpdate_Set` |
| `M25Concordance` | Les concordances entre prédications et versets | `Concordance_GetBySermon`, `Concordance_GetByVerse` |
| `M25ImageSermonMeta` | Les images de prédication en BLOB | `ImageSermon_GetMeta`, `ImageSermon_GetFile` (`M25_Free` libère le buffer) |

**Dans chaque base de langue** :

| Structure | Contenu | Fonctions |
|---|---|---|
| `M25Sermon` | Les prédications : numéro, titre, sous-titre, lien audio, nom du fichier, durée, date de publication, PDF, EPUB, vidéo, `updated_at` | `Sermon_GetAll`, `Sermon_GetByNumber`, `Sermon_GetCount`, `Sermon_Search` |
| `M25Verse` | Les versets de chaque prédication | `Verse_GetByRef`, `Verse_GetBySermon`, `Verse_SearchFirst`, `Verse_SearchAll`, `Verse_GetCount` |
| `M25Photo`, `M25Video`, `M25Event` | Photos, vidéos et événements | `Photo_GetAll`, `Video_GetAll`, `Event_GetAll` |
| `M25Information`, `M25Travel`, `M25Testimony` | Actualités, voyages et témoignages | `Information_GetAll`, `Travel_GetAll`, `Testimony_GetAll` |
| `M25Book`, `M25Biography` | Les livres et la biographie | `Book_GetAll`, `Biography_Get` |

**Ce que le téléchargeur utilise vraiment :** quatre fonctions, `Langue_GetActive` (la liste des langues), `Sermon_GetCount` et `Sermon_GetAll` (les prédications d'une langue) et `Sing_GetAll` (les cantiques). Les autres sont là, mais le programme ne les appelle pas.

Les fonctions renvoient un code (`M25_OK`, `M25_ERROR`, `M25_NOT_FOUND`, `M25_INVALID_ARG`) et remplissent un tableau fourni par l'appelant, avec sa capacité, au lieu d'allouer elles-mêmes : l'allocation et la libération ne se retrouvent pas de part et d'autre d'une frontière de DLL.

### `net_source.cpp` / `net_header.h` → `net.dll` : le téléchargeur

Une seule fonction exportée : `DownloadFile(url, cheminSortie)`. Elle utilise WinINet (la pile HTTP de Windows, donc elle respecte les paramètres de proxy du système) et sert à tout : prédications, cantiques, bases de données, et même les petites réponses JSON de l'API.

### `metadata_source.cpp` / `metadata_header.h` → `metadata.dll` : les tags

Enveloppe autour de TagLib. `SetTags(chemin, paires, nombre)` applique une liste de paires clé/valeur à un fichier audio. Pour un MP3, elle écrit de l'ID3v2.4 en UTF-8 et supprime l'ID3v1. La DLL exporte aussi `SetCoverImage`, qui incruste une pochette : elle n'est plus appelée dans cette branche, mais je l'ai laissée, elle ne gêne pas.

### `projetc.h` : le contrat entre l'exécutable et les DLL

Les définitions de types de pointeurs de fonction (`download_file_t`, `set_tags_t`, `sermon_get_all_t`, `langue_get_active_t`...) que `main.c` utilise avec `GetProcAddress`. Si tu changes la signature d'une fonction exportée, c'est ici qu'il faut aussi la changer, sinon ça compile mais ça plante à l'exécution.

### `build.sh` et `.gitignore`

`build.sh` compile les trois DLL puis l'exécutable pour MSYS2 (détaillé plus bas). `.gitignore` exclut `build/`, les exécutables, les fichiers objets et le fichier `.env` : seules les sources sont dans le dépôt.

### Ce qui n'existe plus dans cette branche

`img_source.cpp`, `img_header.h`, `resources.rc`, `resource.h` et le dossier `Ressources/` (gabarits PNG, police, `check.py`) sont supprimés. Ils sont toujours dans `main`.

## Compiler avec MSYS2 (la méthode qui marche)

### 1. Installer MSYS2

Télécharge l'installeur sur [msys2.org](https://www.msys2.org) et installe-le (par défaut dans `C:\msys64`). Ouvre ensuite le terminal **MSYS2 MINGW64** (pas « MSYS », pas « UCRT64 », pas « CLANG64 »).

### 2. Installer les dépendances

```bash
pacman -Syu
```

Il peut te demander de fermer et rouvrir le terminal. Relance jusqu'à ce qu'il n'y ait plus rien à mettre à jour. Puis :

```bash
pacman -S --needed git \
    mingw-w64-x86_64-gcc \
    mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-sqlite3 \
    mingw-w64-x86_64-taglib
```

ImageMagick n'est plus nécessaire dans cette branche.

### 3. Récupérer le code et compiler

```bash
git clone --branch toutes-langues https://github.com/ederydebibome/matth25v6_downloader.git
cd matth25v6_downloader
bash build.sh
```

Le script fait quatre étapes :

1. `net.dll`, avec `-lwininet`.
2. `matth25v6_db.dll`, lié à SQLite.
3. `metadata.dll`, lié à TagLib.
4. `pred.exe`, à partir de `main.c`, `utils.cpp` et `state_db.cpp`.

Tout arrive dans `build/`. À la fin, le script affiche pour chaque fichier la liste des DLL dont il dépend. Si tu as déjà compilé une ancienne version, fais d'abord `rm -rf build`, pour ne pas garder un vieux `img.dll`.

### 4. Lancer

```bash
cd build
./pred.exe
```

Ou double-clique sur `pred.exe`. Il faut que les trois DLL soient dans le même dossier que l'exécutable, sinon il s'arrête au chargement.

### Ce que fait `build.sh` de particulier

- **Le runtime de GCC est lié en statique** (`-static-libgcc -static-libstdc++`), pour ne pas avoir à distribuer `libgcc_s_seh-1.dll`, `libstdc++-6.dll` et `libwinpthread-1.dll` à côté.
- **SQLite et TagLib sont liés en statique** dans leur DLL respective.
- ImageMagick était la seule dépendance dynamique de `main`. Ici il n'y en a plus : en principe, `pred.exe` et ses trois DLL suffisent pour distribuer le programme. Vérifie avec la liste affichée à la fin du script, elle ne doit montrer que des DLL de Windows.
- `set -e` : au premier échec, tout s'arrête. Lis la dernière ligne avant l'arrêt, c'est presque toujours là qu'est la cause.

### Problèmes classiques

| Symptôme | Cause probable |
|---|---|
| `pkg-config: command not found` | Tu n'es pas dans le terminal MINGW64, ou `mingw-w64-x86_64-pkgconf` n'est pas installé. |
| `Package taglib was not found` | `pacman -S mingw-w64-x86_64-taglib`. |
| `pred.exe` s'ouvre et se ferme, ou affiche « Erreur net.dll » | Les DLL ne sont pas à côté de l'exe. |
| Les questions ne sont pas celles du README (codes à taper, `en-en,fr-fr` par défaut) | Tu as lancé un ancien `pred.exe` : `rm -rf build` puis recompile. |

## Compiler avec MSVC (Visual Studio) : non testé

Je n'ai pas compilé ce projet avec MSVC, et `build.sh` ne couvre pas ce cas. Voici comment je m'y prendrais, à vérifier. Si tu essaies, une issue ou une pull request avec ce qu'il faut corriger sera très bienvenue.

**Prérequis :** Visual Studio avec la charge de travail « Développement Desktop en C++ », et [vcpkg](https://github.com/microsoft/vcpkg) pour les bibliothèques.

```bat
vcpkg install sqlite3:x64-windows taglib:x64-windows
vcpkg integrate install
```

Ouvre ensuite **x64 Native Tools Command Prompt for VS**, et adapte `VCPKG` à ton installation :

```bat
set VCPKG=C:\vcpkg\installed\x64-windows
mkdir build

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD net_source.cpp /Fe:build\net.dll wininet.lib

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD matth25v6_db.cpp /I%VCPKG%\include ^
   /Fe:build\matth25v6_db.dll /link /LIBPATH:%VCPKG%\lib sqlite3.lib

cl /nologo /std:c++17 /O2 /EHsc /utf-8 /LD metadata_source.cpp /I%VCPKG%\include ^
   /Fe:build\metadata.dll /link /LIBPATH:%VCPKG%\lib tag.lib

cl /nologo /O2 /utf-8 /Fe:build\pred.exe main.c utils.cpp state_db.cpp ^
   /I%VCPKG%\include /link /LIBPATH:%VCPKG%\lib sqlite3.lib user32.lib
```

Ce que je sais et ce que je suppose :

- **Le nom du `.lib` de TagLib dépend de la version installée par vcpkg.** Va voir dans `%VCPKG%\lib` ce qui s'y trouve vraiment et corrige.
- Le code est en UTF-8 avec des accents : l'option `/utf-8` est indispensable avec MSVC, sinon les chaînes sortent déformées.
- Mélanger du C (`main.c`) et du C++ (`utils.cpp`, `state_db.cpp`) dans une seule ligne `cl` est possible, mais si tu as des soucis de liaison, compile chaque fichier en `.obj` séparément.
- Les DLL MSVC ont besoin des DLL de vcpkg (sqlite3, tag) à côté pour tourner : copie-les dans `build\`.

## Les détails qui comptent (si tu veux lire le code)

**Tout est en UTF-8.** Chemins, titres, URL. Sous Windows, les API « A » utilisent la page de code ANSI et abîment les accents des titres français, donc partout où je touche au système de fichiers, je convertis en UTF-16 et j'appelle la version « W » de l'API. Si tu ajoutes du code, ne mets jamais un `char*` dans un `CreateFileA` ou un `fopen`.

**Les téléchargements sont atomiques.** `net.dll` écrit dans `fichier.mp3.part`, vérifie le code HTTP (un 404 n'est jamais enregistré comme un MP3), compare la taille reçue à `Content-Length`, et seulement ensuite remplace le fichier final avec `MoveFileEx`. Si ça rate, l'ancienne version reste intacte.

**Le nom des fichiers est nettoyé** : les caractères interdits sous Windows (`< > : " / \ | ? *`) et les caractères de contrôle deviennent `_`. Pour les cantiques, l'extension est forcée en `.mp3`.

**Les tags sont en ID3v2.4, texte en UTF-8.** L'ID3v1, limité au Latin-1, est supprimé et n'est pas recréé. Les tags d'une prédication : titre, artiste, album, genre « Sermon », numéro de piste, année de publication et langue. Pour l'anglais (`en-en`) l'artiste est « Matthew 25v6 » et l'album « The Message of Matthew 25v6 in Audio ». Pour toutes les autres langues ce sont les noms français (« Matthieu 25v6 », « Le Message de Matthieu 25v6 en audio »). Le tag de langue contient le code complet (`fr-fr`, `ci-aba`...).

**L'API.** Le programme appelle `https://api.philippekacou.org/api/...` pour deux choses : connaître la date de mise à jour d'une base, et obtenir l'URL de téléchargement d'une base (`/api/<code>/download-url`). Les réponses JSON sont lues à la main (recherche de `"download_url":"` et de `"updated_at":"`) plutôt qu'avec une vraie bibliothèque JSON. C'est suffisant pour le format actuel, mais ça casserait si le serveur changeait la forme de ses réponses.

## Limites connues

- Windows uniquement : WinINet, `LoadLibrary`, les API « W »...
- Une langue active dans `common.db` n'a pas forcément de base publiée sur le serveur. Dans ce cas elle est ignorée avec un message, et le programme continue avec les autres.
- Je n'ai testé les appels à l'API qu'avec `fr-fr`, `en-en`, `es-es` et `pt-pt` au départ. Pour les autres codes, je me suis appuyé sur le fait que ce sont les codes que le serveur publie lui-même dans `common.db`.
- Deux endroits écrivent encore `en-en` en dur : le choix des noms d'artiste et d'album dans les tags (anglais pour `en-en`, français sinon), et l'URL de vérification de version, qui passe par `/api/en-en/langue-releases/...` avec le code de la langue en paramètre.
- Les numéros du choix de langue sont des positions dans la liste, qui change si le serveur ajoute ou retire une langue.
- Pas de reprise partielle d'un MP3 interrompu : il est retéléchargé en entier.
- Les chemins sont limités à `MAX_PATH` (260 caractères). Évite de le mettre dans un dossier avec un chemin très long.
- Plus de pochettes : les MP3 téléchargés avec cette branche n'en ont pas. Ceux téléchargés avec `main` gardent la leur tant qu'ils ne sont pas retéléchargés.

## Contribuer

Les issues et les pull requests sont les bienvenues, surtout pour la compilation MSVC, la reprise des téléchargements interrompus, et le support des noms d'artiste et d'album dans d'autres langues que le français et l'anglais.

## Remerciements

Ce projet repose sur [SQLite](https://sqlite.org) et [TagLib](https://taglib.org). Les prédications et les cantiques appartiennent à leurs auteurs et sont publiés sur philippekacou.org : ce programme ne fait que les récupérer et les ranger.
