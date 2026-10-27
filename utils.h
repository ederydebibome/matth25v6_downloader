// utils.h - Fonctions utilitaires pour le programme

#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include "matth25v6_db.h"

#ifdef __cplusplus
extern "C" {
#endif

// Un code de langue de l'API est court ("fr-fr", "cd-kgdc"), 16 laisse de la marge
#define LANGUE_CODE_MAX 16
// Nombre max de langues gérées, que ce soit celles du serveur ou celles qu'on choisit
#define LANGUES_MAX 512

// Configuration utilisateur
typedef struct {
    char path_predications[MAX_PATH];  // <dossier du logiciel>\\Matthieu25v6\\Music\\Predications
    char path_cantiques[MAX_PATH];     // <dossier du logiciel>\\Matthieu25v6\\Music\\Cantiques
    int separer_par_langue;            // 1 = un sous-dossier par langue (Predications\\fr, ...) ; 0 = tout dans Predications
    char langues[LANGUES_MAX][LANGUE_CODE_MAX];  // codes de l'API des langues choisies ("fr-fr", "ci-aba"...)
    int nb_langues;
} Config;

// Convention : TOUS les `char*` du programme (chemins, titres, URL) sont en UTF-8.
// Les fonctions ci-dessous convertissent en UTF-16 et appellent les API "W" :
// ne jamais utiliser les API "...A" avec ces chaînes (page de code ANSI).
int  utf8_vers_wide(const char* utf8, wchar_t* out, int outCount);
int  supprimer_fichier(const char* path_utf8);

// Copie tronquée qui ne coupe jamais un caractère UTF-8 multi-octets.
void copier_texte_utf8(char* dst, size_t size, const char* src);

// Fonctions de gestion des fichiers
void obtenir_dossier_executable(char* buffer, size_t size);
int fichier_existe(const char* path);
void creer_dossier_recursif(const char* path);

// Nom court d'une langue pour nos fichiers et notre base d'état : "fr-fr" -> "fr",
// mais "ci-aba" reste "ci-aba" (voir utils.cpp pour la raison).
void cle_langue(const char* code_api, char* out, size_t size);

// 1 si le code ne contient que des lettres, chiffres, '-' et '_' (et pas trop long)
int code_langue_valide(const char* code);

// Nom du fichier local d'une base : "common" -> common.db, sinon matth25v6_<cle>.db
void nom_fichier_base(const char* code_api, char* out, size_t size);

// Télécharge une base depuis l'API ("common", "fr-fr", "ci-aba"...)
int telecharger_base_donnees(const char* initial, const char* dossier_destination);

// Interroge l'API pour connaître le updated_at courant d'une base
// ("common", "fr-fr", "ci-aba"...).
// Retourne 1 en cas de succès (out_updated_at rempli), 0 sinon.
int recuperer_update_langue(const char* code_api, const char* dossier_temp,
                             char* out_updated_at, size_t size);

// Vérifie une base auprès de l'API et ne la (re)télécharge que si elle a changé
// depuis la dernière exécution (comparaison avec la base d'état locale).
// Retourne 1 si une base utilisable est sur le disque à la fin, 0 sinon.
int synchroniser_base(const char* code_api, const char* dossier_ressources, const char* label);

// Configuration interactive. `disponibles` est la liste des langues lue dans common.db :
// elle est triée et nettoyée sur place. Retourne 0 si aucune langue n'a pu être choisie.
void lire_input(const char* prompt, const char* defaut, char* buffer, size_t size);
int configurer_application(Config* config, const char* dossier_exe,
                           M25Langue* disponibles, int nb_disponibles);

#ifdef __cplusplus
}
#endif

#endif // UTILS_H
