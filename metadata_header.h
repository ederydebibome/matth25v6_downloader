#ifndef METADATA_API_H
#define METADATA_API_H

#if defined(_WIN32) || defined(_WIN64)
#  ifdef __cplusplus
#    ifdef METADATA_EXPORTS
#      define METADATA_API extern "C" __declspec(dllexport)
#    else
#      define METADATA_API extern "C" __declspec(dllimport)
#    endif
#  else
#    ifdef METADATA_EXPORTS
#      define METADATA_API __declspec(dllexport)
#    else
#      define METADATA_API __declspec(dllimport)
#    endif
#  endif
#else
#  ifdef __cplusplus
#    define METADATA_API extern "C" __attribute__((visibility("default")))
#  else
#    define METADATA_API __attribute__((visibility("default")))
#  endif
#endif

typedef struct {
    const char* key;
    const char* value;
} TagPair;

METADATA_API int SetTags(const char* audioPath, const TagPair* tags, int count);
METADATA_API int SetCoverImage(const char* audioPath, const char* imagePath);

#endif
