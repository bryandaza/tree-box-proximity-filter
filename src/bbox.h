#pragma once
#include <string>

struct BBox {
    double minX, maxX;
    double minY, maxY;
    double minZ, maxZ;
    int id = -1;
    std::string file;

    double cx() const { return 0.5 * (minX + maxX); }
    double cy() const { return 0.5 * (minY + maxY); }
    double height() const { return maxZ - minZ; }
};

BBox expandXY(const BBox& b, double buffer);

bool overlapsXY(const BBox& a, const BBox& b, double eps = 0.0);
bool overlaps3D(const BBox& a, const BBox& b, double eps = 0.0);
bool near3D_cm(const BBox& a, const BBox& b, double minOverlapZ, double maxGapXY);
bool nearXY_and_overlapZ(const BBox& a, const BBox& b,
                         double maxGapXY, double minOverlapZ);

