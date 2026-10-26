#pragma once

#include <windows.h>
#include "img_header.h"
#include "net_header.h"
#include "metadata_header.h"
#include "matth25v6_db.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*create_image_t)(
    const char* prefix,
    const char* number,
    const char* title,
    const char* fontName,
    double      fontSize,
    const char* color,
    const char* templatePath,
    const char* outputPath,
    double      margin,
    double      yStart
);

typedef int (*download_file_t)(const char* url, const char* outputPath);
typedef int (*set_tags_t)(const char* audioPath, const TagPair* tags, int count);
typedef int (*set_cover_image_t)(const char* audioPath, const char* imagePath);
typedef M25Result (*sermon_get_count_t)(const char* langDbPath, int* outCount);
typedef M25Result (*sermon_get_all_t)(const char* langDbPath, M25Sermon* outArray, int capacity, int* outCount);
typedef M25Result (*sing_get_all_t)(const char* commonDbPath, M25Sing* outArray, int capacity, int* outCount);

typedef struct {
    const char* name;
    HMODULE value;
} lib;

#ifdef __cplusplus
}
#endif
