#define DOWNLOAD_API_EXPORT
#include "net_header.h"

#include <windows.h>
#include <wininet.h>
#include <string>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Convention : tous les `const char*` de cette DLL sont en UTF-8.
// ---------------------------------------------------------------------------

// UTF-8 -> UTF-16 (échoue si l'UTF-8 est invalide au lieu de le déformer en silence)
static bool Utf8ToWide(const char* s, std::wstring& out) {
    out.clear();
    if (!s || !*s) return false;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
    if (n <= 1) return false;
    out.assign((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &out[0], n);
    return true;
}

// Encode en %XX (octets UTF-8) les caractères non permis dans une URL
// (espaces, accents, ...). Les %XX déjà présents ne sont pas ré-encodés.
static std::string EscapeUrl(const std::string& url) {
    static const char* HEX = "0123456789ABCDEF";
    std::string out;
    out.reserve(url.size() + 16);
    for (unsigned char c : url) {
        bool unsafe = (c <= 0x20 || c >= 0x7F ||
                       c == '"' || c == '<' || c == '>' || c == '\\' ||
                       c == '^' || c == '`' || c == '{' || c == '|' || c == '}');
        if (unsafe) {
            out += '%';
            out += HEX[c >> 4];
            out += HEX[c & 0x0F];
        } else {
            out += (char)c;
        }
    }
    return out;
}

// Téléchargement vers "<sortie>.part", puis remplacement atomique de la
// destination uniquement si tout s'est bien passé : un échec ne détruit donc
// jamais une ancienne version valide.
static bool DownloadFileImpl(const char* urlUtf8, const char* outputUtf8) {
    std::wstring wOut;
    if (!Utf8ToWide(outputUtf8, wOut)) {
        std::cerr << "Error: Invalid UTF-8 output path" << std::endl;
        return false;
    }
    fs::path outPath(wOut);
    fs::path partPath = outPath;
    partPath += L".part";

    const std::string url = EscapeUrl(urlUtf8);

    HINTERNET hInternet = InternetOpenA("DownloadAgent/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        std::cerr << "Error: Failed to initialize WinINet" << std::endl;
        return false;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
                                      INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!hUrl) {
        std::cerr << "Error: Failed to open URL: " << url << std::endl;
        InternetCloseHandle(hInternet);
        return false;
    }

    // Une page d'erreur (404, 500...) ne doit jamais être enregistrée comme un fichier valide.
    DWORD status = 0, statusSize = sizeof(status);
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &statusSize, NULL)) {
        if (status < 200 || status >= 300) {
            std::cerr << "Error: HTTP status " << status << " for " << url << std::endl;
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return false;
        }
    }

    DWORD expected = 0, expectedSize = sizeof(expected);
    bool hasExpected = HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                                      &expected, &expectedSize, NULL) != FALSE;

    std::error_code ec;
    fs::path parentDir = outPath.parent_path();
    if (!parentDir.empty() && !fs::exists(parentDir, ec)) {
        fs::create_directories(parentDir, ec);
        if (ec) {
            std::cerr << "Error: Failed to create directory: " << ec.message() << std::endl;
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return false;
        }
    }

    bool ok = true;
    unsigned long long totalBytes = 0;
    {
        HANDLE hFile = CreateFileW(partPath.c_str(), GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            std::cerr << "Error: Cannot write temporary file" << std::endl;
            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return false;
        }

        static BYTE buffer[65536];
        DWORD bytesRead = 0;
        BOOL readOk;
        while ((readOk = InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead)) && bytesRead > 0) {
            DWORD written = 0;
            if (!WriteFile(hFile, buffer, bytesRead, &written, NULL) || written != bytesRead) {
                ok = false;
                break;
            }
            totalBytes += bytesRead;
        }
        if (!readOk) ok = false;          // coupure réseau en cours de route
        if (!CloseHandle(hFile)) ok = false;
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (ok && totalBytes == 0) {
        std::cerr << "Error: No data downloaded" << std::endl;
        ok = false;
    }
    if (ok && hasExpected && totalBytes != expected) {
        std::cerr << "Error: Incomplete download (" << totalBytes << "/" << expected << " bytes)" << std::endl;
        ok = false;
    }

    if (!ok) {
        fs::remove(partPath, ec);
        return false;
    }

    if (!MoveFileExW(partPath.c_str(), outPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::cerr << "Error: Cannot replace destination file (code " << GetLastError() << ")" << std::endl;
        fs::remove(partPath, ec);
        return false;
    }

    std::cout << "Downloaded " << totalBytes << " bytes" << std::endl;
    return true;
}

// Public C API
extern "C" DOWNLOAD_API int DownloadFile(const char* url, const char* outputPath) {
    if (!url || !outputPath) {
        std::cerr << "Error: NULL parameter passed to DownloadFile" << std::endl;
        return 0;
    }

    try {
        return DownloadFileImpl(url, outputPath) ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: Exception in DownloadFile: " << e.what() << std::endl;
        return 0;
    } catch (...) {
        std::cerr << "Error: Unknown exception in DownloadFile" << std::endl;
        return 0;
    }
}
