#ifndef BULBA_CORE_MATH3V_POLYGON_H
#define BULBA_CORE_MATH3V_POLYGON_H

#include "bulba/core/math3v/math3v.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  BLB_Vec3 *vertices;
  BLB_Vec3 *normals;
  BLB_Vec2 *uvs;
  size_t vertex_count;
  unsigned int *indices;
  size_t index_count;
} Mesh;

typedef struct {
  BLB_Vec3 *vertices;
  BLB_Vec3 *base_vertices;
  BLB_Vec3 *normals;
  BLB_Vec2 *uvs;
  size_t vertex_count;
  unsigned int *indices;
  size_t index_count;
  BLB_Vec3 normal;
  uint64_t geometry_hash;
  bool dynamic_vertices;
  float bounds_radius;
} BLB_Polygon3D;

typedef struct {
  BLB_Vec2 *vertices;
  BLB_Vec2 *base_vertices;
  BLB_Vec2 *uvs;
  size_t vertex_count;
  unsigned int *indices;
  size_t index_count;
  uint64_t geometry_hash;
} BLB_Polygon2D;

BLB_Polygon3D *BLB_CreatePolygon3D(BLB_Vec3 *vertices, size_t vertex_count,
                                   unsigned int *indices, size_t index_count);

void BLB_DestroyPolygon3D(BLB_Polygon3D *polygon);

void BLB_TriangulatePolygon3D(BLB_Polygon3D *polygon);

BLB_Polygon2D *BLB_CreatePolygon2D(BLB_Vec2 *vertices, size_t vertex_count,
                                   unsigned int *indices, size_t index_count);

void BLB_DestroyPolygon2D(BLB_Polygon2D *polygon);

void BLB_TriangulatePolygon2D(BLB_Polygon2D *polygon);

#endif
