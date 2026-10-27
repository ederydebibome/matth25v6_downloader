// main.c - Programme principal

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "projetc.h"
#include "utils.h"
#include "state_db.h"
#include <locale.h>

download_file_t g_downloadFile = NULL;
set_tags_t g_setTags = NULL;

static void copy_text(char* dst, size_t size, const char* src) {
    copier_texte_utf8(dst, size, src);
}

static void sanitize_filename(char* text) {
    if (!text) return;
    const char* bad = "<>:\"/\\|?*";
    for (char* p = text; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 32 || strchr(bad, *p)) *p = '_';
    }
}

static void year_from_date(const char* date, char* out, size_t size) {
    copy_text(out, size, "2002");
    if (!date) return;
    size_t len = strlen(date);
    if (len >= 4) {
        copy_text(out, size, date + len - 4);
    }
}

static const char* sermon_album_for_lang(const char* langue) {
    if (langue && strcmp(langue, "en-en") == 0) {
        return "The Message of Matthew 25v6 in Audio";
    }
    return "Le Message de Matthieu 25v6 en audio";
}

static const char* sermon_artist_for_lang(const char* langue) {
    if (langue && strcmp(langue, "en-en") == 0) {
        return "Matthew 25v6";
    }
    return "Matthieu 25v6";
}

static void set_audio_tags(const char* audioPath, const TagPair* tags, int count) {
    if (!g_setTags) return;
    if (g_setTags(audioPath, tags, count) == 0) {
        printf("✓ Métadonnées ajoutées\n");
    } else {
        fprintf(stderr, "✗ Erreur métadonnées\n");
    }
}

// Faut-il (re)télécharger ?  Oui si le fichier est absent du disque, OU si la
// version du serveur (updated_at) diffère de celle du fichier présent
// (mémorisée dans pred_state.db), OU si aucune version n'est mémorisée
// (on ne peut alors pas garantir que le fichier est à jour).
// Retourne 0 si le fichier est à jour, 1 sinon ; `raison` décrit pourquoi.
static int doit_telecharger(const char* scope, int item_id, const char* audioPath,
                            const char* updated_at_serveur, char* raison, size_t raison_size) {
    char version_locale[64] = {0};
    int connu = StateDB_GetAudioUpdatedAt(scope, item_id, version_locale, sizeof(version_locale));
    int present = fichier_existe(audioPath);

    if (!present) {
        snprintf(raison, raison_size, "absent de l'ordinateur");
        return 1;
    }
    if (!connu) {
        snprintf(raison, raison_size, "version locale inconnue");
        return 1;
    }
    if (strcmp(version_locale, updated_at_serveur) != 0) {
        snprintf(raison, raison_size, "nouvelle version sur le serveur (%s -> %s)",
                 version_locale, updated_at_serveur);
        return 1;
    }
    raison[0] = '\0';
    return 0;
}

// Traite une prédication : téléchargement si besoin, puis tags.
// Retourne 1 si le fichier est bon (déjà à jour ou téléchargé), 0 si ça a échoué.
// *mis_a_jour passe à 1 quand on a vraiment téléchargé quelque chose.
static int process_sermon(const M25Sermon* sermon, const Config* config,
                          const char* code, const char* cle, int* mis_a_jour) {
    if (mis_a_jour) *mis_a_jour = 0;
    if (!sermon || !config || !code || !cle) return 0;
    if (!sermon->audio_name[0] || !sermon->cloud_audio[0]) return 0;

    char numero[32];
    snprintf(numero, sizeof(numero), "%d", sermon->number);

    char annee[8];
    year_from_date(sermon->publication_date, annee, sizeof(annee));

    printf("\n=== Kacou %s - %s ===\n", numero, code);

    char dossierAudio[MAX_PATH];
    if (config->separer_par_langue) {
        snprintf(dossierAudio, sizeof(dossierAudio), "%s\\%s", config->path_predications, cle);
    } else {
        snprintf(dossierAudio, sizeof(dossierAudio), "%s", config->path_predications);
    }
    creer_dossier_recursif(dossierAudio);

    char nomAudio[260];
    snprintf(nomAudio, sizeof(nomAudio), "%s", sermon->audio_name);
    sanitize_filename(nomAudio);

    char audioPath[MAX_PATH];
    snprintf(audioPath, sizeof(audioPath), "%s\\%s", dossierAudio, nomAudio);

    char raison[160];
    if (!doit_telecharger(cle, sermon->number, audioPath, sermon->updated_at, raison, sizeof(raison))) {
        printf("Audio : à jour, réutilisation\n");
        return 1;
    }
    printf("Audio : %s, téléchargement...\n", raison);

    if (!g_downloadFile || !g_downloadFile(sermon->cloud_audio, audioPath)) {
        fprintf(stderr, "✗ Échec téléchargement audio\n");
        return 0;
    }
    printf("✓ Audio téléchargé\n");

    TagPair tags[] = {
        {"TITLE", sermon->title},
        {"ARTIST", sermon_artist_for_lang(code)},
        {"ALBUM", sermon_album_for_lang(code)},
        {"GENRE", "Sermon"},
        {"TRACKNUMBER", numero},
        {"DATE", annee},
        {"LANGUAGE", code}
    };
    set_audio_tags(audioPath, tags, (int)(sizeof(tags) / sizeof(tags[0])));

    // la version n'est notée qu'ici, une fois que tout s'est bien passé
    if (!StateDB_SetAudioUpdatedAt(cle, sermon->number, sermon->updated_at)) {
        fprintf(stderr, "⚠ Impossible d'enregistrer la version dans pred_state.db\n");
    }

    if (mis_a_jour) *mis_a_jour = 1;
    return 1;
}

// Force l'extension .mp3 : retire l'extension existante (.m3, .mm3, .mp3...)
// si elle fait 1 à 4 caractères alphanumériques, puis ajoute ".mp3".
static void forcer_extension_mp3(char* name, size_t size) {
    if (!name || size < 6) return;

    char* dot = strrchr(name, '.');
    if (dot && dot != name) {
        size_t extlen = strlen(dot + 1);
        int alnum = (extlen > 0 && extlen <= 4);
        for (size_t i = 1; alnum && i <= extlen; ++i) {
            if (!isalnum((unsigned char)dot[i])) alnum = 0;
        }
        if (alnum) *dot = '\0';
    }

    size_t len = strlen(name);
    if (len > size - 5) len = size - 5;
    memcpy(name + len, ".mp3", 5);
}

static void telecharger_cantiques_actifs(const char* commonDbPath, const Config* config, sing_get_all_t Sing_GetAll) {
    if (!commonDbPath || !config || !Sing_GetAll) return;
    if (!fichier_existe(commonDbPath)) {
        fprintf(stderr, "⚠ common.db introuvable : %s\n", commonDbPath);
        return;
    }

    const int capacity = 10000;
    M25Sing* sings = (M25Sing*)calloc((size_t)capacity, sizeof(M25Sing));
    if (!sings) {
        fprintf(stderr, "✗ Mémoire insuffisante pour les cantiques\n");
        return;
    }

    int count = 0;
    if (Sing_GetAll(commonDbPath, sings, capacity, &count) != M25_OK) {
        fprintf(stderr, "✗ Erreur lecture des cantiques actifs\n");
        free(sings);
        return;
    }

    printf("\n========== CANTIQUES ACTIFS (%d) ==========\n", count);

    creer_dossier_recursif(config->path_cantiques);

    int telecharges = 0;
    for (int i = 0; i < count; ++i) {
        M25Sing* sing = &sings[i];
        if (!sing->audio_url[0]) continue;

        char fileName[512];
        if (sing->file_name[0]) {
            copy_text(fileName, sizeof(fileName), sing->file_name);
        } else {
            snprintf(fileName, sizeof(fileName), "%03d_%s.mp3", sing->order > 0 ? sing->order : sing->id, sing->title);
        }
        sanitize_filename(fileName);
        forcer_extension_mp3(fileName, sizeof(fileName));

        char audioPath[MAX_PATH];
        snprintf(audioPath, sizeof(audioPath), "%s\\%s", config->path_cantiques, fileName);

        printf("\n=== Cantique %d - %s ===\n", sing->id, sing->title);

        char raison[160];
        if (!doit_telecharger("cantiques", sing->id, audioPath, sing->updated_at,
                              raison, sizeof(raison))) {
            printf("Audio : à jour, réutilisation\n");
            continue;
        }

        printf("Audio : %s, téléchargement...\n", raison);
        if (!g_downloadFile || !g_downloadFile(sing->audio_url, audioPath)) {
            fprintf(stderr, "✗ Échec téléchargement cantique\n");
            continue;
        }
        printf("✓ Cantique téléchargé\n");

        char track[32];
        snprintf(track, sizeof(track), "%d", sing->order > 0 ? sing->order : sing->id);

        TagPair tags[] = {
            {"TITLE", sing->title},
            {"ARTIST", sing->singer_name[0] ? sing->singer_name : "Matthieu 25v6"},
            {"ALBUM", sing->album_title[0] ? sing->album_title : "Cantiques"},
            {"GENRE", "Cantique"},
            {"TRACKNUMBER", track}
        };
        set_audio_tags(audioPath, tags, (int)(sizeof(tags) / sizeof(tags[0])));

        if (!StateDB_SetAudioUpdatedAt("cantiques", sing->id, sing->updated_at)) {
            fprintf(stderr, "⚠ Impossible d'enregistrer la version dans pred_state.db\n");
        }
        telecharges++;
    }

    printf("\n✓ Cantiques : %d téléchargé(s) ou mis à jour\n", telecharges);

    if (count == capacity) {
        fprintf(stderr, "⚠ Limite de %d cantiques atteinte; augmentez capacity si nécessaire.\n", capacity);
    }
    free(sings);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    setlocale(LC_ALL, NULL);

    lib net = {"net.dll", NULL};
    lib meta = {"metadata.dll", NULL};
    lib m25 = {"matth25v6_db.dll", NULL};

    sermon_get_count_t Sermon_GetCount = NULL;
    sermon_get_all_t Sermon_GetAll = NULL;
    sing_get_all_t Sing_GetAll = NULL;
    langue_get_active_t Langue_GetActive = NULL;

    Config config = {0};
    M25Langue* disponibles = NULL;
    int nb_disponibles = 0;
    int compteurs_maj[LANGUES_MAX] = {0};

    char dossier_exe[MAX_PATH];
    char dossier_ressources[MAX_PATH];
    char commonDbPath[MAX_PATH];
    char state_db_path[MAX_PATH];

    printf("=== PKP Downloader ===\n");
    obtenir_dossier_executable(dossier_exe, sizeof(dossier_exe));
    printf("Dossier : %s\n", dossier_exe);

    // net.dll d'abord : sans elle on ne peut même pas récupérer les bases
    net.value = LoadLibraryA(net.name);
    if (!net.value) {
        fprintf(stderr, "✗ Erreur net.dll (requis pour téléchargement)\n");
        printf("Appuyez sur Entrée pour quitter...");
        getchar();
        return -1;
    }
    g_downloadFile = (download_file_t)GetProcAddress(net.value, "DownloadFile");
    if (!g_downloadFile) {
        fprintf(stderr, "✗ DownloadFile introuvable dans net.dll\n");
        goto cleanup;
    }

    snprintf(dossier_ressources, sizeof(dossier_ressources), "%s\\Ressources", dossier_exe);
    creer_dossier_recursif(dossier_ressources);

    snprintf(state_db_path, sizeof(state_db_path), "%s\\pred_state.db", dossier_exe);
    if (!StateDB_Open(state_db_path)) {
        fprintf(stderr, "✗ Impossible d'ouvrir pred_state.db\n");
        goto cleanup;
    }

    // common.db en premier et tout seul : c'est lui qui contient la liste des langues,
    // on ne peut pas proposer de choix avant de l'avoir. Les bases des langues, elles,
    // ne sont téléchargées que pour celles qu'on choisit.
    printf("\n--- Vérification de common.db ---\n");
    snprintf(commonDbPath, sizeof(commonDbPath), "%s\\common.db", dossier_ressources);
    synchroniser_base("common", dossier_ressources, "common");
    if (!fichier_existe(commonDbPath)) {
        fprintf(stderr, "✗ common.db est absente et impossible à télécharger, on s'arrête là\n");
        goto cleanup;
    }

    meta.value = LoadLibraryA(meta.name);
    m25.value = LoadLibraryA(m25.name);
    if (!meta.value) {
        fprintf(stderr, "✗ Erreur metadata.dll\n");
        goto cleanup;
    }
    if (!m25.value) {
        fprintf(stderr, "✗ Erreur matth25v6_db.dll\n");
        goto cleanup;
    }

    g_setTags = (set_tags_t)GetProcAddress(meta.value, "SetTags");
    Sermon_GetCount = (sermon_get_count_t)GetProcAddress(m25.value, "Sermon_GetCount");
    Sermon_GetAll = (sermon_get_all_t)GetProcAddress(m25.value, "Sermon_GetAll");
    Sing_GetAll = (sing_get_all_t)GetProcAddress(m25.value, "Sing_GetAll");
    Langue_GetActive = (langue_get_active_t)GetProcAddress(m25.value, "Langue_GetActive");
    if (!g_setTags || !Sermon_GetCount || !Sermon_GetAll || !Sing_GetAll || !Langue_GetActive) {
        fprintf(stderr, "✗ Il manque des fonctions dans les DLL\n");
        goto cleanup;
    }

    disponibles = (M25Langue*)calloc(LANGUES_MAX, sizeof(M25Langue));
    if (!disponibles) {
        fprintf(stderr, "✗ Mémoire insuffisante\n");
        goto cleanup;
    }
    if (Langue_GetActive(commonDbPath, disponibles, LANGUES_MAX, &nb_disponibles) != M25_OK || nb_disponibles <= 0) {
        fprintf(stderr, "✗ Impossible de lire la liste des langues dans common.db\n");
        goto cleanup;
    }
    if (nb_disponibles >= LANGUES_MAX) {
        fprintf(stderr, "⚠ Liste des langues tronquée à %d\n", LANGUES_MAX);
    }

    if (!configurer_application(&config, dossier_exe, disponibles, nb_disponibles)) {
        goto cleanup;
    }

    // les cantiques ne dépendent pas de la langue, ils sont dans common.db
    telecharger_cantiques_actifs(commonDbPath, &config, Sing_GetAll);

    for (int i = 0; i < config.nb_langues; i++) {
        const char* code = config.langues[i];
        char cle[LANGUE_CODE_MAX];
        cle_langue(code, cle, sizeof(cle));

        printf("\n========== %s ==========\n", code);

        if (!synchroniser_base(code, dossier_ressources, code)) {
            fprintf(stderr, "✗ Pas de base pour %s, langue ignorée\n", code);
            continue;
        }

        char nom_base[64];
        nom_fichier_base(code, nom_base, sizeof(nom_base));
        char dbPath[MAX_PATH];
        snprintf(dbPath, sizeof(dbPath), "%s\\%s", dossier_ressources, nom_base);

        int sermonCount = 0;
        if (Sermon_GetCount(dbPath, &sermonCount) != M25_OK || sermonCount <= 0) {
            fprintf(stderr, "✗ Aucune prédication active lue pour %s\n", code);
            continue;
        }
        printf("%d prédications actives\n", sermonCount);

        M25Sermon* sermons = (M25Sermon*)calloc((size_t)sermonCount, sizeof(M25Sermon));
        if (!sermons) {
            fprintf(stderr, "✗ Mémoire insuffisante\n");
            continue;
        }

        int loaded = 0;
        if (Sermon_GetAll(dbPath, sermons, sermonCount, &loaded) != M25_OK || loaded <= 0) {
            fprintf(stderr, "✗ Impossible de lire les prédications de %s\n", code);
            free(sermons);
            continue;
        }

        for (int s = 0; s < loaded; ++s) {
            int mis_a_jour = 0;
            process_sermon(&sermons[s], &config, code, cle, &mis_a_jour);
            if (mis_a_jour) compteurs_maj[i]++;
        }

        free(sermons);
        printf("\n✓ Terminé pour %s\n", code);
    }

    printf("\n=== Résumé ===\n");
    for (int i = 0; i < config.nb_langues; i++) {
        printf("%s : %d prédication(s) téléchargée(s) ou mise(s) à jour\n",
               config.langues[i], compteurs_maj[i]);
    }

cleanup:
    free(disponibles);
    StateDB_Close();
    if (m25.value) FreeLibrary(m25.value);
    if (meta.value) FreeLibrary(meta.value);
    if (net.value) FreeLibrary(net.value);

    printf("\n=== Terminé ===\n");
    printf("Appuyez sur Entrée pour quitter...");
    getchar();
    return 0;
}
