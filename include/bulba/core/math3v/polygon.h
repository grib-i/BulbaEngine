#ifndef BULBA_CORE_MATH3V_POLYGON_H
#define BULBA_CORE_MATH3V_POLYGON_H

#include "bulba/core/math3v/math3v.h"

#include <stddef.h>

typedef struct {
  HMM_Vec3 *vertices;
  HMM_Vec3 *normals;
  HMM_Vec2 *uvs;
  size_t vertex_count;
  unsigned int *indices;
  size_t index_count;
} Mesh;

typedef struct {
  HMM_Vec3 *vertices;
  HMM_Vec3 *base_vertices;
  HMM_Vec2 *uvs;
  size_t *vertex_count;
  unsigned int *indices;
  size_t index_count;
  HMM_Vec3 normal;
} BLB_Polygon3D;

typedef struct {
  HMM_Vec2 *vertices;
  HMM_Vec2 *base_vertices;
  HMM_Vec2 *uvs;
  size_t *vertex_count;
  unsigned int *indices;
  size_t index_count;
} BLB_Polygon2D;

BLB_Polygon3D *BLB_CreatePolygon3D(HMM_Vec3 *vertices, size_t *vertex_count, unsigned int *indices, size_t index_count);

void BLB_DestroyPolygon3D(BLB_Polygon3D *polygon);

void BLB_TriangulatePolygon3D(BLB_Polygon3D *polygon);

BLB_Polygon2D *BLB_CreatePolygon2D(HMM_Vec2 *vertices, size_t *vertex_count, unsigned int *indices, size_t index_count);

void BLB_DestroyPolygon2D(BLB_Polygon2D *polygon);

void BLB_TriangulatePolygon2D(BLB_Polygon2D *polygon);

#endif
