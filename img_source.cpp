// Valeurs par défaut seulement si la ligne de compilation (pkg-config) ne les fournit pas :
// elles DOIVENT correspondre à la build d'ImageMagick installée (MSYS2 = Q16 + HDRI).
#ifndef MAGICKCORE_QUANTUM_DEPTH
#define MAGICKCORE_QUANTUM_DEPTH 16
#endif
#ifndef MAGICKCORE_HDRI_ENABLE
#define MAGICKCORE_HDRI_ENABLE 0
#endif

#define IMAGE_API_EXPORT
#include "img_header.h"

#include <Magick++.h>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// Les chemins reçus sont en UTF-8 : conversion explicite en UTF-16 sous Windows
// (fs::path(const char*) utiliserait la page de code ANSI et abîmerait les accents).
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

static std::vector<std::string> splitWords(const std::string& text) {
    std::istringstream iss(text);
    std::vector<std::string> words;
    std::string word;
    while (iss >> word) {
        words.push_back(word);
    }
    return words;
}

static void DrawTextWithWrapping(
    Magick::Image& image,
    const std::string& text,
    const std::string& fontName,
    double fontSize,
    const char* colorStr,
    double margin,
    double maxWidth,
    double startY
) {
    image.font(fontName);
    image.fontPointsize(fontSize);
    
    Magick::TypeMetric tm;
    std::vector<std::string> words = splitWords(text);

    std::string line;
    std::vector<std::string> lines;

    // Créer les lignes avec retour automatique
    for (size_t i = 0; i < words.size(); ++i) {
        std::string testLine = line.empty() ? words[i] : line + " " + words[i];
        image.fontTypeMetrics(testLine, &tm);

        if (tm.textWidth() > maxWidth && !line.empty()) {
            lines.push_back(line);
            line = words[i];
        } else {
            line = testLine;
        }
    }

    // Ajouter la dernière ligne
    if (!line.empty()) {
        lines.push_back(line);
    }

    // Obtenir les métriques pour calculer la hauteur totale
    if (!lines.empty()) {
        image.fontTypeMetrics(lines[0], &tm);
    }

    // CORRECTION IMPORTANTE :
    // startY est le haut de la zone de texte (comme en C#)
    // Mais DrawableText attend la baseline de la première ligne
    // Donc on ajoute l'ascent (la partie du texte au-dessus de la baseline)
    double cursorY = startY + tm.ascent();

    // Dessiner chaque ligne centrée
    for (const auto& currentLine : lines) {
        image.fontTypeMetrics(currentLine, &tm);
        
        // Calculer la position X pour centrer le texte
        double lineWidth = tm.textWidth();
        double xPosition = margin + (maxWidth - lineWidth) / 2.0;

        std::vector<Magick::Drawable> drawList;
        drawList.push_back(Magick::DrawableFillColor(colorStr));
        drawList.push_back(Magick::DrawableStrokeColor(colorStr));
        drawList.push_back(Magick::DrawableStrokeWidth(0));
        drawList.push_back(Magick::DrawableText(xPosition, cursorY, currentLine));
        image.draw(drawList);

        // Passer à la ligne suivante (hauteur + espacement)
        cursorY += tm.textHeight() + 4;
    }
}

extern "C" IMAGE_API void CreateImage(
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
) {
    if (!prefix || !number || !title || !fontName || !color || !templatePath || !outputPath)
        return;

    try {
        Magick::InitializeMagick(nullptr);
        Magick::Image image(templatePath);
        std::string text = std::string(prefix) + " " + number + " : " + title;
        
        // Largeur maximale fixe de 760 comme dans le C#
        double maxWidth = 760.0;
        
        DrawTextWithWrapping(image, text, fontName, fontSize, color, margin, maxWidth, yStart);

        fs::path parent = U8Path(outputPath).parent_path();
        if (!parent.empty() && !fs::exists(parent)) {
            fs::create_directories(parent);
        }

        image.write(outputPath);
    } catch (...) {
    }
}