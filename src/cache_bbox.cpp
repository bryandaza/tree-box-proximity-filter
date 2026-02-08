#include "cache_bbox.h"
#include "ply_bbox_reader.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <atomic>
#include <thread>
#include <stdexcept>

namespace fs = std::filesystem;

// -------------------------
// Helpers de lectura/escritura binaria
// -------------------------
static void writeU32(std::ofstream& out, uint32_t v) { out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
static void writeU64(std::ofstream& out, uint64_t v) { out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
static void writeI64(std::ofstream& out, int64_t v)  { out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }
static void writeF64(std::ofstream& out, double v)   { out.write(reinterpret_cast<const char*>(&v), sizeof(v)); }

static uint32_t readU32(std::ifstream& in) { uint32_t v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
static uint64_t readU64(std::ifstream& in) { uint64_t v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
static int64_t  readI64(std::ifstream& in)  { int64_t v;  in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v; }
static double   readF64(std::ifstream& in)  { double v;   in.read(reinterpret_cast<char*>(&v), sizeof(v)); return v; }

static void writeString(std::ofstream& out, const std::string& s) {
    writeU32(out, static_cast<uint32_t>(s.size()));
    out.write(s.data(), s.size());
}

static std::string readString(std::ifstream& in) {
    uint32_t n = readU32(in);
    std::string s(n, '\0');
    in.read(s.data(), n);
    return s;
}

// last_write_time portable -> int64
static int64_t fileMtime(const fs::path& p) {
    auto ftime = fs::last_write_time(p);
    // Convertimos file_time_type a "ticks" (count) como int64
    // Es consistente dentro de la misma máquina.
    return static_cast<int64_t>(ftime.time_since_epoch().count());
}

static uint64_t fileSize(const fs::path& p) {
    return static_cast<uint64_t>(fs::file_size(p));
}

bool loadBBoxCache(const std::string& cacheFile, CacheData& cache) {
    std::ifstream in(cacheFile, std::ios::binary);
    if (!in) return false;

    // Header: magic + version
    char magic[8];
    in.read(magic, 8);
    if (!in) return false;

    const std::string expected = "BBXCACHE"; // 8 bytes
    if (std::string(magic, 8) != expected) return false;

    uint32_t version = readU32(in);
    if (version != 1) return false;

    uint32_t count = readU32(in);

    cache.version = version;
    cache.byPath.clear();
    cache.byPath.reserve(count * 2);

    for (uint32_t i = 0; i < count; ++i) {
        CacheEntry e;
        e.relPath = readString(in);
        e.fileSize = readU64(in);
        e.mtime = readI64(in);

        e.bbox.minX = readF64(in); e.bbox.maxX = readF64(in);
        e.bbox.minY = readF64(in); e.bbox.maxY = readF64(in);
        e.bbox.minZ = readF64(in); e.bbox.maxZ = readF64(in);

        // id y file se rellenan luego
        cache.byPath[e.relPath] = std::move(e);
    }

    return true;
}

bool saveBBoxCache(const std::string& cacheFile, const CacheData& cache) {
    fs::path p(cacheFile);

    // 1) Crear carpeta padre si no existe
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path());
    }

    // 2) Abrir archivo
    std::ofstream out(cacheFile, std::ios::binary);
    if (!out) {
        throw std::runtime_error("No pude crear/escribir el cache: " + cacheFile);
    }

    // Header
    out.write("BBXCACHE", 8);
    writeU32(out, 1);

    // entries
    writeU32(out, static_cast<uint32_t>(cache.byPath.size()));
    for (const auto& kv : cache.byPath) {
        const CacheEntry& e = kv.second;
        writeString(out, e.relPath);
        writeU64(out, e.fileSize);
        writeI64(out, e.mtime);

        writeF64(out, e.bbox.minX); writeF64(out, e.bbox.maxX);
        writeF64(out, e.bbox.minY); writeF64(out, e.bbox.maxY);
        writeF64(out, e.bbox.minZ); writeF64(out, e.bbox.maxZ);
    }

    // 3) Asegurar que se escribió todo
    out.flush();
    if (!out) {
        throw std::runtime_error("Error al flush del cache: " + cacheFile);
    }

    return true;
}

static std::string makeRelPath(const std::string& inputDir, const std::string& absFile) {
    fs::path base(inputDir);
    fs::path p(absFile);
    auto rel = fs::relative(p, base);
    return rel.generic_string(); // slash normalizado
}

std::vector<BBox> buildBBoxesWithCache(
    const std::string& inputDir,
    const std::vector<std::string>& absFiles,
    int numThreads,
    const std::string& cacheFile,
    bool& cacheWasUpdated
) {
    cacheWasUpdated = false;

    CacheData cache;
    loadBBoxCache(cacheFile, cache);

    const int n = static_cast<int>(absFiles.size());
    std::vector<BBox> out(n);

    // Prepara lista de trabajos: solo los que cambiaron o no existen
    struct WorkItem { int idx; std::string abs; std::string rel; uint64_t size; int64_t mt; };
    std::vector<WorkItem> work;
    work.reserve(n);

    for (int i = 0; i < n; ++i) {
        const std::string& abs = absFiles[i];
        std::string rel = makeRelPath(inputDir, abs);

        uint64_t sz = fileSize(abs);
        int64_t mt = fileMtime(abs);

        auto it = cache.byPath.find(rel);
        const bool hit = (it != cache.byPath.end() &&
                          it->second.fileSize == sz &&
                          it->second.mtime == mt);

        if (hit) {
            // Cache hit: rellenar bbox
            BBox b = it->second.bbox;
            b.id = i;
            b.file = abs;
            out[i] = b;
        } else {
            // Cache miss: recalcular en paralelo
            work.push_back({i, abs, rel, sz, mt});
        }
    }

    // Paralelizar los misses
    std::atomic<int> next{0};
    std::exception_ptr firstErr = nullptr;
    std::mutex errMtx;
    std::mutex cacheMtx; // proteger escritura en cache.byPath

    auto worker = [&]() {
        while (true) {
            int k = next.fetch_add(1);
            if (k >= static_cast<int>(work.size())) break;

            const auto& w = work[k];
            try {
                BBox b = readPlyBBox(w.abs, w.idx);
                b.file = w.abs;

                out[w.idx] = b;

                // Actualizar cache
                CacheEntry e;
                e.relPath = w.rel;
                e.fileSize = w.size;
                e.mtime = w.mt;
                e.bbox = b;

                std::lock_guard<std::mutex> lk(cacheMtx);
                cache.byPath[w.rel] = std::move(e);
                cacheWasUpdated = true;
            } catch (...) {
                std::lock_guard<std::mutex> lk(errMtx);
                if (!firstErr) firstErr = std::current_exception();
                return;
            }
        }
    };

    const int t = std::max(1, std::min(numThreads, (int)work.size()));
    std::vector<std::thread> threads;
    threads.reserve(t);
    for (int i = 0; i < t; ++i) threads.emplace_back(worker);
    for (auto& th : threads) th.join();

    if (firstErr) std::rethrow_exception(firstErr);

    // Guardar cache actualizado (solo si cambió algo)
    if (cacheWasUpdated) {
        if (!saveBBoxCache(cacheFile, cache)) {
            throw std::runtime_error("saveBBoxCache() devolvió false: " + cacheFile);
        }
    }
    
    return out;
}
