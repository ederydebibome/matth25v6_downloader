// utils.cpp - Implémentation des fonctions utilitaires

#include "utils.h"
#include "resource.h"
#include "projetc.h"
#include "state_db.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <ctype.h>

// Variables externes (déclarées dans main.c)
extern download_file_t g_downloadFile;

// UTF-8 -> UTF-16 (échoue si l'UTF-8 est invalide ou trop long)
int utf8_vers_wide(const char* utf8, wchar_t* out, int outCount) {
    if (!utf8 || !out || outCount <= 0) return 0;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, outCount);
    if (n <= 0) { out[0] = L'\0'; return 0; }
    return 1;
}

int supprimer_fichier(const char* path_utf8) {
    wchar_t w[MAX_PATH];
    if (!utf8_vers_wide(path_utf8, w, MAX_PATH)) return 0;
    return DeleteFileW(w) ? 1 : 0;
}

// Copie tronquée UTF-8 : recule jusqu'au début du dernier caractère entier.
void copier_texte_utf8(char* dst, size_t size, const char* src) {
    if (!dst || size == 0) return;
    if (!src) src = "";
    size_t n = strlen(src);
    if (n > size - 1) {
        n = size - 1;
        while (n > 0 && (((unsigned char)src[n]) & 0xC0) == 0x80) --n;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void normaliser_couleur(char* couleur, size_t size) {
    if (!couleur || size < 8) return;
    size_t len = strlen(couleur);
    while (len > 0 && isspace((unsigned char)couleur[len - 1])) couleur[--len] = '\0';
    char* debut = couleur;
    while (*debut && isspace((unsigned char)*debut)) debut++;
    if (debut != couleur) memmove(couleur, debut, strlen(debut) + 1);
    len = strlen(couleur);
    if (len == 6) {
        for (size_t i = 0; i < 6; i++) {
            if (!isxdigit((unsigned char)couleur[i])) return;
        }
        memmove(couleur + 1, couleur, 7);   // 6 caractères + '\0'
        couleur[0] = '#';
    }
}

// Obtenir le dossier d'exécution (en UTF-8)
void obtenir_dossier_executable(char* buffer, size_t size) {
    if (!buffer || size == 0) return;
    buffer[0] = '\0';
    wchar_t wpath[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, wpath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    wchar_t* last_slash = wcsrchr(wpath, L'\\');
    if (last_slash) *last_slash = L'\0';
    if (!WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buffer, (int)size, NULL, NULL)) {
        buffer[0] = '\0';
    }
}

// Vérifier si un fichier existe
int fichier_existe(const char* path) {
    wchar_t w[MAX_PATH];
    if (!utf8_vers_wide(path, w, MAX_PATH)) return 0;
    DWORD attrib = GetFileAttributesW(w);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Créer un dossier récursivement
void creer_dossier_recursif(const char* path) {
    wchar_t temp[MAX_PATH];
    if (!utf8_vers_wide(path, temp, MAX_PATH)) return;

    size_t len = wcslen(temp);
    if (len > 0 && temp[len - 1] == L'\\') {
        temp[len - 1] = L'\0';
    }

    for (wchar_t* p = temp + 1; *p; p++) {
        if (*p == L'\\') {
            *p = L'\0';
            CreateDirectoryW(temp, NULL);
            *p = L'\\';
        }
    }
    CreateDirectoryW(temp, NULL);
}

// Extraire une ressource embarquée vers un fichier
int extraire_ressource(int resource_id, const char* output_path) {
    HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(resource_id), RT_RCDATA);
    if (!hResInfo) {
        fprintf(stderr, "Ressource %d introuvable\n", resource_id);
        return 0;
    }

    HGLOBAL hResData = LoadResource(NULL, hResInfo);
    if (!hResData) {
        fprintf(stderr, "Erreur chargement ressource %d\n", resource_id);
        return 0;
    }

    DWORD size = SizeofResource(NULL, hResInfo);
    void* data = LockResource(hResData);
    if (!data) {
        fprintf(stderr, "Erreur verrouillage ressource %d\n", resource_id);
        return 0;
    }

    // Créer le dossier parent si nécessaire
    char temp_path[MAX_PATH];
    strncpy(temp_path, output_path, sizeof(temp_path) - 1);
    temp_path[sizeof(temp_path) - 1] = '\0';
    
    char* last_slash = strrchr(temp_path, '\\');
    if (last_slash) {
        *last_slash = '\0';
        creer_dossier_recursif(temp_path);
    }

    // Écrire le fichier
    wchar_t wout[MAX_PATH];
    if (!utf8_vers_wide(output_path, wout, MAX_PATH)) {
        fprintf(stderr, "Chemin invalide : %s\n", output_path);
        return 0;
    }
    HANDLE hFile = CreateFileW(wout, GENERIC_WRITE, 0, NULL, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Erreur création fichier : %s\n", output_path);
        return 0;
    }

    DWORD written;
    BOOL result = WriteFile(hFile, data, size, &written, NULL);
    CloseHandle(hFile);

    if (!result || written != size) {
        fprintf(stderr, "Erreur écriture fichier : %s\n", output_path);
        return 0;
    }

    return 1;
}

// Ouvre un fichier (chemin UTF-8) en lecture
static FILE* ouvrir_lecture_utf8(const char* path_utf8) {
    wchar_t w[MAX_PATH];
    if (!utf8_vers_wide(path_utf8, w, MAX_PATH)) return NULL;
    return _wfopen(w, L"rb");
}

// Télécharger une base de données depuis l'API
int telecharger_base_donnees(const char* initial, const char* dossier_destination) {
    char api_url[512];
    char json_temp_path[MAX_PATH];
    char db_path[MAX_PATH];
    char nom_fichier[64];
    
    // Construire l'URL de l'API
    snprintf(api_url, sizeof(api_url), 
             "https://api.philippekacou.org/api/%s/download-url", initial);
    
    // Construire le nom du fichier
    if (strcmp(initial, "common") == 0) {
        strcpy(nom_fichier, "common.db");
    } else {
        // Extraire la partie après le tiret (ex: "fr-fr" -> "fr")
        char langue[3] = {0};
        const char* tiret = strchr(initial, '-');
        if (tiret && tiret[1]) {
            langue[0] = tiret[1];
            langue[1] = tiret[2] ? tiret[2] : '\0';
            snprintf(nom_fichier, sizeof(nom_fichier), "matth25v6_%s.db", langue);
        } else {
            snprintf(nom_fichier, sizeof(nom_fichier), "matth25v6_%s.db", initial);
        }
    }
    
    // Construire les chemins
    snprintf(db_path, sizeof(db_path), "%s\\%s", dossier_destination, nom_fichier);
    snprintf(json_temp_path, sizeof(json_temp_path), "%s\\temp_api_response.json", dossier_destination);
    
    printf("  → Récupération de l'URL de téléchargement depuis l'API...\n");
    
    // Étape 1 : Télécharger le JSON de l'API
    if (!g_downloadFile || !g_downloadFile(api_url, json_temp_path)) {
        fprintf(stderr, "  ✗ Échec de la récupération de l'URL depuis l'API\n");
        return 0;
    }
    
    // Étape 2 : Lire le JSON et extraire download_url
    FILE* json_file = ouvrir_lecture_utf8(json_temp_path);
    if (!json_file) {
        fprintf(stderr, "  ✗ Impossible de lire la réponse de l'API\n");
        supprimer_fichier(json_temp_path);
        return 0;
    }
    
    char json_content[8192] = {0};
    size_t bytes_read = fread(json_content, 1, sizeof(json_content) - 1, json_file);
    fclose(json_file);
    
    // Supprimer le fichier temporaire JSON
    supprimer_fichier(json_temp_path);
    
    if (bytes_read == 0) {
        fprintf(stderr, "  ✗ Réponse API vide\n");
        return 0;
    }
    
    // Étape 3 : Parser le JSON pour extraire download_url
    char* url_start = strstr(json_content, "\"download_url\":\"");
    if (!url_start) {
        fprintf(stderr, "  ✗ Format JSON invalide (download_url introuvable)\n");
        return 0;
    }
    
    url_start += 16; // Sauter "download_url":"
    
    char* url_end = strchr(url_start, '"');
    if (!url_end) {
        fprintf(stderr, "  ✗ Format JSON invalide (guillemet de fin manquant)\n");
        return 0;
    }
    
    // Extraire l'URL
    size_t url_length = url_end - url_start;
    char download_url[2048] = {0};
    strncpy(download_url, url_start, url_length < sizeof(download_url) ? url_length : sizeof(download_url) - 1);
    
    // Remplacer les \/ par / (échappement JSON)
    char cleaned_url[2048] = {0};
    char* src = download_url;
    char* dst = cleaned_url;
    while (*src && (dst - cleaned_url) < sizeof(cleaned_url) - 1) {
        if (*src == '\\' && *(src + 1) == '/') {
            *dst++ = '/';
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    
    printf("  → URL extraite, téléchargement de %s...\n", nom_fichier);
    
    // Étape 4 : Télécharger le fichier depuis l'URL extraite
    if (g_downloadFile && g_downloadFile(cleaned_url, db_path)) {
        printf("  ✓ Base de données téléchargée : %s\n", nom_fichier);
        return 1;
    } else {
        fprintf(stderr, "  ✗ Échec du téléchargement de %s\n", nom_fichier);
        return 0;
    }
}

// Interroger l'API pour connaître le updated_at courant d'une langue/base
int recuperer_update_langue(const char* code_api, const char* dossier_temp,
                             char* out_updated_at, size_t size) {
    if (!code_api || !dossier_temp || !out_updated_at || size == 0) return 0;

    char api_url[512];
    snprintf(api_url, sizeof(api_url),
             "https://api.philippekacou.org/api/en-en/langue-releases/all-new-updates?langs[]=%s",
             code_api);

    char json_temp_path[MAX_PATH];
    snprintf(json_temp_path, sizeof(json_temp_path), "%s\\temp_update_%s.json", dossier_temp, code_api);

    if (!g_downloadFile || !g_downloadFile(api_url, json_temp_path)) {
        return 0;
    }

    FILE* json_file = ouvrir_lecture_utf8(json_temp_path);
    if (!json_file) {
        supprimer_fichier(json_temp_path);
        return 0;
    }

    char json_content[2048] = {0};
    size_t bytes_read = fread(json_content, 1, sizeof(json_content) - 1, json_file);
    fclose(json_file);
    supprimer_fichier(json_temp_path);

    if (bytes_read == 0) return 0;

    // Le format renvoyé est : [{"langue":"fr-fr","updated_at":"..."}]
    char* val_start = strstr(json_content, "\"updated_at\":\"");
    if (!val_start) return 0;
    val_start += 14; // sauter "updated_at":"

    char* val_end = strchr(val_start, '"');
    if (!val_end) return 0;

    size_t len = (size_t)(val_end - val_start);
    if (len >= size) len = size - 1;
    strncpy(out_updated_at, val_start, len);
    out_updated_at[len] = '\0';

    return 1;
}

// Synchronise common.db + les 4 bases de langue : ne (re)télécharge que ce
// qui a réellement changé côté serveur, en comparant avec la base d'état
// locale (state_db).
int synchroniser_bases_donnees(const char* dossier_ressources) {
    typedef struct {
        const char* interne;  // clé utilisée dans la base d'état
        const char* api;      // valeur attendue par l'API (langs[]=...)
        const char* fichier;  // nom du fichier local
        const char* label;    // libellé pour l'affichage console
    } LangueDB;

    static const LangueDB LANGUES_DB[] = {
        {"common", "common", "common.db",       "Commune"},
        {"fr",     "fr-fr",  "matth25v6_fr.db", "Français"},
        {"en",     "en-en",  "matth25v6_en.db", "Anglais"},
        {"es",     "es-es",  "matth25v6_es.db", "Espagnol"},
        {"pt",     "pt-pt",  "matth25v6_pt.db", "Portugais"},
    };

    printf("\n--- Vérification des mises à jour (bases officielles) ---\n");

    int ok = 1;
    for (int i = 0; i < 5; i++) {
        const LangueDB* l = &LANGUES_DB[i];

        char db_path[MAX_PATH];
        snprintf(db_path, sizeof(db_path), "%s\\%s", dossier_ressources, l->fichier);
        int absente = !fichier_existe(db_path);

        char remote_updated_at[64] = {0};
        int verif_ok = recuperer_update_langue(l->api, dossier_ressources, remote_updated_at, sizeof(remote_updated_at));
        if (!verif_ok) {
            if (!absente) {
                fprintf(stderr, "  ⚠ %s : vérification impossible (API indisponible)\n", l->label);
                continue;
            }
            printf("  → %s : absente, téléchargement sans vérification de version...\n", l->label);
            if (!telecharger_base_donnees(l->api, dossier_ressources)) {
                fprintf(stderr, "  ✗ %s : échec du téléchargement\n", l->label);
                ok = 0;
            }
            continue;
        }

        char local_updated_at[64] = {0};
        int connu = StateDB_GetLangueUpdatedAt(l->interne, local_updated_at, sizeof(local_updated_at));

        if (!connu || absente || strcmp(remote_updated_at, local_updated_at) != 0) {
            printf("  → %s : nouvelle version disponible (%s), téléchargement...\n", l->label, remote_updated_at);
            if (telecharger_base_donnees(l->api, dossier_ressources)) {
                StateDB_SetLangueUpdatedAt(l->interne, remote_updated_at);
            } else {
                fprintf(stderr, "  ✗ %s : échec du téléchargement\n", l->label);
                ok = 0;
            }
        } else {
            printf("  ✓ %s : déjà à jour (%s)\n", l->label, local_updated_at);
        }
    }

    return ok;
}

// Initialiser les ressources (extraction des ressources embarquées uniquement ;
// les bases officielles sont désormais gérées par synchroniser_bases_donnees)
int initialiser_ressources(const char* dossier_exe) {
    char dossier_ressources[MAX_PATH];
    snprintf(dossier_ressources, sizeof(dossier_ressources), "%s\\Ressources", dossier_exe);
    
    printf("\n=== Vérification des ressources ===\n");
    
    // Créer le dossier Ressources
    creer_dossier_recursif(dossier_ressources);
    
    // 1. Extraire les templates d'images et la police (ressources embarquées)
    printf("\n--- Ressources embarquées ---\n");
    
    struct {
        int id;
        const char* nom;
        char path[MAX_PATH];
    } ressources[] = {
        {IDR_IMG_FR, "Template FR", ""},
        {IDR_IMG_EN, "Template EN", ""},
        {IDR_FONT_LORA, "Police Lora-Bold", ""}
    };
    
    // Construire les chemins
    snprintf(ressources[0].path, MAX_PATH, "%s\\img_FR.png", dossier_ressources);
    snprintf(ressources[1].path, MAX_PATH, "%s\\img_EN.png", dossier_ressources);
    snprintf(ressources[2].path, MAX_PATH, "%s\\Lora-Bold.ttf", dossier_ressources);
    
    // Extraire les ressources embarquées si nécessaire (celles-ci ne changent pas)
    for (int i = 0; i < 3; i++) {
        if (!fichier_existe(ressources[i].path)) {
            printf("  Extraction : %s... ", ressources[i].nom);
            if (extraire_ressource(ressources[i].id, ressources[i].path)) {
                printf("✓\n");
            } else {
                printf("✗ ÉCHEC\n");
                return 0;
            }
        } else {
            printf("  ✓ %s déjà présent\n", ressources[i].nom);
        }
    }
    
    printf("✓ Initialisation des ressources terminée\n");
    return 1;
}

// Initialiser la structure de police
void initialiser_police(Font* font, const char* dossier_exe) {
    snprintf(font->path, sizeof(font->path), "%s\\Ressources\\Lora-Bold.ttf", dossier_exe);
    strcpy(font->name, "Lora-Bold");
}

// Lire une entrée utilisateur avec valeur par défaut
void lire_input(const char* prompt, const char* defaut, char* buffer, size_t size) {
    printf("%s [Défaut: %s]: ", prompt, defaut);
    fflush(stdout);
    
    if (fgets(buffer, size, stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        
        // Si vide, utiliser la valeur par défaut
        if (strlen(buffer) == 0) {
            strncpy(buffer, defaut, size - 1);
            buffer[size - 1] = '\0';
        }
    } else {
        strncpy(buffer, defaut, size - 1);
        buffer[size - 1] = '\0';
    }
}

// Configurer l'application
void configurer_application(Config* config, const char* dossier_exe) {
    char buffer[512];
    
    printf("\n=== Configuration ===\n\n");
    
    // 1. Dossiers d'enregistrement (fixes, relatifs au dossier du logiciel)
    snprintf(config->path_predications, sizeof(config->path_predications),
             "%s\\Matthieu25v6\\Music\\Predications", dossier_exe);
    snprintf(config->path_cantiques, sizeof(config->path_cantiques),
             "%s\\Matthieu25v6\\Music\\Cantiques", dossier_exe);
    // Les images de couverture restent sur l'ordinateur : le dossier Matthieu25v6
    // est partagé avec les téléphones, qui n'en ont pas besoin.
    snprintf(config->path_images, sizeof(config->path_images), "%s\\img", dossier_exe);
    
    // 2. Conserver les images
    lire_input("Conserver les images après traitement ? (O/N)", "O", buffer, sizeof(buffer));
    config->conserver_images = (buffer[0] == 'O' || buffer[0] == 'o') ? 1 : 0;
    
    // 2b. Séparer les prédications par langue ?
    lire_input("Séparer les prédications par langue (dossiers Fr, En...) ? (O/N)", "N", buffer, sizeof(buffer));
    config->separer_par_langue = (buffer[0] == 'O' || buffer[0] == 'o') ? 1 : 0;

    // 3. Langues (Fr, En, Es, Pt)
    printf("\nLangues disponibles : Fr, En, Es, Pt\n");
    lire_input("Langues à télécharger (séparées par des virgules)", "En,Fr", buffer, sizeof(buffer));
    
    // Parser les langues
    config->nb_langues = 0;
    
    // Si entrée vide ou invalide, utiliser En,Fr par défaut (ANGLAIS EN PREMIER)
    if (strlen(buffer) == 0) {
        strcpy(config->langues[0], "En");
        strcpy(config->langues[1], "Fr");
        config->nb_langues = 2;
    } else {
        char* token = strtok(buffer, ",; ");
        while (token != NULL && config->nb_langues < 4) {
            // Nettoyer les espaces
            while (*token == ' ') token++;
            
            // Vérifier si c'est une langue valide (insensible à la casse)
            if (strlen(token) >= 2) {
                // Normaliser en format "Fr" (première lettre majuscule, deuxième minuscule)
                char lang[3];
                lang[0] = toupper(token[0]);
                lang[1] = tolower(token[1]);
                lang[2] = '\0';
                
                // Vérifier si c'est une langue valide
                if (strcmp(lang, "Fr") == 0 || strcmp(lang, "En") == 0 || 
                    strcmp(lang, "Es") == 0 || strcmp(lang, "Pt") == 0) {
                    
                    // Vérifier si cette langue n'est pas déjà dans la liste (éviter doublons)
                    int deja_presente = 0;
                    for (int i = 0; i < config->nb_langues; i++) {
                        if (strcmp(config->langues[i], lang) == 0) {
                            deja_presente = 1;
                            break;
                        }
                    }
                    
                    if (!deja_presente) {
                        strcpy(config->langues[config->nb_langues], lang);
                        config->nb_langues++;
                    }
                }
            }
            token = strtok(NULL, ",; ");
        }
        
        // Si aucune langue valide, utiliser En,Fr par défaut (ANGLAIS EN PREMIER)
        if (config->nb_langues == 0) {
            strcpy(config->langues[0], "En");
            strcpy(config->langues[1], "Fr");
            config->nb_langues = 2;
        }
    }
    
    // 4. Couleur du texte sur la cover
    lire_input("Couleur du texte sur la cover (ex: red, white, #FF0000)", "#30d5c8", buffer, sizeof(buffer));
    copier_texte_utf8(config->couleur_cover, sizeof(config->couleur_cover), buffer);
    normaliser_couleur(config->couleur_cover, sizeof(config->couleur_cover));
    
    // Afficher la configuration
    printf("\n=== Configuration choisie ===\n");
    printf("Prédications : %s\n", config->path_predications);
    printf("Cantiques    : %s\n", config->path_cantiques);
    printf("Images       : %s\n", config->path_images);
    printf("Séparation par langue : %s\n", config->separer_par_langue ? "Oui" : "Non");
    printf("Conserver images : %s\n", config->conserver_images ? "Oui" : "Non");
    printf("Langues : ");
    for (int i = 0; i < config->nb_langues; i++) {
        printf("%s%s", config->langues[i], (i < config->nb_langues - 1) ? ", " : "");
    }
    printf("\n");
    printf("Couleur cover : %s\n", config->couleur_cover);
    printf("\n");
}