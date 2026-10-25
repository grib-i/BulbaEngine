#ifndef BLB_MESH_SIMPLIFICATION_H
#define BLB_MESH_SIMPLIFICATION_H

#include "bulba/core/math3v/polygon.h"

#include <stdbool.h>
#include <stddef.h>

Mesh *BLB_MeshSimplifyQEM(const Mesh *mesh, float max_error);
Mesh *BLB_MeshSimplifyQEMWithReduction(const Mesh *mesh, float max_error, float reduction);

bool BLB_MeshBuildHSA(const Mesh *source, int levels, float error, Mesh ***lods, size_t *lod_count);

void BLB_MeshDestroy(Mesh *mesh);

void BLB_MeshDestroyLODChain(Mesh **lods, size_t lod_count);

#endif
