#include "bbox.h"

static inline double overlap1D(double aMin, double aMax, double bMin, double bMax) {
    return std::max(0.0, std::min(aMax, bMax) - std::max(aMin, bMin));
}

static inline double gap1D(double aMin, double aMax, double bMin, double bMax) {
    if (aMax < bMin) return bMin - aMax;
    if (bMax < aMin) return aMin - bMax;
    return 0.0; // se solapan (o tocan)
}

BBox expandXY(const BBox& b, double buffer) {
    BBox out = b;
    out.minX -= buffer; out.maxX += buffer;
    out.minY -= buffer; out.maxY += buffer;
    return out;
}

bool overlapsXY(const BBox& a, const BBox& b, double eps) {
    const bool ox = (a.minX <= b.maxX + eps) && (b.minX <= a.maxX + eps);
    const bool oy = (a.minY <= b.maxY + eps) && (b.minY <= a.maxY + eps);
    return ox && oy;
}

bool overlaps3D(const BBox& a, const BBox& b, double eps) {
    const bool ox = (a.minX <= b.maxX + eps) && (b.minX <= a.maxX + eps);
    const bool oy = (a.minY <= b.maxY + eps) && (b.minY <= a.maxY + eps);
    const bool oz = (a.minZ <= b.maxZ + eps) && (b.minZ <= a.maxZ + eps);
    return ox && oy && oz;
}

bool near3D_cm(const BBox& a, const BBox& b, double minOverlapZ, double maxGapXY) {
    const double oz = overlap1D(a.minZ, a.maxZ, b.minZ, b.maxZ);
    if (oz < minOverlapZ) return false; // evita el "debajo"

    const double gx = gap1D(a.minX, a.maxX, b.minX, b.maxX);
    const double gy = gap1D(a.minY, a.maxY, b.minY, b.maxY);

    return (gx <= maxGapXY) && (gy <= maxGapXY);
}

bool nearXY_and_overlapZ(const BBox& a, const BBox& b,
                         double maxGapXY, double minOverlapZ) {
    const double gx = gap1D(a.minX, a.maxX, b.minX, b.maxX);
    const double gy = gap1D(a.minY, a.maxY, b.minY, b.maxY);
    if (gx > maxGapXY || gy > maxGapXY) return false;

    const double oz = overlap1D(a.minZ, a.maxZ, b.minZ, b.maxZ);
    return oz >= minOverlapZ;
}
