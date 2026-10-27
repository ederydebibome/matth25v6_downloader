// utils.cpp - Implémentation des fonctions utilitaires

#include "utils.h"
#include "projetc.h"
#include "state_db.h"
#include <stdio.h>
#include <stdlib.h>
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

// Ouvre un fichier (chemin UTF-8) en lecture
static FILE* ouvrir_lecture_utf8(const char* path_utf8) {
    wchar_t w[MAX_PATH];
    if (!utf8_vers_wide(path_utf8, w, MAX_PATH)) return NULL;
    return _wfopen(w, L"rb");
}

// Le code de l'API ("fr-fr", "ci-aba") sert dans les URL. Pour nos fichiers et la base
// d'état je garde l'ancien nom court quand c'est possible ("fr-fr" -> "fr"), comme ça
// ce qui est déjà téléchargé ne bouge pas. Je ne raccourcis que si les deux moitiés
// sont identiques : sinon "ci-aba" et "ci-abu" finiraient toutes les deux en "ab".
void cle_langue(const char* code_api, char* out, size_t size) {
    if (!out || size == 0) return;
    out[0] = '\0';
    if (!code_api) return;

    const char* tiret = strchr(code_api, '-');
    if (tiret && tiret[1]) {
        size_t len_pays = (size_t)(tiret - code_api);
        if (strlen(tiret + 1) == len_pays && strncmp(code_api, tiret + 1, len_pays) == 0) {
            copier_texte_utf8(out, size, tiret + 1);
            return;
        }
    }
    copier_texte_utf8(out, size, code_api);
}

// Ces codes finissent dans des URL et des noms de fichiers : on laisse passer
// que des lettres, des chiffres, '-' et '_'.
int code_langue_valide(const char* code) {
    if (!code || !code[0]) return 0;
    size_t n = strlen(code);
    if (n >= LANGUE_CODE_MAX) return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)code[i];
        if (!(isalnum(c) || c == '-' || c == '_')) return 0;
    }
    return 1;
}

void nom_fichier_base(const char* code_api, char* out, size_t size) {
    if (!out || size == 0) return;
    if (code_api && strcmp(code_api, "common") == 0) {
        snprintf(out, size, "common.db");
        return;
    }
    char cle[LANGUE_CODE_MAX];
    cle_langue(code_api, cle, sizeof(cle));
    snprintf(out, size, "matth25v6_%s.db", cle);
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
    
    // le nom du fichier local, c'est nous qui le choisissons
    nom_fichier_base(initial, nom_fichier, sizeof(nom_fichier));

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

// Regarde auprès de l'API si une base a changé depuis la dernière fois et ne la
// retélécharge que si c'est le cas. Si l'API ne répond pas, on garde la base qu'on a.
int synchroniser_base(const char* code_api, const char* dossier_ressources, const char* label) {
    char cle[LANGUE_CODE_MAX];
    cle_langue(code_api, cle, sizeof(cle));

    char nom_fichier[64];
    nom_fichier_base(code_api, nom_fichier, sizeof(nom_fichier));

    char db_path[MAX_PATH];
    snprintf(db_path, sizeof(db_path), "%s\\%s", dossier_ressources, nom_fichier);
    int absente = !fichier_existe(db_path);

    char remote_updated_at[64] = {0};
    if (!recuperer_update_langue(code_api, dossier_ressources, remote_updated_at, sizeof(remote_updated_at))) {
        if (!absente) {
            fprintf(stderr, "  ⚠ %s : vérification impossible (API indisponible), on garde la base locale\n", label);
            return 1;
        }
        printf("  → %s : absente, téléchargement sans vérification de version...\n", label);
        if (!telecharger_base_donnees(code_api, dossier_ressources)) {
            fprintf(stderr, "  ✗ %s : échec du téléchargement\n", label);
            return 0;
        }
        return 1;
    }

    char local_updated_at[64] = {0};
    int connu = StateDB_GetLangueUpdatedAt(cle, local_updated_at, sizeof(local_updated_at));

    if (connu && !absente && strcmp(remote_updated_at, local_updated_at) == 0) {
        printf("  ✓ %s : déjà à jour (%s)\n", label, local_updated_at);
        return 1;
    }

    printf("  → %s : nouvelle version disponible (%s), téléchargement...\n", label, remote_updated_at);
    if (!telecharger_base_donnees(code_api, dossier_ressources)) {
        fprintf(stderr, "  ✗ %s : échec du téléchargement\n", label);
        return !absente;   // si on avait déjà une base, on peut continuer avec
    }
    StateDB_SetLangueUpdatedAt(cle, remote_updated_at);
    return 1;
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

static int comparer_codes(const void* a, const void* b) {
    const M25Langue* la = (const M25Langue*)a;
    const M25Langue* lb = (const M25Langue*)b;
    return strcmp(la->initial, lb->initial);
}

static int est_oui(const char* s) {
    return s[0] == 'O' || s[0] == 'o';
}

// Retrouve une langue d'après ce que la personne a tapé : un numéro de la liste, le
// code complet ("ci-aba") ou le code court quand il existe ("fr" pour "fr-fr").
// Renvoie l'indice dans la liste, ou -1.
static int trouver_langue(const M25Langue* liste, int nb, const char* mot) {
    char* fin = NULL;
    long num = strtol(mot, &fin, 10);
    if (fin != mot && *fin == '\0') {
        return (num >= 1 && num <= nb) ? (int)(num - 1) : -1;
    }
    for (int i = 0; i < nb; i++) {
        char cle[LANGUE_CODE_MAX];
        cle_langue(liste[i].initial, cle, sizeof(cle));
        if (_stricmp(mot, liste[i].initial) == 0 || _stricmp(mot, cle) == 0) return i;
    }
    return -1;
}

// Configurer l'application
int configurer_application(Config* config, const char* dossier_exe,
                           M25Langue* disponibles, int nb_disponibles) {
    char buffer[2048];

    printf("\n=== Configuration ===\n\n");

    // Dossiers de destination, toujours à côté de l'exécutable
    snprintf(config->path_predications, sizeof(config->path_predications),
             "%s\\Matthieu25v6\\Music\\Predications", dossier_exe);
    snprintf(config->path_cantiques, sizeof(config->path_cantiques),
             "%s\\Matthieu25v6\\Music\\Cantiques", dossier_exe);

    lire_input("Séparer les prédications par langue (un dossier par langue) ? (O/N)", "N", buffer, sizeof(buffer));
    config->separer_par_langue = est_oui(buffer);

    // On vire les codes louches, puis on trie par code : les langues d'un même pays se suivent
    int n = 0;
    for (int i = 0; i < nb_disponibles; i++) {
        if (!code_langue_valide(disponibles[i].initial)) continue;
        if (n != i) disponibles[n] = disponibles[i];
        n++;
    }
    if (n == 0) {
        fprintf(stderr, "✗ Aucune langue disponible dans common.db\n");
        return 0;
    }
    qsort(disponibles, (size_t)n, sizeof(M25Langue), comparer_codes);

    // Le code passe en premier : certains noms (arabe, hindi...) s'affichent mal dans la console
    printf("\nLangues disponibles (%d) :\n", n);
    for (int i = 0; i < n; i++) {
        const M25Langue* l = &disponibles[i];
        printf("  %3d. %-9s %s", i + 1, l->initial, l->libelle);
        if (l->country_name[0]) printf(" (%s)", l->country_name);
        printf("\n");
    }

    char* choisie = (char*)calloc((size_t)n, 1);
    if (!choisie) {
        fprintf(stderr, "✗ Mémoire insuffisante\n");
        return 0;
    }

    int nb_choisies = 0;
    for (int essai = 0; essai < 5 && nb_choisies == 0; essai++) {
        printf("\nTape des numéros ou des codes séparés par des virgules (ex: 2, fr, ci-aba), ou 'all' pour tout.\n");
        lire_input("Langues à télécharger", "en-en,fr-fr", buffer, sizeof(buffer));

        int tout = 0;
        for (char* mot = strtok(buffer, ",; \t"); mot; mot = strtok(NULL, ",; \t")) {
            if (_stricmp(mot, "all") == 0 || _stricmp(mot, "tout") == 0 || _stricmp(mot, "toutes") == 0) {
                tout = 1;
                continue;
            }
            int idx = trouver_langue(disponibles, n, mot);
            if (idx < 0) {
                printf("  \"%s\" : inconnu, ignoré\n", mot);
            } else if (!choisie[idx]) {
                choisie[idx] = 1;
                nb_choisies++;
            }
        }

        if (tout) {
            char rep[16];
            printf("\n%d langues avec toutes leurs prédications, ça va être très long et très lourd.\n", n);
            lire_input("Tu confirmes ? (O/N)", "N", rep, sizeof(rep));
            if (est_oui(rep)) {
                memset(choisie, 1, (size_t)n);
                nb_choisies = n;
            }
        }

        if (nb_choisies == 0) printf("Aucune langue valide, on recommence.\n");
    }

    config->nb_langues = 0;
    for (int i = 0; i < n; i++) {
        if (!choisie[i]) continue;
        copier_texte_utf8(config->langues[config->nb_langues], LANGUE_CODE_MAX, disponibles[i].initial);
        config->nb_langues++;
    }
    free(choisie);

    if (config->nb_langues == 0) return 0;

    printf("\n=== Configuration choisie ===\n");
    printf("Prédications : %s\n", config->path_predications);
    printf("Cantiques    : %s\n", config->path_cantiques);
    printf("Séparation par langue : %s\n", config->separer_par_langue ? "Oui" : "Non");
    printf("Langues (%d) : ", config->nb_langues);
    for (int i = 0; i < config->nb_langues; i++) {
        printf("%s%s", config->langues[i], (i < config->nb_langues - 1) ? ", " : "");
    }
    printf("\n\n");
    return 1;
}
