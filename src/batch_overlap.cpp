#include "batch_overlap.h"
#include <unordered_set>
#include <atomic>
#include <thread>
#include <mutex>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

static uint64_t pairKey(int i, int j) {
    uint32_t a = static_cast<uint32_t>(std::min(i, j));
    uint32_t b = static_cast<uint32_t>(std::max(i, j));
    return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
}

std::vector<OverlapPair> findAllOverlapsByCells(
    const std::vector<BBox>& boxes,
    const GridIndex& index,
    int threads,
    double maxGapXY,
    double minOverlapZ
) {
    // Convertimos celdas a una lista indexable para paralelizar
    std::vector<const std::vector<int>*> cellLists;
    cellLists.reserve(index.cells().size());
    for (const auto& kv : index.cells()) {
        cellLists.push_back(&kv.second);
    }

    std::atomic<int> next{0};
    std::mutex mergeMtx;

    std::unordered_set<uint64_t> globalSeen;
    globalSeen.reserve(cellLists.size() * 4);

    std::vector<OverlapPair> globalOut;
    globalOut.reserve(4096);

    auto worker = [&]() {
        std::unordered_set<uint64_t> localSeen;
        localSeen.reserve(4096);

        std::vector<OverlapPair> localOut;
        localOut.reserve(2048);

        while (true) {
            int idx = next.fetch_add(1);
            if (idx >= (int)cellLists.size()) break;

            const auto& ids = *cellLists[idx];
            const int m = (int)ids.size();
            if (m < 2) continue;

            for (int a = 0; a < m; ++a) {
                int ia = ids[a];
                const BBox& A = boxes[ia];

                for (int b = a + 1; b < m; ++b) {
                    int ib = ids[b];
                    const BBox& B = boxes[ib];

                    const double minOverlapZ = 0.04;
                    const double maxGapXY    = 0.04;
                    if (!near3D_cm(A, B, minOverlapZ, maxGapXY)) continue;


                    uint64_t k = pairKey(ia, ib);
                    if (localSeen.insert(k).second) {
                        localOut.push_back({ std::min(ia, ib), std::max(ia, ib) });
                    }
                }
            }
        }

        // Merge local -> global
        std::lock_guard<std::mutex> lk(mergeMtx);
        for (const auto& p : localOut) {
            uint64_t k = pairKey(p.a, p.b);
            if (globalSeen.insert(k).second) {
                globalOut.push_back(p);
            }
        }
    };

    int t = std::max(1, std::min(threads, (int)cellLists.size()));
    std::vector<std::thread> ths;
    ths.reserve(t);
    for (int i = 0; i < t; ++i) ths.emplace_back(worker);
    for (auto& th : ths) th.join();

    return globalOut;
}

bool savePairsCSV(
    const std::string& outCsv,
    const std::vector<BBox>& boxes,
    const std::vector<OverlapPair>& pairs
) {
    fs::path p(outCsv);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());

    std::ofstream out(outCsv);
    if (!out) return false;

    // Incluye alturas y Z
    out << "a_id,b_id,a_file,b_file,a_height,b_height,a_minZ,a_maxZ,b_minZ,b_maxZ\n";

    for (const auto& pr : pairs) {
        const BBox& A = boxes[pr.a];
        const BBox& B = boxes[pr.b];

        out << pr.a << "," << pr.b << ","
            << fs::path(A.file).filename().string() << ","
            << fs::path(B.file).filename().string() << ","
            << A.height() << "," << B.height() << ","
            << A.minZ << "," << A.maxZ << ","
            << B.minZ << "," << B.maxZ << "\n";
    }
    return true;
}

bool savePairsTXT(
    const std::string& outTxt,
    const std::vector<BBox>& boxes,
    const std::vector<OverlapPair>& pairs
) {
    fs::path p(outTxt);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());

    std::ofstream out(outTxt);
    if (!out) return false;

    for (const auto& pr : pairs) {
        const BBox& A = boxes[pr.a];
        const BBox& B = boxes[pr.b];
        out << fs::path(A.file).filename().string()
            << " (h=" << A.height() << ")  <->  "
            << fs::path(B.file).filename().string()
            << " (h=" << B.height() << ")\n";
    }
    return true;
}
