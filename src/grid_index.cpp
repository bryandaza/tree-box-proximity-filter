#include "grid_index.h"
#include <cmath>
#include <unordered_set>

GridIndex::GridIndex(double cellSize) : cell_(cellSize) {}

int GridIndex::cellCoord(double v, double cellSize) {
    return static_cast<int>(std::floor(v / cellSize));
}

void GridIndex::insertBox(const BBox& b) {
    const int gx0 = cellCoord(b.minX, cell_);
    const int gx1 = cellCoord(b.maxX, cell_);
    const int gy0 = cellCoord(b.minY, cell_);
    const int gy1 = cellCoord(b.maxY, cell_);

    for (int gx = gx0; gx <= gx1; ++gx) {
        for (int gy = gy0; gy <= gy1; ++gy) {
            grid_[CellKey{gx, gy}].push_back(b.id);
        }
    }
}

void GridIndex::build(const std::vector<BBox>& boxes) {
    visited_.assign(boxes.size(), 0);
    stamp_ = 1;

    grid_.clear();
    grid_.reserve(boxes.size() * 2);

    for (const auto& b : boxes) {
        insertBox(b);
    }
}

std::vector<int> GridIndex::queryCandidates(const BBox& q) const {
    const int gx0 = cellCoord(q.minX, cell_);
    const int gx1 = cellCoord(q.maxX, cell_);
    const int gy0 = cellCoord(q.minY, cell_);
    const int gy1 = cellCoord(q.maxY, cell_);

    // evitar overflow (muy raro, pero correcto)
    if (stamp_ == 0) {
        std::fill(visited_.begin(), visited_.end(), 0);
        stamp_ = 1;
    }
    const uint32_t myStamp = stamp_++;

    std::vector<int> out;
    out.reserve(128);

    for (int gx = gx0; gx <= gx1; ++gx) {
        for (int gy = gy0; gy <= gy1; ++gy) {
            auto it = grid_.find(CellKey{gx, gy});
            if (it == grid_.end()) continue;

            const auto& ids = it->second;
            for (int id : ids) {
                if (visited_[id] != myStamp) {
                    visited_[id] = myStamp;
                    out.push_back(id);
                }
            }
        }
    }
    return out;
}

