// state_db.cpp - Implémentation de la base d'état locale (SQLite)

#include "state_db.h"
#include <sqlite3.h>
#include <cstring>
#include <cstdio>

static sqlite3* g_stateDb = nullptr;

static const char* SCHEMA_SQL =
    "CREATE TABLE IF NOT EXISTS langues_updates ("
    "   langue      TEXT PRIMARY KEY,"
    "   updated_at  TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS predications_updates ("
    "   langue      TEXT NOT NULL,"
    "   number      INTEGER NOT NULL,"
    "   updated_at  TEXT NOT NULL,"
    "   PRIMARY KEY (langue, number)"
    ");"
    "CREATE TABLE IF NOT EXISTS audio_state ("
    "   scope       TEXT NOT NULL,"
    "   item_id     INTEGER NOT NULL,"
    "   updated_at  TEXT NOT NULL,"
    "   PRIMARY KEY (scope, item_id)"
    ");";

int StateDB_Open(const char* dbPath) {
    if (g_stateDb) return 1; // déjà ouverte
    if (!dbPath) return 0;

    if (sqlite3_open_v2(dbPath, &g_stateDb,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        fprintf(stderr, "✗ Impossible d'ouvrir la base d'état : %s (%s)\n",
                dbPath, g_stateDb ? sqlite3_errmsg(g_stateDb) : "?");
        if (g_stateDb) { sqlite3_close(g_stateDb); g_stateDb = nullptr; }
        return 0;
    }

    char* errMsg = nullptr;
    if (sqlite3_exec(g_stateDb, SCHEMA_SQL, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "✗ Erreur création schéma base d'état : %s\n", errMsg ? errMsg : "?");
        sqlite3_free(errMsg);
        sqlite3_close(g_stateDb);
        g_stateDb = nullptr;
        return 0;
    }

    return 1;
}

void StateDB_Close(void) {
    if (g_stateDb) {
        sqlite3_close(g_stateDb);
        g_stateDb = nullptr;
    }
}

// --- Helpers internes -------------------------------------------------

static int get_by_langue(const char* sql, const char* langue,
                          char* outBuffer, size_t bufferSize) {
    if (!g_stateDb || !langue || !outBuffer || bufferSize == 0) return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_stateDb, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, langue, -1, SQLITE_STATIC);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* val = sqlite3_column_text(stmt, 0);
        if (val) {
            strncpy(outBuffer, (const char*)val, bufferSize - 1);
            outBuffer[bufferSize - 1] = '\0';
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

static int get_by_langue_number(const char* sql, const char* langue, int number,
                                 char* outBuffer, size_t bufferSize) {
    if (!g_stateDb || !langue || !outBuffer || bufferSize == 0) return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_stateDb, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, langue, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, number);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* val = sqlite3_column_text(stmt, 0);
        if (val) {
            strncpy(outBuffer, (const char*)val, bufferSize - 1);
            outBuffer[bufferSize - 1] = '\0';
            found = 1;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

// --- API publique -------------------------------------------------------

int StateDB_GetLangueUpdatedAt(const char* langue, char* outBuffer, size_t bufferSize) {
    return get_by_langue(
        "SELECT updated_at FROM langues_updates WHERE langue = ?;",
        langue, outBuffer, bufferSize);
}

int StateDB_SetLangueUpdatedAt(const char* langue, const char* updatedAt) {
    if (!g_stateDb || !langue || !updatedAt) return 0;

    const char* sql =
        "INSERT INTO langues_updates (langue, updated_at) VALUES (?, ?) "
        "ON CONFLICT(langue) DO UPDATE SET updated_at = excluded.updated_at;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_stateDb, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, langue, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, updatedAt, -1, SQLITE_STATIC);

    int ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int StateDB_GetPredicationUpdatedAt(const char* langue, int number, char* outBuffer, size_t bufferSize) {
    return get_by_langue_number(
        "SELECT updated_at FROM predications_updates WHERE langue = ? AND number = ?;",
        langue, number, outBuffer, bufferSize);
}

int StateDB_SetPredicationUpdatedAt(const char* langue, int number, const char* updatedAt) {
    if (!g_stateDb || !langue || !updatedAt) return 0;

    const char* sql =
        "INSERT INTO predications_updates (langue, number, updated_at) VALUES (?, ?, ?) "
        "ON CONFLICT(langue, number) DO UPDATE SET updated_at = excluded.updated_at;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_stateDb, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, langue, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, number);
    sqlite3_bind_text(stmt, 3, updatedAt, -1, SQLITE_STATIC);

    int ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

int StateDB_GetAudioUpdatedAt(const char* scope, int item_id, char* outBuffer, size_t bufferSize) {
    return get_by_langue_number(
        "SELECT updated_at FROM audio_state WHERE scope = ? AND item_id = ?;",
        scope, item_id, outBuffer, bufferSize);
}

int StateDB_SetAudioUpdatedAt(const char* scope, int item_id, const char* updatedAt) {
    if (!g_stateDb || !scope || !updatedAt) return 0;

    const char* sql =
        "INSERT INTO audio_state (scope, item_id, updated_at) VALUES (?, ?, ?) "
        "ON CONFLICT(scope, item_id) DO UPDATE SET updated_at = excluded.updated_at;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(g_stateDb, sql, -1, &stmt, nullptr) != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, scope, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, item_id);
    sqlite3_bind_text(stmt, 3, updatedAt, -1, SQLITE_STATIC);

    int ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
