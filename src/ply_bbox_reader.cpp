#include "ply_bbox_reader.h"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "tinyply.h"

static void initBBox(BBox& b) {
    b.minX = b.minY = b.minZ =  std::numeric_limits<double>::infinity();
    b.maxX = b.maxY = b.maxZ = -std::numeric_limits<double>::infinity();
}

template <typename T>
static void updateMinMax(double& mn, double& mx, T v) {
    const double dv = static_cast<double>(v);
    if (dv < mn) mn = dv;
    if (dv > mx) mx = dv;
}

BBox readPlyBBox(const std::string& filepath, int id) {
    std::ifstream ss(filepath, std::ios::binary);
    if (!ss) throw std::runtime_error("No pude abrir: " + filepath);

    tinyply::PlyFile file;
    file.parse_header(ss);

    // Pedimos x,y,z del elemento "vertex"
    std::shared_ptr<tinyply::PlyData> vertices;
    try {
        vertices = file.request_properties_from_element("vertex", { "x", "y", "z" });
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("PLY sin x,y,z en vertex: ") + e.what());
    }

    file.read(ss);

    if (!vertices || vertices->count == 0) {
        throw std::runtime_error("PLY sin vertices: " + filepath);
    }

    BBox b;
    b.id = id;
    b.file = filepath;
    initBBox(b);

    // vertices->buffer contiene xyz intercalado: x y z x y z ...
    // El tipo puede variar (float/double/etc). Tinyply nos da vertices->t
    const auto t = vertices->t;
    const size_t count = vertices->count;

    const uint8_t* raw = vertices->buffer.get();
    const size_t bytes_per_row = vertices->buffer.size_bytes() / count;

    // Interpretación por tipo
    auto process = [&](auto dummy) {
        using Scalar = decltype(dummy);
        for (size_t i = 0; i < count; ++i) {
            const Scalar* row = reinterpret_cast<const Scalar*>(raw + i * bytes_per_row);
            // row[0]=x row[1]=y row[2]=z
            updateMinMax(b.minX, b.maxX, row[0]);
            updateMinMax(b.minY, b.maxY, row[1]);
            updateMinMax(b.minZ, b.maxZ, row[2]);
        }
    };

    switch (t) {
    case tinyply::Type::FLOAT32: process(float{}); break;
    case tinyply::Type::FLOAT64: process(double{}); break;
    case tinyply::Type::INT32:   process(int32_t{}); break;
    case tinyply::Type::UINT32:  process(uint32_t{}); break;
    case tinyply::Type::INT16:   process(int16_t{}); break;
    case tinyply::Type::UINT16:  process(uint16_t{}); break;
    case tinyply::Type::INT8:    process(int8_t{}); break;
    case tinyply::Type::UINT8:   process(uint8_t{}); break;
    default:
        throw std::runtime_error("Tipo de vertex no soportado en: " + filepath);
    }

    return b;
}
