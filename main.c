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

create_image_t g_createImage = NULL;
download_file_t g_downloadFile = NULL;
set_tags_t g_setTags = NULL;
set_cover_image_t g_setCoverImage = NULL;

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
    if (langue && strcmp(langue, "En") == 0) {
        return "The Message of Matthew 25v6 in Audio";
    }
    return "Le Message de Matthieu 25v6 en audio";
}

static const char* sermon_artist_for_lang(const char* langue) {
    if (langue && strcmp(langue, "En") == 0) {
        return "Matthew 25v6";
    }
    return "Matthieu 25v6";
}

static void set_audio_tags(const char* audioPath, const TagPair* tags, int count, const char* coverPath) {
    if (g_setTags) {
        if (g_setTags(audioPath, tags, count) == 0) {
            printf("✓ Métadonnées ajoutées\n");
        } else {
            fprintf(stderr, "✗ Erreur métadonnées\n");
        }
    }

    if (coverPath && coverPath[0] && fichier_existe(coverPath) && g_setCoverImage) {
        if (g_setCoverImage(audioPath, coverPath) == 0) {
            printf("✓ Pochette ajoutée\n");
        } else {
            fprintf(stderr, "✗ Erreur pochette\n");
        }
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

// Retourne 1 si la prédication est valide (à jour ou téléchargée), 0 en cas d'échec.
// *mis_a_jour vaut 1 si un téléchargement a eu lieu.
static int process_sermon(const M25Sermon* sermon, const Config* config, const Font* font,
                          const char* dossier_exe, const char* langue, const char* langue_lower,
                          int* mis_a_jour) {
    if (mis_a_jour) *mis_a_jour = 0;
    if (!sermon || !config || !font || !dossier_exe || !langue || !langue_lower) return 0;
    if (!sermon->audio_name[0] || !sermon->cloud_audio[0]) return 0;

    char numero[32];
    snprintf(numero, sizeof(numero), "%d", sermon->number);

    char annee[8];
    year_from_date(sermon->publication_date, annee, sizeof(annee));

    printf("\n=== Kacou %s - %s ===\n", numero, langue);

    char dossierAudio[MAX_PATH];
    if (config->separer_par_langue) {
        snprintf(dossierAudio, sizeof(dossierAudio), "%s\\%s", config->path_predications, langue);
    } else {
        snprintf(dossierAudio, sizeof(dossierAudio), "%s", config->path_predications);
    }
    creer_dossier_recursif(dossierAudio);

    char nomAudio[256];
    copy_text(nomAudio, sizeof(nomAudio), sermon->audio_name);
    sanitize_filename(nomAudio);

    char audioPath[MAX_PATH];
    snprintf(audioPath, sizeof(audioPath), "%s\\%s", dossierAudio, nomAudio);

    char raison[160];
    if (!doit_telecharger(langue_lower, sermon->number, audioPath, sermon->updated_at,
                          raison, sizeof(raison))) {
        printf("Audio : à jour, réutilisation\n");
        return 1;
    }

    printf("Audio : %s, téléchargement...\n", raison);
    if (!g_downloadFile || !g_downloadFile(sermon->cloud_audio, audioPath)) {
        // L'ancienne version éventuelle reste intacte (remplacement atomique dans net.dll)
        // et la version n'est PAS mémorisée : on réessaiera à la prochaine exécution.
        fprintf(stderr, "✗ Échec téléchargement audio\n");
        return 0;
    }
    printf("✓ Audio téléchargé\n");

    // Image de couverture : toujours régénérée avec le nouveau fichier (le titre a pu changer)
    // Images : dossier local "img" à côté de l'exécutable (le nom du fichier contient déjà la langue)
    const char* dossierImages = config->path_images;
    creer_dossier_recursif(dossierImages);

    char templatePath[MAX_PATH];
    snprintf(templatePath, sizeof(templatePath), "%s\\Ressources\\img_%s.png",
             dossier_exe, (strcmp(langue, "Fr") == 0 || strcmp(langue, "Es") == 0) ? "FR" : "EN");

    char imagePath[MAX_PATH];
    snprintf(imagePath, sizeof(imagePath), "%s\\Kacou_%s_%s.png", dossierImages, numero, langue);

    int image_creee_maintenant = 0;
    if (g_createImage) {
        if (fichier_existe(imagePath)) supprimer_fichier(imagePath);
        printf("Image : Création en cours...\n");
        g_createImage("Kacou", numero, sermon->title, font->path, 35.0, config->couleur_cover,
                      templatePath, imagePath, 20.0, 460.0);
        Sleep(100);
        if (fichier_existe(imagePath)) {
            printf("✓ Image créée\n");
            image_creee_maintenant = 1;
        } else {
            fprintf(stderr, "✗ Échec création image\n");
        }
    }

    TagPair tags[] = {
        {"TITLE", sermon->title},
        {"ARTIST", sermon_artist_for_lang(langue)},
        {"ALBUM", sermon_album_for_lang(langue)},
        {"GENRE", "Sermon"},
        {"TRACKNUMBER", numero},
        {"DATE", annee},
        {"LANGUAGE", langue}
    };
    set_audio_tags(audioPath, tags, (int)(sizeof(tags) / sizeof(tags[0])), fichier_existe(imagePath) ? imagePath : NULL);

    if (!config->conserver_images && image_creee_maintenant && fichier_existe(imagePath)) {
        supprimer_fichier(imagePath);
        printf("✓ Image supprimée\n");
    }

    // Mémorisation de la version SEULEMENT après un téléchargement réussi
    if (!StateDB_SetAudioUpdatedAt(langue_lower, sermon->number, sermon->updated_at)) {
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
        set_audio_tags(audioPath, tags, (int)(sizeof(tags) / sizeof(tags[0])), NULL);

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
    

    printf("=== PKP Downloader - Version Portable ===\n");

    char dossier_exe[MAX_PATH];
    obtenir_dossier_executable(dossier_exe, sizeof(dossier_exe));
    printf("Dossier : %s\n", dossier_exe);

    lib net = {"net.dll", NULL};
    lib img = {"img.dll", NULL};
    lib meta = {"metadata.dll", NULL};
    lib m25 = {"matth25v6_db.dll", NULL};

    printf("\nChargement de net.dll...\n");
    net.value = LoadLibraryA(net.name);
    if (!net.value) {
        fprintf(stderr, "✗ Erreur net.dll (requis pour téléchargement)\n");
        printf("Appuyez sur Entrée pour quitter...");
        getchar();
        return -1;
    }
    g_downloadFile = (download_file_t)GetProcAddress(net.value, "DownloadFile");
    if (!g_downloadFile) {
        fprintf(stderr, "✗ Fonction DownloadFile introuvable dans net.dll\n");
        goto cleanup;
    }
    printf("✓ net.dll chargée\n");

    char dossier_ressources[MAX_PATH];
    snprintf(dossier_ressources, sizeof(dossier_ressources), "%s\\Ressources", dossier_exe);
    creer_dossier_recursif(dossier_ressources);

    char state_db_path[MAX_PATH];
    snprintf(state_db_path, sizeof(state_db_path), "%s\\pred_state.db", dossier_exe);
    if (!StateDB_Open(state_db_path)) {
        fprintf(stderr, "✗ Impossible d'ouvrir la base d'état locale\n");
        goto cleanup;
    }

    if (!initialiser_ressources(dossier_exe)) {
        fprintf(stderr, "\n✗ Erreur lors de l'initialisation des ressources\n");
        goto cleanup;
    }

    synchroniser_bases_donnees(dossier_ressources);

    Font font;
    initialiser_police(&font, dossier_exe);

    Config config = {0};
    configurer_application(&config, dossier_exe);

    printf("\nChargement des DLL restantes...\n");
    img.value = LoadLibraryA(img.name);
    meta.value = LoadLibraryA(meta.name);
    m25.value = LoadLibraryA(m25.name);

    if (!img.value) { fprintf(stderr, "✗ Erreur img.dll\n"); goto cleanup; }
    if (!meta.value) { fprintf(stderr, "✗ Erreur metadata.dll\n"); goto cleanup; }
    if (!m25.value) { fprintf(stderr, "✗ Erreur matth25v6_db.dll\n"); goto cleanup; }

    g_createImage = (create_image_t)GetProcAddress(img.value, "CreateImage");
    g_setTags = (set_tags_t)GetProcAddress(meta.value, "SetTags");
    g_setCoverImage = (set_cover_image_t)GetProcAddress(meta.value, "SetCoverImage");
    sermon_get_count_t Sermon_GetCount = (sermon_get_count_t)GetProcAddress(m25.value, "Sermon_GetCount");
    sermon_get_all_t Sermon_GetAll = (sermon_get_all_t)GetProcAddress(m25.value, "Sermon_GetAll");
    sing_get_all_t Sing_GetAll = (sing_get_all_t)GetProcAddress(m25.value, "Sing_GetAll");

    if (!g_createImage || !g_setTags || !g_setCoverImage || !Sermon_GetCount || !Sermon_GetAll || !Sing_GetAll) {
        fprintf(stderr, "✗ Fonctions manquantes dans les DLL\n");
        goto cleanup;
    }
    printf("✓ Toutes les DLL chargées\n");

    char commonDbPath[MAX_PATH];
    snprintf(commonDbPath, sizeof(commonDbPath), "%s\\common.db", dossier_ressources);
    telecharger_cantiques_actifs(commonDbPath, &config, Sing_GetAll);

    for (int i = 0; i < config.nb_langues - 1; i++) {
        for (int j = i + 1; j < config.nb_langues; j++) {
            if (strcmp(config.langues[i], config.langues[j]) > 0) {
                char temp[3];
                strcpy(temp, config.langues[i]);
                strcpy(config.langues[i], config.langues[j]);
                strcpy(config.langues[j], temp);
            }
        }
    }

    int compteurs_maj[4] = {0};

    for (int i = 0; i < config.nb_langues; i++) {
        const char* langue = config.langues[i];
        char langue_lower[3];
        langue_lower[0] = (char)tolower((unsigned char)langue[0]);
        langue_lower[1] = (char)tolower((unsigned char)langue[1]);
        langue_lower[2] = '\0';

        char dbPath[MAX_PATH];
        snprintf(dbPath, sizeof(dbPath), "%s\\matth25v6_%s.db", dossier_ressources, langue_lower);

        if (!fichier_existe(dbPath)) {
            fprintf(stderr, "⚠ Base introuvable : %s\n", dbPath);
            fprintf(stderr, "   La langue %s est passée.\n", langue);
            continue;
        }

        printf("\n========== %s ==========" "\n", langue);

        int sermonCount = 0;
        if (Sermon_GetCount(dbPath, &sermonCount) != M25_OK || sermonCount <= 0) {
            fprintf(stderr, "✗ Aucun sermon actif lu pour %s\n", langue);
            continue;
        }

        M25Sermon* sermons = (M25Sermon*)calloc((size_t)sermonCount, sizeof(M25Sermon));
        if (!sermons) {
            fprintf(stderr, "✗ Mémoire insuffisante pour %s\n", langue);
            continue;
        }

        int loaded = 0;
        if (Sermon_GetAll(dbPath, sermons, sermonCount, &loaded) != M25_OK) {
            fprintf(stderr, "✗ Erreur lecture sermons pour %s\n", langue);
            free(sermons);
            continue;
        }

        for (int s = 0; s < loaded; ++s) {
            int mis_a_jour = 0;
            process_sermon(&sermons[s], &config, &font, dossier_exe, langue, langue_lower, &mis_a_jour);
            if (mis_a_jour) compteurs_maj[i]++;
        }

        free(sermons);
        printf("\n✓ Terminé pour %s\n", langue);
    }

    printf("\n========== Résumé des mises à jour ==========\n");
    for (int i = 0; i < config.nb_langues; i++) {
        if (compteurs_maj[i] > 0) {
            printf("  %s : %d prédication(s) téléchargée(s) ou mise(s) à jour\n", config.langues[i], compteurs_maj[i]);
        } else {
            printf("  %s : aucune mise à jour\n", config.langues[i]);
        }
    }

cleanup:
    StateDB_Close();
    if (m25.value) FreeLibrary(m25.value);
    if (meta.value) FreeLibrary(meta.value);
    if (img.value) FreeLibrary(img.value);
    if (net.value) FreeLibrary(net.value);

    printf("\n=== Téléchargement terminé ===\n");
    printf("Appuyez sur Entrée pour quitter...");
    getchar();
    return 0;
}

