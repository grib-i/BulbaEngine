#ifndef BULBA_CORE_OBJECTS3D_POLYGON3D_H
#define BULBA_CORE_OBJECTS3D_POLYGON3D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/utils/object.h"

BLB_Object3D *BLB_CreatePolygon3D(BLB_Vec3 *vertices, size_t vertex_count, unsigned int *indices, size_t index_count);
void BLB_TriangulatePolygon3D(BLB_Object3D *polygon);

#endif
