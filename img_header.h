#ifndef IMAGE_CREATOR_H
#define IMAGE_CREATOR_H

#if defined(_WIN32)
  #ifdef IMAGE_API_EXPORT
    #define IMAGE_API __declspec(dllexport)
  #elif defined(IMAGE_API_IMPORT)
    #define IMAGE_API __declspec(dllimport)
  #else
    #define IMAGE_API
  #endif
#else
  #define IMAGE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Création d'image avec texte automatiquement formaté avec retour à la ligne
// Tous les paramètres sont fournis par l'appelant pour une flexibilité maximale
IMAGE_API void CreateImage(
    const char* prefix,       // Préfixe du texte (ex: "Kacou", "Episode", "Chapter")
    const char* number,       // Numéro (ex: "8", "123")
    const char* title,        // Titre
    const char* fontName,     // Nom de la police (ex: "Arial", "Times New Roman")
    double      fontSize,     // Taille de la police (ex: 24.0)
    const char* color,        // Couleur du texte (ex: "white", "#FF0000")
    const char* templatePath, // Chemin du template image
    const char* outputPath,   // Chemin de sortie
    double      margin,       // Marge gauche et droite (ex: 20.0)
    double      yStart        // Position Y de départ du texte (ex: 460.0)
);

#ifdef __cplusplus
}
#endif

#endif // IMAGE_CREATOR_H