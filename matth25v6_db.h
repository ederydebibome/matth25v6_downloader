#pragma once

#ifdef _WIN32
#  ifdef __cplusplus
#    ifdef MATTH25V6_DB_EXPORTS
#      define M25_API extern "C" __declspec(dllexport)
#    else
#      define M25_API extern "C" __declspec(dllimport)
#    endif
#  else
#    ifdef MATTH25V6_DB_EXPORTS
#      define M25_API __declspec(dllexport)
#    else
#      define M25_API __declspec(dllimport)
#    endif
#  endif
#else
#  ifdef __cplusplus
#    define M25_API extern "C" __attribute__((visibility("default")))
#  else
#    define M25_API __attribute__((visibility("default")))
#  endif
#endif

#include <stdint.h>

// ===========================================================================
// CODES DE RETOUR
// ===========================================================================

typedef enum {
    M25_OK          = 0,
    M25_ERROR       = 1,
    M25_NOT_FOUND   = 2,
    M25_INVALID_ARG = 3,
} M25Result;

// ===========================================================================
// STRUCTURES — COMMON.DB
// ===========================================================================

typedef struct {
    int     id;
    char    uuid[64];
    char    title[256];
    char    description[512];
    int     order;
    int     is_active;
    int     is_prayer;
    char    created_at[32];
    char    updated_at[32];
} M25Album;

typedef struct {
    int     id;
    int     brother_id;
    char    full_name[256];   // jointure brothers
    int     country_id;
    int     position;
    char    created_at[32];
    char    updated_at[32];
} M25Singer;

// Cantique enrichi : toutes les infos pour métadonnées MP3
typedef struct {
    int     id;
    char    title[256];
    char    audio_url[512];   // lien stream
    char    file_name[256];
    char    content[4096];
    int     time_seconds;
    int     order;
    int     is_active;
    // Album
    int     album_id;
    char    album_title[256];
    int     album_is_prayer;
    // Chanteur
    int     singer_id;
    char    singer_name[256];
    char    singer_phone[64];
    char    singer_avatar[512];
    char    singer_facebook[512];
    char    singer_youtube[512];
    char    created_at[32];
    char    updated_at[32];
} M25Sing;

typedef struct {
    int     id;
    char    full_name[256];
    char    phone[64];
    char    avatar[512];
    char    facebook[512];
    char    youtube[512];
    char    email[128];
    int     order;
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Brother;

typedef struct {
    int     id;
    char    name[256];
    int     city_id;
    char    city_name[128];   // jointure cities
    char    country_name[128];// jointure countries
    char    localisation[64];
    char    address[512];
    int     order;
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Assembly;

typedef struct {
    int     id;
    char    libelle[256];
    char    initial[32];
    int     country_id;
    char    country_name[128];
    int     principal;
    int     order;
    int     is_active;
    char    web_translation[16];
    int     is_african_language;
    int     display_in_sermon_page;
    char    other_initials[64];
    char    created_at[32];
    char    updated_at[32];
} M25Langue;

typedef struct {
    int     id;
    char    langue[32];
    char    updated_at[32];
    char    sync_status[32];
    char    sync_at[32];
    char    sync_by[64];
} M25LastUpdate;

typedef struct {
    int     id;
    int     num_pred;
    int     num_verset;
    char    concordance[512];
    char    created_at[32];
    char    updated_at[32];
} M25Concordance;

// ===========================================================================
// STRUCTURES — DB LANGUE (matth25v6_XX.db)
// ===========================================================================

typedef struct {
    int     id;
    int     number;
    char    title[512];
    char    chapter[64];
    char    sub_title[512];
    char    audio[512];
    char    cloud_audio[512];
    char    audio_name[256];
    char    video[512];
    char    time[32];
    char    cover[128];
    char    similar_sermon[512];
    char    publication_date[64];
    char    pdf[512];
    char    epub[512];
    char    legende[512];
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Sermon;

typedef struct {
    int     id;
    int     number;
    char    content[8192];
    char    info[512];
    char    link_at_content[512];
    char    url_content[512];
    int     sermon_id;
    int     sermon_number;  // jointure sermons
    char    title[256];
    char    verse_links[1024];
    char    created_at[32];
    char    updated_at[32];
} M25Verse;

typedef struct {
    int     id;
    char    url[512];
    char    title[256];
    char    description[1024];
    int     event_id;
    int     order;
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Photo;

typedef struct {
    int     id;
    char    url[512];
    char    title[256];
    char    description[1024];
    int     event_id;
    int     order;
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Video;

typedef struct {
    int     id;
    char    date[32];
    char    title[512];
    char    sub_title[512];
    char    description[4096];
    char    photo[512];
    int     order;
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Information;

typedef struct {
    int     id;
    char    libelle[512];
    int     order;
    int     is_active;
    int     country_id;
    char    created_at[32];
    char    updated_at[32];
} M25Travel;

typedef struct {
    int     id;
    char    title[512];
    char    video[512];
    char    photo[512];
    char    content[8192];
    int     order;
    int     is_active;
    int     country_id;
    char    created_at[32];
    char    updated_at[32];
} M25Testimony;

typedef struct {
    int     id;
    char    libelle[512];
    int     order;
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Event;

typedef struct {
    int     id;
    char    pdf[512];
    char    epub[512];
    char    name[256];
    char    extract_from[256];
    char    created_at[32];
    char    updated_at[32];
} M25Book;

typedef struct {
    int     id;
    char    photo[512];
    char    description[8192];
    int     is_active;
    char    created_at[32];
    char    updated_at[32];
} M25Biography;

// image_sermons : BLOB retourné via buffer alloué par l'appelant
typedef struct {
    int     id;
    char    name[256];
    char    description[512];
    char    link[512];
    int     apply_for_all_langue;
    char    created_at[32];
    char    updated_at[32];
    // Le BLOB est retourné séparément via ImageSermon_GetFile
} M25ImageSermonMeta;

// ===========================================================================
// COMMON.DB — ALBUMS & CANTIQUES
// ===========================================================================

// Tous les albums actifs
M25_API M25Result Album_GetAll(const char* commonDbPath,
                               M25Album*   outArray,
                               int         capacity,
                               int*        outCount);

// Un album par id
M25_API M25Result Album_GetById(const char* commonDbPath,
                                int         albumId,
                                M25Album*   out);

// Tous les cantiques enrichis (jointure sings + singers + brothers + albums)
// Idéal pour téléchargement complet avec métadonnées
M25_API M25Result Sing_GetAll(const char* commonDbPath,
                              M25Sing*    outArray,
                              int         capacity,
                              int*        outCount);

// Cantiques d'un album spécifique
M25_API M25Result Sing_GetByAlbum(const char* commonDbPath,
                                  int         albumId,
                                  M25Sing*    outArray,
                                  int         capacity,
                                  int*        outCount);

// Cantiques d'un chanteur spécifique
M25_API M25Result Sing_GetBySinger(const char* commonDbPath,
                                   int         singerId,
                                   M25Sing*    outArray,
                                   int         capacity,
                                   int*        outCount);

// Un cantique par id
M25_API M25Result Sing_GetById(const char* commonDbPath,
                               int         singId,
                               M25Sing*    out);

// Tous les chanteurs
M25_API M25Result Singer_GetAll(const char* commonDbPath,
                                M25Singer*  outArray,
                                int         capacity,
                                int*        outCount);

// ===========================================================================
// COMMON.DB — FRÈRES, ASSEMBLÉES, PAYS
// ===========================================================================

M25_API M25Result Brother_GetAll(const char* commonDbPath,
                                 M25Brother* outArray,
                                 int         capacity,
                                 int*        outCount);

M25_API M25Result Brother_GetById(const char* commonDbPath,
                                  int         brotherId,
                                  M25Brother* out);

M25_API M25Result Assembly_GetAll(const char* commonDbPath,
                                  M25Assembly* outArray,
                                  int          capacity,
                                  int*         outCount);

M25_API M25Result Assembly_GetByCountry(const char*  commonDbPath,
                                        int          countryId,
                                        M25Assembly* outArray,
                                        int          capacity,
                                        int*         outCount);

// ===========================================================================
// COMMON.DB — LANGUES & MISES À JOUR
// ===========================================================================

M25_API M25Result Langue_GetAll(const char* commonDbPath,
                                M25Langue*  outArray,
                                int         capacity,
                                int*        outCount);

M25_API M25Result Langue_GetActive(const char* commonDbPath,
                                   M25Langue*  outArray,
                                   int         capacity,
                                   int*        outCount);

M25_API M25Result LastUpdate_GetAll(const char*   commonDbPath,
                                    M25LastUpdate* outArray,
                                    int            capacity,
                                    int*           outCount);

M25_API M25Result LastUpdate_GetByLangue(const char*   commonDbPath,
                                         const char*   langue,
                                         M25LastUpdate* out);

// Met à jour le champ updated_at + sync_status d'une langue dans langue_last_updateds
M25_API M25Result LastUpdate_Set(const char* commonDbPath,
                                 const char* langue,
                                 const char* updatedAt,
                                 const char* syncStatus);

// ===========================================================================
// COMMON.DB — CONCORDANCES
// ===========================================================================

M25_API M25Result Concordance_GetBySermon(const char*    commonDbPath,
                                          int            numPred,
                                          M25Concordance* outArray,
                                          int            capacity,
                                          int*           outCount);

M25_API M25Result Concordance_GetByVerse(const char*    commonDbPath,
                                         int            numPred,
                                         int            numVerset,
                                         M25Concordance* out);

// ===========================================================================
// COMMON.DB — IMAGE SERMONS (BLOB)
// ===========================================================================

M25_API M25Result ImageSermon_GetMeta(const char*         commonDbPath,
                                      M25ImageSermonMeta* outArray,
                                      int                 capacity,
                                      int*                outCount);

// Retourne le BLOB d'une image sermon dans un buffer alloué par la DLL
// L'appelant doit libérer avec M25_Free
M25_API M25Result ImageSermon_GetFile(const char* commonDbPath,
                                      int         imageId,
                                      uint8_t**   outData,
                                      int*        outSize);

// ===========================================================================
// DB LANGUE — SERMONS
// ===========================================================================

M25_API M25Result Sermon_GetAll(const char* langDbPath,
                                M25Sermon*  outArray,
                                int         capacity,
                                int*        outCount);

M25_API M25Result Sermon_GetByNumber(const char* langDbPath,
                                     int         number,
                                     M25Sermon*  out);

M25_API M25Result Sermon_GetCount(const char* langDbPath,
                                  int*        outCount);

// Recherche full-text sur title + sub_title
M25_API M25Result Sermon_Search(const char* langDbPath,
                                const char* text,
                                M25Sermon*  outArray,
                                int         capacity,
                                int*        outCount);

// ===========================================================================
// DB LANGUE — VERSETS
// ===========================================================================

M25_API M25Result Verse_GetByRef(const char* langDbPath,
                                 int         sermonNumber,
                                 int         verseNumber,
                                 M25Verse*   out);

// Tous les versets d'un sermon
M25_API M25Result Verse_GetBySermon(const char* langDbPath,
                                    int         sermonNumber,
                                    M25Verse*   outArray,
                                    int         capacity,
                                    int*        outCount);

// Première occurrence d'un texte dans tous les versets (ORDER BY sermon ASC, verse ASC)
M25_API M25Result Verse_SearchFirst(const char* langDbPath,
                                    const char* text,
                                    M25Verse*   out);

// Toutes les occurrences d'un texte
M25_API M25Result Verse_SearchAll(const char* langDbPath,
                                  const char* text,
                                  M25Verse*   outArray,
                                  int         capacity,
                                  int*        outCount);

M25_API M25Result Verse_GetCount(const char* langDbPath,
                                 int         sermonNumber,
                                 int*        outCount);

// ===========================================================================
// DB LANGUE — PHOTOS, VIDÉOS, INFOS, VOYAGES, TÉMOIGNAGES
// ===========================================================================

M25_API M25Result Photo_GetAll(const char* langDbPath,
                               M25Photo*   outArray,
                               int         capacity,
                               int*        outCount);

M25_API M25Result Video_GetAll(const char* langDbPath,
                               M25Video*   outArray,
                               int         capacity,
                               int*        outCount);

M25_API M25Result Information_GetAll(const char*    langDbPath,
                                     M25Information* outArray,
                                     int             capacity,
                                     int*            outCount);

M25_API M25Result Travel_GetAll(const char* langDbPath,
                                M25Travel*  outArray,
                                int         capacity,
                                int*        outCount);

M25_API M25Result Testimony_GetAll(const char*  langDbPath,
                                   M25Testimony* outArray,
                                   int           capacity,
                                   int*          outCount);

M25_API M25Result Event_GetAll(const char* langDbPath,
                               M25Event*   outArray,
                               int         capacity,
                               int*        outCount);

M25_API M25Result Book_GetAll(const char* langDbPath,
                              M25Book*    outArray,
                              int         capacity,
                              int*        outCount);

M25_API M25Result Biography_Get(const char*   langDbPath,
                                M25Biography* out);

// Image sermons de la DB langue
M25_API M25Result LangImageSermon_GetMeta(const char*         langDbPath,
                                          M25ImageSermonMeta* outArray,
                                          int                 capacity,
                                          int*                outCount);

M25_API M25Result LangImageSermon_GetFile(const char* langDbPath,
                                          int         imageId,
                                          uint8_t**   outData,
                                          int*        outSize);

// ===========================================================================
// DB LANGUE — CONCORDANCES
// ===========================================================================

M25_API M25Result LangConcordance_GetBySermon(const char*    langDbPath,
                                              int            numPred,
                                              M25Concordance* outArray,
                                              int            capacity,
                                              int*           outCount);

M25_API M25Result LangConcordance_GetByVerse(const char*    langDbPath,
                                             int            numPred,
                                             int            numVerset,
                                             M25Concordance* out);

// ===========================================================================
// MÉMOIRE
// ===========================================================================

// Libère un buffer alloué par la DLL (ImageSermon_GetFile etc.)
M25_API void M25_Free(void* ptr);

