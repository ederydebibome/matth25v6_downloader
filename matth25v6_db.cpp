#ifndef MATTH25V6_DB_EXPORTS
#define MATTH25V6_DB_EXPORTS
#endif
#include "matth25v6_db.h"
#include <sqlite3.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>

// ===========================================================================
// HELPERS INTERNES
// ===========================================================================

// Copie tronquée UTF-8 : ne coupe jamais un caractère multi-octets en deux
// (strncpy pouvait laisser un octet orphelin => texte UTF-8 invalide).
static void sc(char* dst, int sz, const char* src) {
    if (!dst || sz <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n > (size_t)(sz - 1)) {
        n = (size_t)(sz - 1);
        // src[n] est le premier octet NON copié : si c'est un octet de
        // continuation (10xxxxxx), on recule jusqu'au début du caractère.
        while (n > 0 && (((unsigned char)src[n]) & 0xC0) == 0x80) --n;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int si(sqlite3_stmt* s, int col) {
    return sqlite3_column_int(s, col);
}

static const char* st(sqlite3_stmt* s, int col) {
    const char* v = (const char*)sqlite3_column_text(s, col);
    return v ? v : "";
}

static sqlite3* open_ro(const char* path) {
    if (!path || !path[0]) return nullptr;
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(path, &db,
        SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) { if (db) sqlite3_close(db); return nullptr; }
    sqlite3_busy_timeout(db, 3000);
    return db;
}

static sqlite3* open_rw(const char* path) {
    if (!path || !path[0]) return nullptr;
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(path, &db,
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) { if (db) sqlite3_close(db); return nullptr; }
    sqlite3_busy_timeout(db, 3000);
    return db;
}

static sqlite3_stmt* prep(sqlite3* db, const char* sql) {
    sqlite3_stmt* s = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) return nullptr;
    return s;
}

// ===========================================================================
// FILL HELPERS
// ===========================================================================

static void fill_album(sqlite3_stmt* s, M25Album* a) {
    a->id         = si(s, 0);
    sc(a->uuid,        sizeof(a->uuid),        st(s, 1));
    sc(a->title,       sizeof(a->title),       st(s, 2));
    sc(a->description, sizeof(a->description), st(s, 3));
    a->order      = si(s, 4);
    a->is_active  = si(s, 5);
    a->is_prayer  = si(s, 6);
    sc(a->created_at,  sizeof(a->created_at),  st(s, 7));
    sc(a->updated_at,  sizeof(a->updated_at),  st(s, 8));
}

static void fill_sing(sqlite3_stmt* s, M25Sing* g) {
    g->id             = si(s, 0);
    sc(g->title,        sizeof(g->title),        st(s, 1));
    sc(g->audio_url,    sizeof(g->audio_url),     st(s, 2));
    sc(g->file_name,    sizeof(g->file_name),     st(s, 3));
    sc(g->content,      sizeof(g->content),       st(s, 4));
    g->time_seconds   = si(s, 5);
    g->order          = si(s, 6);
    g->is_active      = si(s, 7);
    g->album_id       = si(s, 8);
    sc(g->album_title,  sizeof(g->album_title),   st(s, 9));
    g->album_is_prayer = si(s, 10);
    g->singer_id      = si(s, 11);
    sc(g->singer_name,  sizeof(g->singer_name),   st(s, 12));
    sc(g->singer_phone, sizeof(g->singer_phone),  st(s, 13));
    sc(g->singer_avatar,sizeof(g->singer_avatar), st(s, 14));
    sc(g->singer_facebook,sizeof(g->singer_facebook),st(s,15));
    sc(g->singer_youtube, sizeof(g->singer_youtube), st(s,16));
    sc(g->created_at,   sizeof(g->created_at),   st(s, 17));
    sc(g->updated_at,   sizeof(g->updated_at),   st(s, 18));
}

static const char* SQL_SING =
    "SELECT sg.id, sg.title, sg.audio, sg.file_name, sg.content, sg.time, "
    "       sg.order, sg.is_active, "
    "       al.id, al.title, al.is_prayer, "
    "       si.id, br.full_name, br.phone, br.avatar, br.facebook, br.youtube, "
    "       sg.created_at, sg.updated_at "
    "FROM sings sg "
    "LEFT JOIN albums  al ON al.id = sg.album_id "
    "LEFT JOIN singers si ON si.id = sg.singer_id "
    "LEFT JOIN brothers br ON br.id = si.brother_id ";

static void fill_singer(sqlite3_stmt* s, M25Singer* g) {
    g->id         = si(s, 0);
    g->brother_id = si(s, 1);
    sc(g->full_name, sizeof(g->full_name), st(s, 2));
    g->country_id = si(s, 3);
    g->position   = si(s, 4);
    sc(g->created_at, sizeof(g->created_at), st(s, 5));
    sc(g->updated_at, sizeof(g->updated_at), st(s, 6));
}

static void fill_brother(sqlite3_stmt* s, M25Brother* b) {
    b->id        = si(s, 0);
    sc(b->full_name, sizeof(b->full_name), st(s, 1));
    sc(b->phone,     sizeof(b->phone),     st(s, 2));
    sc(b->avatar,    sizeof(b->avatar),    st(s, 3));
    sc(b->facebook,  sizeof(b->facebook),  st(s, 4));
    sc(b->youtube,   sizeof(b->youtube),   st(s, 5));
    sc(b->email,     sizeof(b->email),     st(s, 6));
    b->order     = si(s, 7);
    b->is_active = si(s, 8);
    sc(b->created_at, sizeof(b->created_at), st(s, 9));
    sc(b->updated_at, sizeof(b->updated_at), st(s, 10));
}

static void fill_assembly(sqlite3_stmt* s, M25Assembly* a) {
    a->id        = si(s, 0);
    sc(a->name,         sizeof(a->name),         st(s, 1));
    a->city_id   = si(s, 2);
    sc(a->city_name,    sizeof(a->city_name),     st(s, 3));
    sc(a->country_name, sizeof(a->country_name),  st(s, 4));
    sc(a->localisation, sizeof(a->localisation),  st(s, 5));
    sc(a->address,      sizeof(a->address),       st(s, 6));
    a->order     = si(s, 7);
    a->is_active = si(s, 8);
    sc(a->created_at,   sizeof(a->created_at),    st(s, 9));
    sc(a->updated_at,   sizeof(a->updated_at),    st(s, 10));
}

static void fill_langue(sqlite3_stmt* s, M25Langue* l) {
    l->id                     = si(s, 0);
    sc(l->libelle,              sizeof(l->libelle),              st(s, 1));
    sc(l->initial,              sizeof(l->initial),              st(s, 2));
    l->country_id             = si(s, 3);
    sc(l->country_name,         sizeof(l->country_name),         st(s, 4));
    l->principal              = si(s, 5);
    l->order                  = si(s, 6);
    l->is_active              = si(s, 7);
    sc(l->web_translation,      sizeof(l->web_translation),      st(s, 8));
    l->is_african_language    = si(s, 9);
    l->display_in_sermon_page = si(s, 10);
    sc(l->other_initials,       sizeof(l->other_initials),       st(s, 11));
    sc(l->created_at,           sizeof(l->created_at),           st(s, 12));
    sc(l->updated_at,           sizeof(l->updated_at),           st(s, 13));
}

static void fill_lastupdate(sqlite3_stmt* s, M25LastUpdate* u) {
    u->id = si(s, 0);
    sc(u->langue,      sizeof(u->langue),      st(s, 1));
    sc(u->updated_at,  sizeof(u->updated_at),  st(s, 2));
    sc(u->sync_status, sizeof(u->sync_status), st(s, 3));
    sc(u->sync_at,     sizeof(u->sync_at),     st(s, 4));
    sc(u->sync_by,     sizeof(u->sync_by),     st(s, 5));
}

static void fill_concordance(sqlite3_stmt* s, M25Concordance* c) {
    c->id          = si(s, 0);
    c->num_pred    = si(s, 1);
    c->num_verset  = si(s, 2);
    sc(c->concordance, sizeof(c->concordance), st(s, 3));
    sc(c->created_at,  sizeof(c->created_at),  st(s, 4));
    sc(c->updated_at,  sizeof(c->updated_at),  st(s, 5));
}

static void fill_sermon(sqlite3_stmt* s, M25Sermon* m) {
    m->id       = si(s, 0);
    m->number   = si(s, 1);
    sc(m->title,           sizeof(m->title),           st(s, 2));
    sc(m->chapter,         sizeof(m->chapter),         st(s, 3));
    sc(m->sub_title,       sizeof(m->sub_title),       st(s, 4));
    sc(m->audio,           sizeof(m->audio),           st(s, 5));
    sc(m->cloud_audio,     sizeof(m->cloud_audio),     st(s, 6));
    sc(m->audio_name,      sizeof(m->audio_name),      st(s, 7));
    sc(m->video,           sizeof(m->video),           st(s, 8));
    sc(m->time,            sizeof(m->time),            st(s, 9));
    sc(m->cover,           sizeof(m->cover),           st(s, 10));
    sc(m->similar_sermon,  sizeof(m->similar_sermon),  st(s, 11));
    sc(m->publication_date,sizeof(m->publication_date),st(s, 12));
    sc(m->pdf,             sizeof(m->pdf),             st(s, 13));
    sc(m->epub,            sizeof(m->epub),            st(s, 14));
    sc(m->legende,         sizeof(m->legende),         st(s, 15));
    m->is_active = si(s, 16);
    sc(m->created_at,      sizeof(m->created_at),      st(s, 17));
    sc(m->updated_at,      sizeof(m->updated_at),      st(s, 18));
}

static void fill_verse(sqlite3_stmt* s, M25Verse* v) {
    v->id           = si(s, 0);
    v->number       = si(s, 1);
    sc(v->content,        sizeof(v->content),        st(s, 2));
    sc(v->info,           sizeof(v->info),           st(s, 3));
    sc(v->link_at_content,sizeof(v->link_at_content),st(s, 4));
    sc(v->url_content,    sizeof(v->url_content),    st(s, 5));
    v->sermon_id    = si(s, 6);
    v->sermon_number= si(s, 7);
    sc(v->title,          sizeof(v->title),          st(s, 8));
    sc(v->verse_links,    sizeof(v->verse_links),    st(s, 9));
    sc(v->created_at,     sizeof(v->created_at),     st(s, 10));
    sc(v->updated_at,     sizeof(v->updated_at),     st(s, 11));
}

static void fill_photo(sqlite3_stmt* s, M25Photo* p) {
    p->id        = si(s, 0);
    sc(p->url,         sizeof(p->url),         st(s, 1));
    sc(p->title,       sizeof(p->title),       st(s, 2));
    sc(p->description, sizeof(p->description), st(s, 3));
    p->event_id  = si(s, 4);
    p->order     = si(s, 5);
    p->is_active = si(s, 6);
    sc(p->created_at,  sizeof(p->created_at),  st(s, 7));
    sc(p->updated_at,  sizeof(p->updated_at),  st(s, 8));
}

static void fill_video(sqlite3_stmt* s, M25Video* v) {
    v->id        = si(s, 0);
    sc(v->url,         sizeof(v->url),         st(s, 1));
    sc(v->title,       sizeof(v->title),       st(s, 2));
    sc(v->description, sizeof(v->description), st(s, 3));
    v->event_id  = si(s, 4);
    v->order     = si(s, 5);
    v->is_active = si(s, 6);
    sc(v->created_at,  sizeof(v->created_at),  st(s, 7));
    sc(v->updated_at,  sizeof(v->updated_at),  st(s, 8));
}

static void fill_information(sqlite3_stmt* s, M25Information* i) {
    i->id        = si(s, 0);
    sc(i->date,        sizeof(i->date),        st(s, 1));
    sc(i->title,       sizeof(i->title),       st(s, 2));
    sc(i->sub_title,   sizeof(i->sub_title),   st(s, 3));
    sc(i->description, sizeof(i->description), st(s, 4));
    sc(i->photo,       sizeof(i->photo),       st(s, 5));
    i->order     = si(s, 6);
    i->is_active = si(s, 7);
    sc(i->created_at,  sizeof(i->created_at),  st(s, 8));
    sc(i->updated_at,  sizeof(i->updated_at),  st(s, 9));
}

static void fill_travel(sqlite3_stmt* s, M25Travel* t) {
    t->id         = si(s, 0);
    sc(t->libelle,    sizeof(t->libelle),    st(s, 1));
    t->order      = si(s, 2);
    t->is_active  = si(s, 3);
    t->country_id = si(s, 4);
    sc(t->created_at, sizeof(t->created_at), st(s, 5));
    sc(t->updated_at, sizeof(t->updated_at), st(s, 6));
}

static void fill_testimony(sqlite3_stmt* s, M25Testimony* t) {
    t->id         = si(s, 0);
    sc(t->title,      sizeof(t->title),      st(s, 1));
    sc(t->video,      sizeof(t->video),      st(s, 2));
    sc(t->photo,      sizeof(t->photo),      st(s, 3));
    sc(t->content,    sizeof(t->content),    st(s, 4));
    t->order      = si(s, 5);
    t->is_active  = si(s, 6);
    t->country_id = si(s, 7);
    sc(t->created_at, sizeof(t->created_at), st(s, 8));
    sc(t->updated_at, sizeof(t->updated_at), st(s, 9));
}

static void fill_event(sqlite3_stmt* s, M25Event* e) {
    e->id        = si(s, 0);
    sc(e->libelle,    sizeof(e->libelle),    st(s, 1));
    e->order     = si(s, 2);
    e->is_active = si(s, 3);
    sc(e->created_at, sizeof(e->created_at), st(s, 4));
    sc(e->updated_at, sizeof(e->updated_at), st(s, 5));
}

static void fill_book(sqlite3_stmt* s, M25Book* b) {
    b->id = si(s, 0);
    sc(b->pdf,          sizeof(b->pdf),          st(s, 1));
    sc(b->epub,         sizeof(b->epub),         st(s, 2));
    sc(b->name,         sizeof(b->name),         st(s, 3));
    sc(b->extract_from, sizeof(b->extract_from), st(s, 4));
    sc(b->created_at,   sizeof(b->created_at),   st(s, 5));
    sc(b->updated_at,   sizeof(b->updated_at),   st(s, 6));
}

static void fill_imagesermon_meta(sqlite3_stmt* s, M25ImageSermonMeta* m) {
    m->id = si(s, 0);
    sc(m->name,               sizeof(m->name),               st(s, 1));
    sc(m->description,        sizeof(m->description),        st(s, 2));
    sc(m->link,               sizeof(m->link),               st(s, 3));
    m->apply_for_all_langue = si(s, 4);
    sc(m->created_at,         sizeof(m->created_at),         st(s, 5));
    sc(m->updated_at,         sizeof(m->updated_at),         st(s, 6));
}

// Macro pour les fonctions "GetAll" génériques
#define GETALL_IMPL(fn_name, db_path, sql_str, fill_fn, out_type)   \
M25_API M25Result fn_name(const char* db_path,                       \
                           out_type*  outArray,                       \
                           int        capacity,                       \
                           int*       outCount) {                     \
    if (!db_path || !outArray || capacity <= 0 || !outCount)         \
        return M25_INVALID_ARG;                                       \
    *outCount = 0;                                                    \
    sqlite3* db = open_ro(db_path);                                  \
    if (!db) return M25_ERROR;                                        \
    sqlite3_stmt* s = prep(db, sql_str);                             \
    if (!s) { sqlite3_close(db); return M25_ERROR; }                 \
    int n = 0;                                                        \
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)            \
        fill_fn(s, &outArray[n++]);                                   \
    *outCount = n;                                                    \
    sqlite3_finalize(s);                                              \
    sqlite3_close(db);                                                \
    return M25_OK;                                                    \
}

// ===========================================================================
// ALBUMS
// ===========================================================================

GETALL_IMPL(Album_GetAll, commonDbPath,
    "SELECT id,uuid,title,description,\"order\",is_active,is_prayer,created_at,updated_at "
    "FROM albums WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_album, M25Album)

M25_API M25Result Album_GetById(const char* commonDbPath, int albumId, M25Album* out) {
    if (!commonDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,uuid,title,description,\"order\",is_active,is_prayer,created_at,updated_at "
        "FROM albums WHERE id=?");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, albumId);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_album(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

// ===========================================================================
// CANTIQUES (SINGS)
// ===========================================================================

static const char* SQL_SING_FULL =
    "SELECT sg.id, sg.title, sg.audio, sg.file_name, sg.content, sg.time, "
    "       sg.\"order\", sg.is_active, "
    "       al.id, al.title, al.is_prayer, "
    "       si.id, br.full_name, br.phone, br.avatar, br.facebook, br.youtube, "
    "       sg.created_at, sg.updated_at "
    "FROM sings sg "
    "LEFT JOIN albums   al ON al.id = sg.album_id "
    "LEFT JOIN singers  si ON si.id = sg.singer_id "
    "LEFT JOIN brothers br ON br.id = si.brother_id "
    "WHERE sg.is_active=1 ORDER BY al.\"order\" ASC, sg.\"order\" ASC";

GETALL_IMPL(Sing_GetAll, commonDbPath, SQL_SING_FULL, fill_sing, M25Sing)

M25_API M25Result Sing_GetByAlbum(const char* commonDbPath, int albumId,
                                   M25Sing* outArray, int capacity, int* outCount) {
    if (!commonDbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT sg.id, sg.title, sg.audio, sg.file_name, sg.content, sg.time, "
        "       sg.\"order\", sg.is_active, "
        "       al.id, al.title, al.is_prayer, "
        "       si.id, br.full_name, br.phone, br.avatar, br.facebook, br.youtube, "
        "       sg.created_at, sg.updated_at "
        "FROM sings sg "
        "LEFT JOIN albums   al ON al.id = sg.album_id "
        "LEFT JOIN singers  si ON si.id = sg.singer_id "
        "LEFT JOIN brothers br ON br.id = si.brother_id "
        "WHERE sg.album_id=? AND sg.is_active=1 ORDER BY sg.\"order\" ASC");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, albumId);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_sing(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

M25_API M25Result Sing_GetBySinger(const char* commonDbPath, int singerId,
                                    M25Sing* outArray, int capacity, int* outCount) {
    if (!commonDbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT sg.id, sg.title, sg.audio, sg.file_name, sg.content, sg.time, "
        "       sg.\"order\", sg.is_active, "
        "       al.id, al.title, al.is_prayer, "
        "       si.id, br.full_name, br.phone, br.avatar, br.facebook, br.youtube, "
        "       sg.created_at, sg.updated_at "
        "FROM sings sg "
        "LEFT JOIN albums   al ON al.id = sg.album_id "
        "LEFT JOIN singers  si ON si.id = sg.singer_id "
        "LEFT JOIN brothers br ON br.id = si.brother_id "
        "WHERE sg.singer_id=? AND sg.is_active=1 ORDER BY sg.\"order\" ASC");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, singerId);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_sing(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

M25_API M25Result Sing_GetById(const char* commonDbPath, int singId, M25Sing* out) {
    if (!commonDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT sg.id, sg.title, sg.audio, sg.file_name, sg.content, sg.time, "
        "       sg.\"order\", sg.is_active, "
        "       al.id, al.title, al.is_prayer, "
        "       si.id, br.full_name, br.phone, br.avatar, br.facebook, br.youtube, "
        "       sg.created_at, sg.updated_at "
        "FROM sings sg "
        "LEFT JOIN albums   al ON al.id = sg.album_id "
        "LEFT JOIN singers  si ON si.id = sg.singer_id "
        "LEFT JOIN brothers br ON br.id = si.brother_id "
        "WHERE sg.id=?");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, singId);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_sing(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

GETALL_IMPL(Singer_GetAll, commonDbPath,
    "SELECT si.id, si.brother_id, br.full_name, si.country_id, si.position, "
    "       si.created_at, si.updated_at "
    "FROM singers si LEFT JOIN brothers br ON br.id = si.brother_id "
    "ORDER BY si.position ASC",
    fill_singer, M25Singer)

// ===========================================================================
// FRÈRES & ASSEMBLÉES
// ===========================================================================

GETALL_IMPL(Brother_GetAll, commonDbPath,
    "SELECT id,full_name,phone,avatar,facebook,youtube,email,\"order\",is_active,created_at,updated_at "
    "FROM brothers WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_brother, M25Brother)

M25_API M25Result Brother_GetById(const char* commonDbPath, int brotherId, M25Brother* out) {
    if (!commonDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,full_name,phone,avatar,facebook,youtube,email,\"order\",is_active,created_at,updated_at "
        "FROM brothers WHERE id=?");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, brotherId);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_brother(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

GETALL_IMPL(Assembly_GetAll, commonDbPath,
    "SELECT a.id, a.name, a.city_id, c.libelle, co.name, a.localisation, "
    "       a.address, a.\"order\", a.is_active, a.created_at, a.updated_at "
    "FROM assemblies a "
    "LEFT JOIN cities c ON c.id = a.city_id "
    "LEFT JOIN countries co ON co.id = c.country_id "
    "WHERE a.is_active=1 ORDER BY a.\"order\" ASC",
    fill_assembly, M25Assembly)

M25_API M25Result Assembly_GetByCountry(const char* commonDbPath, int countryId,
                                         M25Assembly* outArray, int capacity, int* outCount) {
    if (!commonDbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT a.id, a.name, a.city_id, c.libelle, co.name, a.localisation, "
        "       a.address, a.\"order\", a.is_active, a.created_at, a.updated_at "
        "FROM assemblies a "
        "LEFT JOIN cities c ON c.id = a.city_id "
        "LEFT JOIN countries co ON co.id = c.country_id "
        "WHERE co.id=? AND a.is_active=1 ORDER BY a.\"order\" ASC");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, countryId);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_assembly(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

// ===========================================================================
// LANGUES & MISES À JOUR
// ===========================================================================

GETALL_IMPL(Langue_GetAll, commonDbPath,
    "SELECT l.id, l.libelle, l.initial, l.country_id, c.name, l.principal, "
    "       l.\"order\", l.is_active, l.web_translation, l.is_african_language, "
    "       l.display_in_sermon_page, l.other_initials, l.created_at, l.updated_at "
    "FROM langues l LEFT JOIN countries c ON c.id = l.country_id "
    "ORDER BY l.\"order\" ASC",
    fill_langue, M25Langue)

GETALL_IMPL(Langue_GetActive, commonDbPath,
    "SELECT l.id, l.libelle, l.initial, l.country_id, c.name, l.principal, "
    "       l.\"order\", l.is_active, l.web_translation, l.is_african_language, "
    "       l.display_in_sermon_page, l.other_initials, l.created_at, l.updated_at "
    "FROM langues l LEFT JOIN countries c ON c.id = l.country_id "
    "WHERE l.is_active=1 ORDER BY l.\"order\" ASC",
    fill_langue, M25Langue)

GETALL_IMPL(LastUpdate_GetAll, commonDbPath,
    "SELECT id,langue,updated_at,sync_status,sync_at,sync_by FROM langue_last_updateds",
    fill_lastupdate, M25LastUpdate)

M25_API M25Result LastUpdate_GetByLangue(const char* commonDbPath,
                                          const char* langue, M25LastUpdate* out) {
    if (!commonDbPath || !langue || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,langue,updated_at,sync_status,sync_at,sync_by "
        "FROM langue_last_updateds WHERE langue=? LIMIT 1");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_text(s, 1, langue, -1, SQLITE_STATIC);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_lastupdate(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

M25_API M25Result LastUpdate_Set(const char* commonDbPath,
                                  const char* langue,
                                  const char* updatedAt,
                                  const char* syncStatus) {
    if (!commonDbPath || !langue || !updatedAt || !syncStatus) return M25_INVALID_ARG;
    sqlite3* db = open_rw(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "UPDATE langue_last_updateds SET updated_at=?, sync_status=? WHERE langue=?");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_text(s, 1, updatedAt,  -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 2, syncStatus, -1, SQLITE_STATIC);
    sqlite3_bind_text(s, 3, langue,     -1, SQLITE_STATIC);
    M25Result res = (sqlite3_step(s) == SQLITE_DONE) ? M25_OK : M25_ERROR;
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

// ===========================================================================
// CONCORDANCES (COMMON)
// ===========================================================================

M25_API M25Result Concordance_GetBySermon(const char* commonDbPath, int numPred,
                                           M25Concordance* outArray, int capacity, int* outCount) {
    if (!commonDbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,num_pred,num_verset,concordance,created_at,updated_at "
        "FROM concordances WHERE num_pred=? ORDER BY num_verset ASC");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, numPred);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_concordance(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

M25_API M25Result Concordance_GetByVerse(const char* commonDbPath, int numPred, int numVerset,
                                          M25Concordance* out) {
    if (!commonDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(commonDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,num_pred,num_verset,concordance,created_at,updated_at "
        "FROM concordances WHERE num_pred=? AND num_verset=? LIMIT 1");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, numPred);
    sqlite3_bind_int(s, 2, numVerset);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_concordance(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

// ===========================================================================
// IMAGE SERMONS — BLOB
// ===========================================================================

static M25Result imagesermon_getmeta_impl(const char* dbPath, M25ImageSermonMeta* outArray,
                                           int capacity, int* outCount) {
    if (!dbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(dbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,name,description,link,apply_for_all_langue,created_at,updated_at "
        "FROM image_sermons ORDER BY id ASC");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_imagesermon_meta(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

static M25Result imagesermon_getfile_impl(const char* dbPath, int imageId,
                                           uint8_t** outData, int* outSize) {
    if (!dbPath || !outData || !outSize) return M25_INVALID_ARG;
    *outData = nullptr; *outSize = 0;
    sqlite3* db = open_ro(dbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db, "SELECT file FROM image_sermons WHERE id=?");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, imageId);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) {
        int sz = sqlite3_column_bytes(s, 0);
        const void* blob = sqlite3_column_blob(s, 0);
        if (blob && sz > 0) {
            *outData = (uint8_t*)malloc(sz);
            if (*outData) { memcpy(*outData, blob, sz); *outSize = sz; res = M25_OK; }
            else res = M25_ERROR;
        }
    }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

M25_API M25Result ImageSermon_GetMeta(const char* commonDbPath, M25ImageSermonMeta* outArray,
                                       int capacity, int* outCount) {
    return imagesermon_getmeta_impl(commonDbPath, outArray, capacity, outCount);
}
M25_API M25Result ImageSermon_GetFile(const char* commonDbPath, int imageId,
                                       uint8_t** outData, int* outSize) {
    return imagesermon_getfile_impl(commonDbPath, imageId, outData, outSize);
}
M25_API M25Result LangImageSermon_GetMeta(const char* langDbPath, M25ImageSermonMeta* outArray,
                                           int capacity, int* outCount) {
    return imagesermon_getmeta_impl(langDbPath, outArray, capacity, outCount);
}
M25_API M25Result LangImageSermon_GetFile(const char* langDbPath, int imageId,
                                           uint8_t** outData, int* outSize) {
    return imagesermon_getfile_impl(langDbPath, imageId, outData, outSize);
}

// ===========================================================================
// DB LANGUE — SERMONS
// ===========================================================================

GETALL_IMPL(Sermon_GetAll, langDbPath,
    "SELECT id,number,title,chapter,sub_title,audio,cloud_audio,audio_name,"
    "       video,time,cover,similar_sermon,publication_date,pdf,epub,legende,"
    "       is_active,created_at,updated_at "
    "FROM sermons WHERE is_active=1 ORDER BY number ASC",
    fill_sermon, M25Sermon)

M25_API M25Result Sermon_GetByNumber(const char* langDbPath, int number, M25Sermon* out) {
    if (!langDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,number,title,chapter,sub_title,audio,cloud_audio,audio_name,"
        "       video,time,cover,similar_sermon,publication_date,pdf,epub,legende,"
        "       is_active,created_at,updated_at "
        "FROM sermons WHERE number=? LIMIT 1");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, number);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_sermon(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

M25_API M25Result Sermon_GetCount(const char* langDbPath, int* outCount) {
    if (!langDbPath || !outCount) return M25_INVALID_ARG;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db, "SELECT COUNT(*) FROM sermons WHERE is_active=1");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    *outCount = 0;
    if (sqlite3_step(s) == SQLITE_ROW) *outCount = si(s, 0);
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

M25_API M25Result Sermon_Search(const char* langDbPath, const char* text,
                                 M25Sermon* outArray, int capacity, int* outCount) {
    if (!langDbPath || !text || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    size_t tlen = strlen(text);
    char* pat = (char*)malloc(tlen + 3);
    if (!pat) return M25_ERROR;
    pat[0] = '%'; memcpy(pat+1, text, tlen); pat[tlen+1] = '%'; pat[tlen+2] = '\0';
    sqlite3* db = open_ro(langDbPath);
    if (!db) { free(pat); return M25_ERROR; }
    sqlite3_stmt* s = prep(db,
        "SELECT id,number,title,chapter,sub_title,audio,cloud_audio,audio_name,"
        "       video,time,cover,similar_sermon,publication_date,pdf,epub,legende,"
        "       is_active,created_at,updated_at "
        "FROM sermons WHERE (title LIKE ? OR sub_title LIKE ?) AND is_active=1 "
        "ORDER BY number ASC");
    if (!s) { free(pat); sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_text(s, 1, pat, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, pat, -1, SQLITE_TRANSIENT);
    free(pat);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_sermon(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

// ===========================================================================
// DB LANGUE — VERSETS
// ===========================================================================

static const char* SQL_VERSE =
    "SELECT v.id, v.number, v.content, v.info, v.link_at_content, v.url_content, "
    "       v.sermon_id, s.number, v.title, v.verse_links, v.created_at, v.updated_at "
    "FROM verses v INNER JOIN sermons s ON s.id = v.sermon_id ";

M25_API M25Result Verse_GetByRef(const char* langDbPath, int sermonNumber, int verseNumber,
                                  M25Verse* out) {
    if (!langDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    char sql[512];
    snprintf(sql, sizeof(sql), "%s WHERE s.number=? AND v.number=? LIMIT 1", SQL_VERSE);
    sqlite3_stmt* s = prep(db, sql);
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, sermonNumber);
    sqlite3_bind_int(s, 2, verseNumber);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_verse(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

M25_API M25Result Verse_GetBySermon(const char* langDbPath, int sermonNumber,
                                     M25Verse* outArray, int capacity, int* outCount) {
    if (!langDbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    char sql[512];
    snprintf(sql, sizeof(sql),
        "%s WHERE s.number=? ORDER BY v.number ASC", SQL_VERSE);
    sqlite3_stmt* s = prep(db, sql);
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, sermonNumber);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_verse(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

static M25Result verse_search_impl(const char* langDbPath, const char* text,
                                    M25Verse* outArray, int capacity, int* outCount,
                                    bool firstOnly) {
    if (!langDbPath || !text || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    size_t tlen = strlen(text);
    char* pat = (char*)malloc(tlen + 3);
    if (!pat) return M25_ERROR;
    pat[0] = '%'; memcpy(pat+1, text, tlen); pat[tlen+1] = '%'; pat[tlen+2] = '\0';
    sqlite3* db = open_ro(langDbPath);
    if (!db) { free(pat); return M25_ERROR; }
    char sql[640];
    snprintf(sql, sizeof(sql),
        "%s WHERE v.content LIKE ? "
        "ORDER BY s.number ASC, v.number ASC%s",
        SQL_VERSE, firstOnly ? " LIMIT 1" : "");
    sqlite3_stmt* s = prep(db, sql);
    if (!s) { free(pat); sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_text(s, 1, pat, -1, SQLITE_TRANSIENT);
    free(pat);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_verse(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return (*outCount > 0) ? M25_OK : M25_NOT_FOUND;
}

M25_API M25Result Verse_SearchFirst(const char* langDbPath, const char* text, M25Verse* out) {
    int count = 0;
    return verse_search_impl(langDbPath, text, out, 1, &count, true);
}

M25_API M25Result Verse_SearchAll(const char* langDbPath, const char* text,
                                   M25Verse* outArray, int capacity, int* outCount) {
    return verse_search_impl(langDbPath, text, outArray, capacity, outCount, false);
}

M25_API M25Result Verse_GetCount(const char* langDbPath, int sermonNumber, int* outCount) {
    if (!langDbPath || !outCount) return M25_INVALID_ARG;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT COUNT(*) FROM verses v INNER JOIN sermons sr ON sr.id=v.sermon_id "
        "WHERE sr.number=?");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, sermonNumber);
    *outCount = 0;
    if (sqlite3_step(s) == SQLITE_ROW) *outCount = si(s, 0);
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

// ===========================================================================
// DB LANGUE — PHOTOS, VIDÉOS, INFOS, VOYAGES, TÉMOIGNAGES, EVENTS, BOOKS
// ===========================================================================

GETALL_IMPL(Photo_GetAll, langDbPath,
    "SELECT id,url,title,description,event_id,\"order\",is_active,created_at,updated_at "
    "FROM photos WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_photo, M25Photo)

GETALL_IMPL(Video_GetAll, langDbPath,
    "SELECT id,url,title,description,event_id,\"order\",is_active,created_at,updated_at "
    "FROM videos WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_video, M25Video)

GETALL_IMPL(Information_GetAll, langDbPath,
    "SELECT id,date,title,sub_title,description,photo,\"order\",is_active,created_at,updated_at "
    "FROM informations WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_information, M25Information)

GETALL_IMPL(Travel_GetAll, langDbPath,
    "SELECT id,libelle,\"order\",is_active,country_id,created_at,updated_at "
    "FROM travels WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_travel, M25Travel)

GETALL_IMPL(Testimony_GetAll, langDbPath,
    "SELECT id,title,video,photo,content,\"order\",is_active,country_id,created_at,updated_at "
    "FROM testimonies WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_testimony, M25Testimony)

GETALL_IMPL(Event_GetAll, langDbPath,
    "SELECT id,libelle,\"order\",is_active,created_at,updated_at "
    "FROM events WHERE is_active=1 ORDER BY \"order\" ASC",
    fill_event, M25Event)

GETALL_IMPL(Book_GetAll, langDbPath,
    "SELECT id,pdf,epub,name,extract_from,created_at,updated_at FROM books",
    fill_book, M25Book)

M25_API M25Result Biography_Get(const char* langDbPath, M25Biography* out) {
    if (!langDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,photo,description,is_active,created_at,updated_at "
        "FROM biographies WHERE is_active=1 LIMIT 1");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) {
        out->id        = si(s, 0);
        sc(out->photo,       sizeof(out->photo),       st(s, 1));
        sc(out->description, sizeof(out->description), st(s, 2));
        out->is_active = si(s, 3);
        sc(out->created_at,  sizeof(out->created_at),  st(s, 4));
        sc(out->updated_at,  sizeof(out->updated_at),  st(s, 5));
        res = M25_OK;
    }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

// ===========================================================================
// CONCORDANCES (DB LANGUE)
// ===========================================================================

M25_API M25Result LangConcordance_GetBySermon(const char* langDbPath, int numPred,
                                               M25Concordance* outArray, int capacity, int* outCount) {
    if (!langDbPath || !outArray || capacity <= 0 || !outCount) return M25_INVALID_ARG;
    *outCount = 0;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,num_pred,num_verset,concordance,created_at,updated_at "
        "FROM concordances WHERE num_pred=? ORDER BY num_verset ASC");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, numPred);
    int n = 0;
    while (sqlite3_step(s) == SQLITE_ROW && n < capacity)
        fill_concordance(s, &outArray[n++]);
    *outCount = n;
    sqlite3_finalize(s); sqlite3_close(db);
    return M25_OK;
}

M25_API M25Result LangConcordance_GetByVerse(const char* langDbPath, int numPred, int numVerset,
                                              M25Concordance* out) {
    if (!langDbPath || !out) return M25_INVALID_ARG;
    sqlite3* db = open_ro(langDbPath);
    if (!db) return M25_ERROR;
    sqlite3_stmt* s = prep(db,
        "SELECT id,num_pred,num_verset,concordance,created_at,updated_at "
        "FROM concordances WHERE num_pred=? AND num_verset=? LIMIT 1");
    if (!s) { sqlite3_close(db); return M25_ERROR; }
    sqlite3_bind_int(s, 1, numPred);
    sqlite3_bind_int(s, 2, numVerset);
    M25Result res = M25_NOT_FOUND;
    if (sqlite3_step(s) == SQLITE_ROW) { fill_concordance(s, out); res = M25_OK; }
    sqlite3_finalize(s); sqlite3_close(db);
    return res;
}

// ===========================================================================
// MÉMOIRE
// ===========================================================================

M25_API void M25_Free(void* ptr) {
    free(ptr);
}
