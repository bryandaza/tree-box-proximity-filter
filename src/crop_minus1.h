#pragma once
#include <filesystem>
#include "bbox.h"

namespace fs = std::filesystem;

// Construye el nombre del archivo -1 a partir del nombre del árbol:
//  AMA_..._159648_segmented_0.ply  ->  AMA_..._159648_segmented_-1.ply
std::string buildMinus1NameFromTree(const std::string& treeFilename);

// Construye el nombre del recorte:
//  AMA_..._segmented_0.ply -> AMA_..._segmented_0_-1.ply
std::string buildCropNameFromTree(const std::string& treeFilename);

// Recorta la nube -1 por XY usando el bbox EXACTO del árbol (sin buffer)
// y guarda el resultado en outFile.
// originX/Y/Z son los offsets de normalización global (para trabajar en el mismo sistema).
bool cropMinus1XYKeepAllZ(const fs::path& minus1File,
                          const BBox& treeBoxExact,
                          double originX, double originY, double originZ,
                          const fs::path& outFile);