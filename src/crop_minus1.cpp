#include "crop_minus1.h"
#include "tinyply.h"
#include <fstream>
#include <vector>
#include <iostream>

using namespace tinyply;

std::string buildMinus1NameFromTree(const std::string& treeFilename) {
    const std::string key = "_segmented_";
    auto pos = treeFilename.find(key);
    if (pos == std::string::npos) return {};
    return treeFilename.substr(0, pos + key.size()) + "-1.ply";
}

std::string buildCropNameFromTree(const std::string& treeFilename) {
    // input:  ..._segmented_0.ply
    // output: ..._segmented_0_-1.ply
    auto pos = treeFilename.rfind(".ply");
    if (pos == std::string::npos) return treeFilename + "_-1.ply";
    return treeFilename.substr(0, pos) + "_-1.ply";
}

static bool writePlyXYZ(const fs::path& outFile,
                        const std::vector<double>& xs,
                        const std::vector<double>& ys,
                        const std::vector<double>& zs)
{
    std::filebuf fb;
    fb.open(outFile.string(), std::ios::out | std::ios::binary);
    if (!fb.is_open()) return false;
    std::ostream outstream(&fb);

    std::vector<double> interleaved;
    interleaved.reserve(xs.size() * 3);

    for (size_t i = 0; i < xs.size(); ++i) {
        interleaved.push_back(xs[i]);
        interleaved.push_back(ys[i]);
        interleaved.push_back(zs[i]);
    }

    PlyFile outPly;
    outPly.add_properties_to_element("vertex", { "x","y","z" },
        Type::FLOAT64, xs.size(),
        reinterpret_cast<uint8_t*>(interleaved.data()), Type::INVALID, 0);

    outPly.get_comments().push_back("cropped from -1 by XY (tree bbox), Z unbounded, saved as FLOAT64 ABS");
    outPly.write(outstream, true);
    return true;
}

bool cropMinus1XYKeepAllZ(const fs::path& minus1File,
                          const BBox& treeBoxExact,
                          double originX, double originY, double /*originZ*/,
                          const fs::path& outFile)
{
    std::ifstream ss(minus1File, std::ios::binary);
    if (!ss) {
        std::cerr << "[CROP -1] no pude abrir: " << minus1File.string() << "\n";
        return false;
    }

    PlyFile ply;
    ply.parse_header(ss);

    // 1) Pedir x, y, z por separado (robusto)
    std::shared_ptr<PlyData> vx, vy, vz;
    try {
        vx = ply.request_properties_from_element("vertex", { "x" });
        vy = ply.request_properties_from_element("vertex", { "y" });
        vz = ply.request_properties_from_element("vertex", { "z" });
    } catch (...) {
        std::cerr << "[CROP -1] el PLY no tiene vertex x/y/z: " << minus1File.string() << "\n";
        return false;
    }

    ply.read(ss);

    // 2) Validar tamaños
    const size_t n = vx->count;
    if (n == 0) {
        std::cerr << "[CROP -1] sin vertices: " << minus1File.string() << "\n";
        return false;
    }
    if (vy->count != n || vz->count != n) {
        std::cerr << "[CROP -1] counts inconsistentes x/y/z en: " << minus1File.string() << "\n";
        return false;
    }

    std::vector<double> xs, ys, zs;
    xs.reserve(n / 20);
    ys.reserve(n / 20);
    zs.reserve(n / 20);

    auto keepXY = [&](double xN, double yN) {
        return (xN >= treeBoxExact.minX && xN <= treeBoxExact.maxX &&
                yN >= treeBoxExact.minY && yN <= treeBoxExact.maxY);
    };

    // 3) Helper: leer valor i de un PlyData (float32 o float64)
    auto readAt = [](const std::shared_ptr<PlyData>& d, size_t i) -> double {
        if (d->t == Type::FLOAT32) {
            return static_cast<double>(reinterpret_cast<const float*>(d->buffer.get())[i]);
        } else if (d->t == Type::FLOAT64) {
            return reinterpret_cast<const double*>(d->buffer.get())[i];
        }
        throw std::runtime_error("tipo de vertex no soportado (esperaba float32/float64)");
    };

    // 4) Recorte: filtrar por XY normalizado, pero guardar ABS (xAbs,yAbs,zAbs)
    try {
        for (size_t i = 0; i < n; ++i) {
            const double xAbs = readAt(vx, i);
            const double yAbs = readAt(vy, i);
            const double zAbs = readAt(vz, i);

            const double xN = xAbs - originX;
            const double yN = yAbs - originY;

            if (keepXY(xN, yN)) {
                xs.push_back(xAbs);
                ys.push_back(yAbs);
                zs.push_back(zAbs); // Z sin filtro (entra todo)
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[CROP -1] error leyendo vertices: " << e.what()
                  << " file=" << minus1File.string() << "\n";
        return false;
    }

    fs::create_directories(outFile.parent_path());

    if (xs.empty()) {
        std::cerr << "[CROP -1] recorte vacío (0 puntos) -> " << outFile.string() << "\n";
        return true;
    }

    if (!writePlyXYZ(outFile, xs, ys, zs)) {
        std::cerr << "[CROP -1] no pude escribir: " << outFile.string() << "\n";
        return false;
    }

    std::cerr << "[CROP -1] wrote " << xs.size() << " pts -> " << outFile.string() << "\n";
    return true;
}
