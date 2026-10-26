// state_db.h - Base de données locale de l'application (état de synchro)
//
// Contrairement à matth25v6_db.dll qui ne sert qu'à LIRE les bases
// officielles téléchargées, ce module gère une base SQLite propre à
// l'application (pred_state.db) qui mémorise :
//   - la dernière date de mise à jour connue par langue/base ("common",
//     "fr", "en", "es", "pt")
//   - la dernière date de mise à jour connue pour chaque prédication
//     individuelle (par langue + numéro)
//
// Elle est compilée directement dans l'exécutable (pas une DLL séparée).

#ifndef STATE_DB_H
#define STATE_DB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Ouvre (ou crée si absente) la base d'état à l'emplacement donné et crée
// le schéma si nécessaire. Retourne 1 en cas de succès, 0 sinon.
int StateDB_Open(const char* dbPath);

// Ferme la base d'état. Sans effet si elle n'est pas ouverte.
void StateDB_Close(void);

// --- Suivi par langue / base --------------------------------------------

// Récupère le dernier updated_at connu pour une langue ("common", "fr",
// "en", "es", "pt"). Retourne 1 si une valeur était connue, 0 sinon
// (première exécution : rien de connu pour cette langue).
int StateDB_GetLangueUpdatedAt(const char* langue, char* outBuffer, size_t bufferSize);

// Enregistre (ou met à jour) le updated_at connu pour une langue.
// Retourne 1 en cas de succès, 0 sinon.
int StateDB_SetLangueUpdatedAt(const char* langue, const char* updatedAt);

// --- Suivi par prédication individuelle -----------------------------------

// Récupère le updated_at connu pour une prédication (langue + numéro).
// Retourne 1 si une valeur était connue, 0 sinon (jamais vue auparavant).
int StateDB_GetPredicationUpdatedAt(const char* langue, int number, char* outBuffer, size_t bufferSize);

// Enregistre (ou met à jour) le updated_at connu pour une prédication.
// Retourne 1 en cas de succès, 0 sinon.
int StateDB_SetPredicationUpdatedAt(const char* langue, int number, const char* updatedAt);

// --- Suivi des fichiers audio réellement téléchargés ------------------------
//
// scope : "fr", "en", "es", "pt" (prédications) ou "cantiques".
// item_id : numéro de la prédication, ou id du cantique.
// La valeur enregistrée est le updated_at du serveur correspondant au fichier
// présent sur le disque. Elle n'est écrite QU'APRÈS un téléchargement réussi.

// Retourne 1 si une version est connue, 0 sinon.
int StateDB_GetAudioUpdatedAt(const char* scope, int item_id, char* outBuffer, size_t bufferSize);

// Retourne 1 en cas de succès, 0 sinon.
int StateDB_SetAudioUpdatedAt(const char* scope, int item_id, const char* updatedAt);

#ifdef __cplusplus
}
#endif

#endif // STATE_DB_H
