#pragma once
#include <string>
#include "bbox.h"

BBox readPlyBBox(const std::string& filepath, int id);
