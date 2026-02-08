#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>

#include "bbox.h"
#include "cache_bbox.h"
#include "grid_index.h"
#include "ply_bbox_reader.h"
#include "timer.h"
#include "read_parallel.h"
#include "batch_overlap.h"
#include "crop_minus1.h"


namespace fs = std::filesystem;

struct Args {
    std::string inputDir;
    std::string treeFile;   // nombre o ruta parcial del ply target
    double buffer = 1.0;
    double eps = 0.0;
    double cellSize = 1.0;
    int threads = 8;
    std::string cacheFile = "bboxes_cache.bin";
    bool useCache = true;
    bool allPairs = false;
    std::string outCsv = "overlaps.csv";
    std::string outTxt = "";
    double gapXY = 0.04;
    double minOverlapZ = 0.04;
};

static void printUsage() {
    std::cout <<
    "Uso:\n"
    "  overlap_trees --input <dir> --tree <archivo.ply> [--buffer B] [--eps E] [--cell C]\n\n"
    "Ejemplo:\n"
    "  overlap_trees --input ./segments --tree tree_411.ply --buffer 1.0 --eps 0.0 --cell 1.0\n";
}

static Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];

        auto needValue = [&](const char* opt) {
            if (i + 1 >= argc) throw std::runtime_error(std::string("Falta valor para ") + opt);
            return std::string(argv[++i]);
        };

        if (k == "--input") a.inputDir = needValue("--input");
        else if (k == "--tree") a.treeFile = needValue("--tree");
        else if (k == "--buffer") a.buffer = std::stod(needValue("--buffer"));
        else if (k == "--eps") a.eps = std::stod(needValue("--eps"));
        else if (k == "--cell") a.cellSize = std::stod(needValue("--cell"));
        else if (k == "--threads") a.threads = std::stoi(needValue("--threads"));
        else if (k == "--cache") a.cacheFile = needValue("--cache");
        else if (k == "--no-cache") a.useCache = false;
        else if (k == "--gap-xy") a.gapXY = std::stod(needValue("--gap-xy"));
        else if (k == "--min-overlap-z") a.minOverlapZ = std::stod(needValue("--min-overlap-z"));

        // Batch mode
        else if (k == "--all") a.allPairs = true;
        else if (k == "--out") a.outCsv = needValue("--out");
        else if (k == "--out-txt") a.outTxt = needValue("--out-txt");

        else if (k == "--help" || k == "-h") {
            printUsage();
            std::exit(0);
        }
        else {
            throw std::runtime_error("Argumento desconocido: " + k);
        }
    }

    // ---- Validaciones correctas ----
    if (a.inputDir.empty()) {
        printUsage();
        throw std::runtime_error("Debes pasar --input <dir>");
    }

    // Si NO es batch, entonces sí se requiere --tree
    if (!a.allPairs && a.treeFile.empty()) {
        printUsage();
        throw std::runtime_error("Debes pasar --tree <archivo.ply> si no usas --all");
    }

    // Si es batch, se recomienda --out (pero lo dejamos opcional)
    // a.outCsv ya tiene default "overlaps.csv"

    if (a.cellSize <= 0) a.cellSize = std::max(0.01, a.buffer);
    if (a.threads <= 0) a.threads = 1;

    return a;
}


static std::vector<std::string> listPlyFiles(const std::string& dir) {
    std::vector<std::string> out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        auto p = e.path();
        if (p.extension() == ".ply") {
            const std::string name = p.filename().string();
            if (name.find("_segmented_-1.ply") != std::string::npos) continue;
            out.push_back(p.string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

static int findTargetId(const std::vector<BBox>& boxes, const std::string& treeFile) {
    // 1) match por nombre exacto (solo filename)
    for (size_t i = 0; i < boxes.size(); ++i) {
        const std::string filename = fs::path(boxes[i].file).filename().string();
        if (filename == fs::path(treeFile).filename().string()) return static_cast<int>(i);
    }
    // 2) match por substring
    for (size_t i = 0; i < boxes.size(); ++i) {
        if (boxes[i].file.find(treeFile) != std::string::npos) return static_cast<int>(i);
    }
    return -1;
}


static void normalizeBoxes(std::vector<BBox>& boxes,
                           double& ox, double& oy, double& oz)
{
    if (boxes.empty()) { ox = oy = oz = 0.0; return; }

    // 1) extremos globales
    double gMinX = boxes[0].minX, gMaxX = boxes[0].maxX;
    double gMinY = boxes[0].minY, gMaxY = boxes[0].maxY;
    double gMinZ = boxes[0].minZ, gMaxZ = boxes[0].maxZ;

    for (const auto& b : boxes) {
        gMinX = std::min(gMinX, b.minX); gMaxX = std::max(gMaxX, b.maxX);
        gMinY = std::min(gMinY, b.minY); gMaxY = std::max(gMaxY, b.maxY);
        gMinZ = std::min(gMinZ, b.minZ); gMaxZ = std::max(gMaxZ, b.maxZ);
    }

    // 2) origen = centro del dataset (mejor que usar min)
    ox = 0.5 * (gMinX + gMaxX);
    oy = 0.5 * (gMinY + gMaxY);
    oz = 0.5 * (gMinZ + gMaxZ);

    // 3) shift: restar origen a todas las cajas
    for (auto& b : boxes) {
        b.minX -= ox; b.maxX -= ox;
        b.minY -= oy; b.maxY -= oy;
        b.minZ -= oz; b.maxZ -= oz;
    }
}

int main(int argc, char** argv) {
    try {
        Args args = parseArgs(argc, argv);

        // 1) listar PLYs
        auto files = listPlyFiles(args.inputDir);
        if (files.empty()) throw std::runtime_error("No encontré .ply en " + args.inputDir);

        // 2) calcular bbox por archivo
        std::vector<BBox> boxes;
        {
            ScopedTimer t("read bboxes (cache+parallel)");
            bool updated = false;

            if (args.useCache) {
                boxes = buildBBoxesWithCache(args.inputDir, files, args.threads, args.cacheFile, updated);
                std::cerr << "[CACHE] " << (updated ? "updated" : "hit")
                          << " file=" << args.cacheFile << "\n";
            } else {
                boxes = readAllBBoxesParallel(files, args.threads);
                std::cerr << "[CACHE] disabled\n";
            }
        }

        double originX=0, originY=0, originZ=0;
        normalizeBoxes(boxes, originX, originY, originZ);

        std::cerr << "[NORM] origin=(" << originX << "," << originY << "," << originZ << ")\n";

        // 3) construir grid
        GridIndex index(args.cellSize);
        index.build(boxes);

        // Parámetros de cercanía (en metros)
        const double maxGapXY    = args.gapXY;        // ejemplo: 0.04 (4 cm)
        const double minOverlapZ = args.minOverlapZ;  // ejemplo: 0.04 (4 cm)

        // =====================================================
        // MODO BATCH: buscar "cercanos 3D" para TODOS los árboles
        // =====================================================
        if (args.allPairs) {
            std::vector<OverlapPair> pairs;

            {
                ScopedTimer t("batch near3D (cells)");
                // RECOMENDADO: actualiza la firma de findAllOverlapsByCells
                // para que use maxGapXY y minOverlapZ (y no eps).
                pairs = findAllOverlapsByCells(
                    boxes,
                    index,
                    args.threads,
                    maxGapXY,
                    minOverlapZ
                );
            }

            {
                ScopedTimer t("write csv");
                if (!savePairsCSV(args.outCsv, boxes, pairs)) {
                    throw std::runtime_error("No pude escribir CSV: " + args.outCsv);
                }
            }

            if (!args.outTxt.empty()) {
                ScopedTimer t("write txt");
                if (!savePairsTXT(args.outTxt, boxes, pairs)) {
                    throw std::runtime_error("No pude escribir TXT: " + args.outTxt);
                }
            }

            std::cout << "Pares candidatos (near3D): " << pairs.size() << "\n";
            std::cout << "gapXY=" << maxGapXY << "  minOverlapZ=" << minOverlapZ
                      << "  cell=" << args.cellSize << "  threads=" << args.threads << "\n";
            std::cout << "CSV: " << args.outCsv << "\n";
            if (!args.outTxt.empty()) std::cout << "TXT: " << args.outTxt << "\n";
            return 0;
        }

        // =====================================================
        // MODO INDIVIDUAL: árbol seleccionado (--tree)
        // =====================================================

        // 4) identificar target
        int targetId = findTargetId(boxes, args.treeFile);
        if (targetId < 0) {
            throw std::runtime_error("No encontré el árbol target: " + args.treeFile);
        }
        const BBox& target = boxes[targetId];

        // =====================================================
        // RECORTE de nube -1 usando bbox EXACTO del árbol (solo XY, todo Z)
        // Se guarda en un folder nuevo: <inputDir>/crops_minus1/
        // =====================================================
        {
            // nombre del target y nombre esperado del -1
            const std::string targetName = fs::path(target.file).filename().string();
            const std::string minus1Name = buildMinus1NameFromTree(targetName);

            fs::path minus1Path = fs::path(args.inputDir) / minus1Name;

            // folder de salida
            fs::path outDir = fs::path(args.inputDir) / "crops_minus1";
            fs::path outFile = outDir / buildCropNameFromTree(targetName);

            if (fs::exists(minus1Path)) {
                // NOTA: usamos target (bbox exacto) sin expandXY
                cropMinus1XYKeepAllZ(minus1Path, target, originX, originY, originZ, outFile);
            } else {
                std::cerr << "[CROP -1] No existe archivo -1 esperado: " << minus1Path.string() << "\n";
            }
        }

        // 5) query candidatos con buffer en XY
        // Aseguramos que el buffer no sea menor al umbral de gap,
        // para no perder vecinos a centímetros.
        const double searchBuffer = std::max(args.buffer, maxGapXY);
        BBox q = expandXY(target, searchBuffer);
        auto candidates = index.queryCandidates(q);

        // 6) filtrar candidatos por criterio "cercanía XY + overlapZ mínimo"
        std::vector<int> overlaps;
        overlaps.reserve(candidates.size());

        for (int id : candidates) {
            if (id == targetId) continue;

            // IMPORTANTE: quitamos overlapsXY(), porque eso exige intersección XY
            // y nosotros queremos "cercanía" (gap <= maxGapXY).
            if (near3D_cm(target, boxes[id], minOverlapZ, maxGapXY)) {
                overlaps.push_back(id);
            }
        }

        // 7) imprimir
        std::cout << "Target: " << fs::path(target.file).filename().string()
                  << "  (id=" << targetId << ")\n";

        std::cout << "searchBuffer=" << searchBuffer
                  << "  gapXY=" << maxGapXY
                  << "  minOverlapZ=" << minOverlapZ
                  << "  cell=" << args.cellSize
                  << "  threads=" << args.threads
                  << "\n";

        std::cout << "Candidatos: " << candidates.size()
                  << " | Candidatos near3D: " << overlaps.size() << "\n";

        for (int id : overlaps) {
            std::cout << " - " << fs::path(boxes[id].file).filename().string()
                      << " (id=" << id << ")\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
