#ifndef DOWNLOAD_API_H
#define DOWNLOAD_API_H

#if defined(_WIN32)
  #ifdef DOWNLOAD_API_EXPORT
    #define DOWNLOAD_API __declspec(dllexport)
  #elif defined(DOWNLOAD_API_IMPORT)
    #define DOWNLOAD_API __declspec(dllimport)
  #else
    #define DOWNLOAD_API
  #endif
#else
  #define DOWNLOAD_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

DOWNLOAD_API int DownloadFile(const char* url, const char* outputPath);

#ifdef __cplusplus
}
#endif

#endif // DOWNLOAD_API_H