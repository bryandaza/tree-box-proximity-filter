#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "bbox.h"

struct CacheEntry {
    std::string relPath;     // path relativo al input dir
    uint64_t fileSize = 0;
    int64_t  mtime = 0;      // last write time (en ticks/segundos)
    BBox bbox;
};

struct CacheData {
    uint32_t version = 1;
    std::unordered_map<std::string, CacheEntry> byPath; // key = relPath
};

bool loadBBoxCache(const std::string& cacheFile, CacheData& cache);
bool saveBBoxCache(const std::string& cacheFile, const CacheData& cache);

// Construye bboxes usando cache + recalculo solo de cambiados
std::vector<BBox> buildBBoxesWithCache(
    const std::string& inputDir,
    const std::vector<std::string>& absFiles,
    int numThreads,
    const std::string& cacheFile,
    bool& cacheWasUpdated
);
