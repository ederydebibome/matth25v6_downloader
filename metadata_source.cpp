// metadata.cpp
//
// Convention : tous les `const char*` reçus sont en UTF-8 (chemins ET valeurs
// des tags). Les chemins sont convertis en UTF-16 sous Windows avant d'être
// donnés à std::filesystem / TagLib, sinon les accents sont déformés.

#ifndef METADATA_EXPORTS
#define METADATA_EXPORTS
#endif
#include "metadata_header.h"
#define TAGLIB_STATIC 
#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>

#include <cstdio>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper : chemin UTF-8 -> fs::path (UTF-16 sous Windows). Chemin vide si invalide.
// ---------------------------------------------------------------------------
static fs::path U8Path(const char* s) {
#ifdef _WIN32
    if (!s || !*s) return fs::path();
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
    if (n <= 0) return fs::path();
    std::wstring w((size_t)n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &w[0], n);
    return fs::path(w);
#else
    return fs::path(s ? s : "");
#endif
}

// ---------------------------------------------------------------------------
// Helper : lecture binaire d'un fichier
// ---------------------------------------------------------------------------
static std::vector<char> ReadFileBytes(const fs::path& path) {
#ifdef _WIN32
    FILE* in = _wfopen(path.c_str(), L"rb");
#else
    FILE* in = std::fopen(path.c_str(), "rb");
#endif
    if (!in) return {};
    std::vector<char> data;
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0) data.insert(data.end(), buf, buf + n);
    std::fclose(in);
    return data;
}

// ---------------------------------------------------------------------------
// Helper : type MIME basé sur l'extension
// ---------------------------------------------------------------------------
static TagLib::String MimeFromPath(const fs::path& path) {
    std::wstring ext = path.extension().wstring();
    for (auto& c : ext) c = static_cast<wchar_t>(std::towlower(c));
    if (ext == L".png")  return "image/png";
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".bmp")  return "image/bmp";
    if (ext == L".gif")  return "image/gif";
    return "image/unknown";
}

// ---------------------------------------------------------------------------
// MP3 : on n'écrit QUE de l'ID3v2.4 (texte en UTF-8). L'ID3v1, obsolète et
// limité au Latin-1, est supprimé du fichier (StripOthers) et jamais recréé
// (DoNotDuplicate).
// ---------------------------------------------------------------------------
static void InitTagLib() {
    TagLib::ID3v2::FrameFactory::instance()->setDefaultTextEncoding(TagLib::String::UTF8);
}

static bool SaveMP3(TagLib::MPEG::File& f) {
    return f.save(TagLib::MPEG::File::ID3v2,
                  TagLib::File::StripOthers,
                  TagLib::ID3v2::v4,
                  TagLib::File::DoNotDuplicate);
}

static bool IsMp3(const fs::path& p) {
    std::wstring ext = p.extension().wstring();
    for (auto& c : ext) c = static_cast<wchar_t>(std::towlower(c));
    return ext == L".mp3";
}

// Applique les paires clé/valeur UTF-8 à une PropertyMap
static void ApplyTags(TagLib::PropertyMap& props, const TagPair* tags, int count) {
    for (int i = 0; i < count; ++i) {
        const char* k = tags[i].key;
        const char* v = tags[i].value;

        if (!k || !k[0]) continue;

        TagLib::String key(k, TagLib::String::UTF8);

        if (!v) {
            props.erase(key);
        } else {
            TagLib::StringList list;
            list.append(TagLib::String(v, TagLib::String::UTF8));
            props.replace(key, list);
        }
    }
}

// ---------------------------------------------------------------------------
// Écriture d'une pochette dans un MP3 (ID3v2 APIC)
// ---------------------------------------------------------------------------
static bool WriteCoverMP3(const fs::path& audioPath, const fs::path& imagePath) {
    TagLib::MPEG::File f(audioPath.c_str());
    if (!f.isValid()) return false;

    TagLib::ID3v2::Tag* tag = f.ID3v2Tag(true);
    if (!tag) return false;

    auto picData = ReadFileBytes(imagePath);
    if (picData.empty()) return false;

    auto frames = tag->frameList("APIC");
    for (auto fr : frames) tag->removeFrame(fr);

    auto* frame = new TagLib::ID3v2::AttachedPictureFrame;
    frame->setMimeType(MimeFromPath(imagePath));
    frame->setPicture(TagLib::ByteVector(picData.data(), static_cast<unsigned int>(picData.size())));
    frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    frame->setDescription("Cover");
    tag->addFrame(frame);

    return SaveMP3(f);
}

// ---------------------------------------------------------------------------
// Écriture d'une pochette dans un FLAC (PICTURE)
// ---------------------------------------------------------------------------
static bool WriteCoverFLAC(const fs::path& audioPath, const fs::path& imagePath) {
    TagLib::FLAC::File f(audioPath.c_str());
    if (!f.isValid()) return false;

    auto picData = ReadFileBytes(imagePath);
    if (picData.empty()) return false;

    TagLib::FLAC::Picture* pic = new TagLib::FLAC::Picture;
    pic->setMimeType(MimeFromPath(imagePath));
    pic->setData(TagLib::ByteVector(picData.data(), static_cast<unsigned int>(picData.size())));
    pic->setType(TagLib::FLAC::Picture::FrontCover);
    pic->setDescription("Cover");

    f.removePictures();
    f.addPicture(pic);
    return f.save();
}

// ---------------------------------------------------------------------------
// Fonction générique d'écriture de propriétés (API publique)
// ---------------------------------------------------------------------------
extern "C" METADATA_API int SetTags(const char* audioPath, const TagPair* tags, int count) {
    if (!audioPath || count < 0 || (count > 0 && !tags)) return 1;

    fs::path path = U8Path(audioPath);
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) return 1;

    InitTagLib();

    if (IsMp3(path)) {
        TagLib::MPEG::File f(path.c_str());
        if (!f.isValid()) return 1;
        TagLib::PropertyMap props = f.properties();
        ApplyTags(props, tags, count);
        f.setProperties(props);
        return SaveMP3(f) ? 0 : 1;
    }

    // Autres formats (FLAC, ...) : API générique
    TagLib::FileRef file(path.c_str());
    if (file.isNull()) return 1;

    TagLib::PropertyMap props = file.properties();
    ApplyTags(props, tags, count);
    file.setProperties(props);
    return file.save() ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Fonction d'ajout de pochette (API publique)
// ---------------------------------------------------------------------------
extern "C" METADATA_API int SetCoverImage(const char* audioPath, const char* imagePath) {
    if (!audioPath || !imagePath || !imagePath[0]) return 1;

    fs::path audio = U8Path(audioPath);
    fs::path image = U8Path(imagePath);
    std::error_code ec;
    if (audio.empty() || image.empty() || !fs::exists(audio, ec) || !fs::exists(image, ec)) return 1;

    InitTagLib();

    std::wstring ext = audio.extension().wstring();
    for (auto& c : ext) c = static_cast<wchar_t>(std::towlower(c));

    if (ext == L".mp3") {
        return WriteCoverMP3(audio, image) ? 0 : 1;
    } else if (ext == L".flac") {
        return WriteCoverFLAC(audio, image) ? 0 : 1;
    } else {
        return 0; // pas bloquant pour les autres formats
    }
}
