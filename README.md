# tree-box-proximity-filter

## A C++ Tool for 3D Proximity Analysis of Segmented Tree Point Clouds Using Bounding Boxes

---

## Abstract

This repository presents **tree-box-proximity-filter**, a C++ tool designed to identify **spatial proximity and potential overlap between individual trees** represented as segmented point clouds.  
The method operates directly on three-dimensional data using **axis-aligned bounding boxes (AABB)** and is optimized for large Terrestrial Laser Scanning (TLS) or LiDAR datasets.

The tool enables centimeter-scale proximity analysis, supports parallel execution, and incorporates persistent caching to reduce computational overhead. Additionally, it provides an automated mechanism to extract **vertical context slices** from unclassified point clouds to facilitate visual validation of detected cases.

---

## 1. Motivation

In forest structure analysis, individual-tree segmentation often produces thousands of point clouds that must be validated for:

- spatial redundancy or duplication,
- mis-segmentation due to crown proximity,
- vertically stacked trees (e.g., understory beneath canopy),
- quality control prior to ecological or biometric analysis.

Brute-force point-to-point comparison is computationally prohibitive for large datasets.  
This tool addresses the problem by operating on **bounding boxes** as a first-order geometric abstraction, drastically reducing complexity while preserving spatial interpretability.

---

## 2. Methodological Overview

The workflow implemented in **tree-box-proximity-filter** follows these steps:

1. **Input representation**  
   Each tree is provided as an independent `.ply` point cloud file.

2. **Bounding Box computation**  
   For each tree, a 3D axis-aligned bounding box (BBox) is computed.

3. **Internal normalization**  
   Coordinates are internally normalized (translation only) to improve numerical stability.  
   Original point clouds remain unmodified.

4. **Spatial indexing**  
   A two-dimensional grid index (XY) is constructed to limit candidate comparisons.

5. **Proximity criteria**  
   Two trees are considered *candidates* if:
   - their horizontal separation (XY) is below a user-defined threshold, and
   - they share a minimum vertical overlap in Z.

6. **Exclusion of unclassified cloud**  
   The unclassified point cloud (`*_segmented_-1.ply`) is excluded from proximity analysis.

7. **Contextual extraction**  
   For visual inspection, a vertical slice of the unclassified cloud is extracted using the exact XY extent of a selected tree.

---

## 3. Expected Data Structure

```text
Data/
└── B04_individual_pcs/
    ├── AMA_B04_2cm_5987_159648_segmented_0.ply
    ├── AMA_B04_2cm_5987_159648_segmented_1.ply
    ├── AMA_B04_2cm_5987_159648_segmented_2.ply
    ├── ...
    ├── AMA_B04_2cm_5987_159648_segmented_-1.ply   # unclassified cloud
```

The file with suffix `segmented_-1.ply` represents the non-classified points and is used **only** for contextual visualization.

---

## 4. Compilation

### 4.1 Requirements

- C++17 or newer
- CMake ≥ 3.15
- Compiler:
  - Windows: MSVC (Visual Studio 2022 recommended)
  - Linux: GCC or Clang

---

### 4.2 Windows (PowerShell)

```powershell
git clone https://github.com/USUARIO/tree-box-proximity-filter.git
cd tree-box-proximity-filter

cmake -S . -B build
cmake --build build --config Release
```

Executable:
```text
build/Release/tree-box-proximity-filter.exe
```

---

### 4.3 Linux

```bash
git clone https://github.com/USUARIO/tree-box-proximity-filter.git
cd tree-box-proximity-filter

cmake -S . -B build
cmake --build build -j
```

Executable:
```text
build/tree-box-proximity-filter
```

---

## 5. Usage

### 5.1 Individual Tree Analysis

```bash
tree-box-proximity-filter \
  --input Data/B04_individual_pcs \
  --tree AMA_B04_2cm_5987_159648_segmented_0.ply \
  --buffer 0.2 \
  --cell 1.0 \
  --gap-xy 0.04 \
  --min-overlap-z 0.04 \
  --threads 8
```

This mode:
- identifies trees within a horizontal tolerance of **4 cm**,
- enforces a minimum **4 cm vertical overlap**,
- extracts a vertical slice from the unclassified cloud using the tree’s exact XY extent.

The extracted file is saved in absolute coordinates:

```text
Data/B04_individual_pcs/crops_minus1/
└── AMA_B04_2cm_5987_159648_segmented_0_-1.ply
```

---

### 5.2 Batch Processing (All Trees)

```bash
tree-box-proximity-filter \
  --input Data/B04_individual_pcs \
  --all \
  --cell 1.0 \
  --gap-xy 0.04 \
  --min-overlap-z 0.04 \
  --threads 8 \
  --out overlaps.csv
```

---

## 6. Output Format

### 6.1 CSV Output

Each row corresponds to a pair of trees identified as spatially proximate.

| Column | Description |
|------|-------------|
| tree_index_a | Internal index of tree A |
| tree_index_b | Internal index of tree B |
| file_a | Filename of tree A |
| file_b | Filename of tree B |
| height_a | Tree height (maxZ − minZ) |
| height_b | Tree height (maxZ − minZ) |
| gap_xy | Minimum horizontal distance |
| overlap_z | Vertical overlap |

---

## 7. Performance Considerations

The implementation includes:

- Parallel computation of bounding boxes
- Persistent on-disk cache (`bboxes_cache.bin`)
- Grid-based spatial indexing (avoids O(n²) comparisons)
- Memory-efficient processing of large point clouds

The tool is suitable for datasets containing **thousands of trees and millions of points**.

---

## 8. Applications

- Quality control of individual-tree segmentation
- Detection of duplicated or ambiguous tree objects
- Identification of understory trees beneath dominant canopies
- Preprocessing for ecological, structural, or biomass analyses
- Visual validation in CloudCompare or similar software

---

## 9. Dependencies

- **tinyply** (included in `external/`)
- C++ Standard Library

---

## 10. Notes

- Original point clouds are never modified.
- All proximity checks are based on bounding boxes, not point-wise distances.
- Unclassified cloud extraction is intended solely for visual validation.

---

## 11. Authorship

Developed as a research-oriented tool for advanced TLS point cloud analysis  
and three-dimensional proximity assessment of individual trees.

---

## Citation

If you use this software in academic work, please cite the repository:

```
tree-box-proximity-filter. A C++ tool for 3D proximity analysis of segmented tree point clouds using bounding boxes.
```

