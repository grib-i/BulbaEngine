#include "bulba/core/math3v/polygon.h"

#include <stdlib.h>
#include <string.h>

static BLB_Vec3 poly_normal(BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c) {
  BLB_Vec3 normal = BLB_Cross(BLB_SubV3(b, a), BLB_SubV3(c, a));
  float length = BLB_LenV3(normal);

  if (length <= 0.000001f)
    return BLB_V3(0.0f, 1.0f, 0.0f);

  return BLB_MulV3F(normal, 1.0f / length);
}

static BLB_Vec3 safe_normalize(BLB_Vec3 value) {
  float length = BLB_LenV3(value);

  if (length <= 0.000001f)
    return BLB_V3(0.0f, 1.0f, 0.0f);

  return BLB_MulV3F(value, 1.0f / length);
}

static bool build_normals(BLB_Polygon3D *polygon) {
  if (!polygon || !polygon->vertices || !polygon->indices || polygon->vertex_count == 0)
    return false;

  if (polygon->normals)
    return true;

  BLB_Vec3 *normals = calloc(polygon->vertex_count, sizeof(*normals));

  if (!normals)
    return false;

  for (size_t i = 0; i + 2 < polygon->index_count; i += 3) {
    unsigned int ia = polygon->indices[i];
    unsigned int ib = polygon->indices[i + 1];
    unsigned int ic = polygon->indices[i + 2];

    if (ia >= polygon->vertex_count || ib >= polygon->vertex_count ||
        ic >= polygon->vertex_count) {
      free(normals);

      return false;
    }

    BLB_Vec3 normal =
        poly_normal(polygon->vertices[ia], polygon->vertices[ib], polygon->vertices[ic]);

    normals[ia] = BLB_AddV3(normals[ia], normal);
    normals[ib] = BLB_AddV3(normals[ib], normal);
    normals[ic] = BLB_AddV3(normals[ic], normal);
  }

  for (size_t i = 0; i < polygon->vertex_count; i++)
    normals[i] = safe_normalize(normals[i]);

  polygon->normals = normals;

  return true;
}

BLB_Polygon3D *BLB_CreatePolygon3D(BLB_Vec3 *vertices, size_t vertex_count,
                                   unsigned int *indices, size_t index_count) {
  if (!vertices)
    return NULL;

  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return NULL;

  polygon->vertices = vertices;
  polygon->vertex_count = vertex_count;
  polygon->indices = indices;
  polygon->index_count = index_count;

  polygon->base_vertices = malloc(sizeof(BLB_Vec3) * vertex_count);

  if (!polygon->base_vertices) {
    free(polygon);

    return NULL;
  }

  memcpy(polygon->base_vertices, vertices, sizeof(BLB_Vec3) * vertex_count);

  if (vertex_count >= 3)
    polygon->normal = poly_normal(vertices[0], vertices[1], vertices[2]);

  if (!build_normals(polygon)) {
    free(polygon->base_vertices);
    free(polygon);

    return NULL;
  }

  return polygon;
}

void BLB_DestroyPolygon3D(BLB_Polygon3D *polygon) {
  if (!polygon)
    return;

  free(polygon->normals);
  free(polygon->base_vertices);
  free(polygon);
}

void BLB_TriangulatePolygon3D(BLB_Polygon3D *polygon) {
  (void)polygon;
}

BLB_Polygon2D *BLB_CreatePolygon2D(BLB_Vec2 *vertices, size_t vertex_count,
                                   unsigned int *indices, size_t index_count) {
  if (!vertices)
    return NULL;

  BLB_Polygon2D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return NULL;

  polygon->vertices = vertices;
  polygon->vertex_count = vertex_count;
  polygon->indices = indices;
  polygon->index_count = index_count;

  polygon->base_vertices = malloc(sizeof(BLB_Vec2) * vertex_count);

  if (!polygon->base_vertices) {
    free(polygon);

    return NULL;
  }

  memcpy(polygon->base_vertices, vertices, sizeof(BLB_Vec2) * vertex_count);

  return polygon;
}

void BLB_DestroyPolygon2D(BLB_Polygon2D *polygon) {
  if (!polygon)
    return;

  free(polygon->base_vertices);
  free(polygon);
}

void BLB_TriangulatePolygon2D(BLB_Polygon2D *polygon) {
  (void)polygon;
}
