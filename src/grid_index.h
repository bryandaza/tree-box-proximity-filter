#pragma once
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "bbox.h"

struct CellKey {
    int gx, gy;
    bool operator==(const CellKey& o) const { return gx == o.gx && gy == o.gy; }
};

struct CellKeyHash {
    std::size_t operator()(const CellKey& k) const noexcept {
        // hash simple y rápido
        return (static_cast<std::size_t>(static_cast<uint32_t>(k.gx)) << 32)
             ^ static_cast<std::size_t>(static_cast<uint32_t>(k.gy));
    }
};

class GridIndex {
public:
    explicit GridIndex(double cellSize);

    void build(const std::vector<BBox>& boxes);

    std::vector<int> queryCandidates(const BBox& queryBox) const;
    
    const std::unordered_map<CellKey, std::vector<int>, CellKeyHash>& cells() const {
    return grid_;
}


private:
    double cell_;
    std::unordered_map<CellKey, std::vector<int>, CellKeyHash> grid_;

    // NUEVO:
    mutable std::vector<uint32_t> visited_;
    mutable uint32_t stamp_ = 1;

    static int cellCoord(double v, double cellSize);
    void insertBox(const BBox& b);
};
