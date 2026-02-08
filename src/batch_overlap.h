#pragma once
#include <vector>
#include <string>
#include "bbox.h"
#include "grid_index.h"

struct OverlapPair {
    int a;
    int b;
};

std::vector<OverlapPair> findAllOverlapsByCells(
    const std::vector<BBox>& boxes,
    const GridIndex& index,
    int threads,
    double maxGapXY,
    double minOverlapZ
);

bool savePairsCSV(
    const std::string& outCsv,
    const std::vector<BBox>& boxes,
    const std::vector<OverlapPair>& pairs
);

bool savePairsTXT(
    const std::string& outTxt,
    const std::vector<BBox>& boxes,
    const std::vector<OverlapPair>& pairs
);
