// utils.h - Fonctions utilitaires pour le programme

#ifndef UTILS_H
#define UTILS_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration utilisateur
typedef struct {
    char path_predications[MAX_PATH];  // <dossier du logiciel>\\Matthieu25v6\\Music\\Predications
    char path_cantiques[MAX_PATH];     // <dossier du logiciel>\\Matthieu25v6\\Music\\Cantiques
    char path_images[MAX_PATH];        // <dossier du logiciel>\\img  (reste sur l'ordinateur, hors de Matthieu25v6)
    int separer_par_langue;            // 1 = Predications\\Fr, Predications\\En... ; 0 = tout dans Predications
    int conserver_images;
    char langues[4][3];  // Fr, En, Es, Pt
    int nb_langues;
    char couleur_cover[32];  // Couleur du texte sur la cover (ex: "red", "white", "#FF0000")
} Config;

// Structure pour gérer les polices
typedef struct {
    char path[MAX_PATH];  // Chemin complet : "C:\...\Ressources\Lora-Bold.ttf"
    char name[64];        // Nom pour img.dll : "Lora-Bold"
} Font;

// Convention : TOUS les `char*` du programme (chemins, titres, URL) sont en UTF-8.
// Les fonctions ci-dessous convertissent en UTF-16 et appellent les API Windows
// "W" : ne jamais utiliser les API "...A" avec ces chaînes (page de code ANSI).
int  utf8_vers_wide(const char* utf8, wchar_t* out, int outCount);
int  supprimer_fichier(const char* path_utf8);

// Copie tronquée qui ne coupe jamais un caractère UTF-8 multi-octets.
void copier_texte_utf8(char* dst, size_t size, const char* src);

// "30d5c8" -> "#30d5c8" (ImageMagick exige le '#'). Sans effet sur les autres formes.
void normaliser_couleur(char* couleur, size_t size);

// Fonctions de gestion des fichiers
void obtenir_dossier_executable(char* buffer, size_t size);
int fichier_existe(const char* path);
void creer_dossier_recursif(const char* path);

// Gestion des ressources embarquées
int extraire_ressource(int resource_id, const char* output_path);
int initialiser_ressources(const char* dossier_exe);
void initialiser_police(Font* font, const char* dossier_exe);

// 🆕 AJOUTER CETTE LIGNE :
int telecharger_base_donnees(const char* initial, const char* dossier_destination);

// Interroge l'API pour connaître le updated_at courant d'une langue/base
// donnée ("common", "fr-fr", "en-en", "es-es", "pt-pt").
// Retourne 1 en cas de succès (out_updated_at rempli), 0 sinon.
int recuperer_update_langue(const char* code_api, const char* dossier_temp,
                             char* out_updated_at, size_t size);

// Vérifie chaque base officielle (common + 4 langues) auprès de l'API et ne
// (re)télécharge que celles qui ont réellement changé depuis la dernière
// exécution (comparaison via la base d'état locale, state_db).
int synchroniser_bases_donnees(const char* dossier_ressources);

// Configuration interactive
void lire_input(const char* prompt, const char* defaut, char* buffer, size_t size);
void configurer_application(Config* config, const char* dossier_exe);

#ifdef __cplusplus
}
#endif

#endif // UTILS_H