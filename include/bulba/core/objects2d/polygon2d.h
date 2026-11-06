#ifndef BULBA_CORE_OBJECTS2D_POLYGON2D_H
#define BULBA_CORE_OBJECTS2D_POLYGON2D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/utils/object.h"

BLB_Object2D *BLB_CreatePolygon2D(BLB_Vec2 *vertices, size_t vertex_count, unsigned int *indices, size_t index_count);
void BLB_TriangulatePolygon2D(BLB_Object2D *polygon);

#endif
