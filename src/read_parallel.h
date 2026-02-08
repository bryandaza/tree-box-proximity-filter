#pragma once
#include <vector>
#include <string>
#include "bbox.h"

std::vector<BBox> readAllBBoxesParallel(const std::vector<std::string>& files, int numThreads);
