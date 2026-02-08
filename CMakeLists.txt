cmake_minimum_required(VERSION 3.16)
project(overlap_trees LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(overlap_trees
    src/main.cpp
    src/bbox.cpp
    src/grid_index.cpp
    src/ply_bbox_reader.cpp
    src/read_parallel.cpp
    external/tinyply.cpp
    src/cache_bbox.cpp
    src/read_parallel.cpp
    src/batch_overlap.cpp
    src/crop_minus1.cpp
)

target_include_directories(overlap_trees PRIVATE
    src
    external
)

# Opcional: más warnings (recomendado para aprender)
# Warnings + optimización por compilador
if (MSVC)
  # Warnings
  target_compile_options(overlap_trees PRIVATE /W4)

  # Optimización fuerte
  target_compile_options(overlap_trees PRIVATE /O2 /GL)

  # Link Time Optimization
  target_link_options(overlap_trees PRIVATE /LTCG)

else()
  # Warnings en GCC / Clang
  target_compile_options(overlap_trees PRIVATE -Wall -Wextra -Wpedantic)
endif()