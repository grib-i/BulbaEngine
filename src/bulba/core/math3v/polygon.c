#include "bulba/core/math3v/polygon.h"
#include <stdlib.h>
#include <string.h>

static HMM_Vec3 poly_normal(HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c) {
  HMM_Vec3 n = HMM_Cross(HMM_SubV3(b, a), HMM_SubV3(c, a));
  return HMM_NormV3(n);
}

BLB_Polygon3D *BLB_CreatePolygon3D(HMM_Vec3 *vertices, size_t *vertex_count, unsigned int *indices, size_t index_count) {
  if (!vertices || !vertex_count)
    return NULL;
  BLB_Polygon3D *p = calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  size_t n = *vertex_count;
  p->vertices = vertices;
  p->vertex_count = vertex_count;
  p->indices = indices;
  p->index_count = index_count;
  p->base_vertices = malloc(sizeof(HMM_Vec3) * n);

  if (!p->base_vertices) {
    free(p);
    return NULL;
  }

  memcpy(p->base_vertices, vertices, sizeof(HMM_Vec3) * n);

  if (n >= 3)
    p->normal = poly_normal(vertices[0], vertices[1], vertices[2]);
  return p;
}

void BLB_DestroyPolygon3D(BLB_Polygon3D *p) {
  if (!p)
    return;
  free(p->base_vertices);
  free(p);
}

void BLB_TriangulatePolygon3D(BLB_Polygon3D *p) { (void)p; }

BLB_Polygon2D *BLB_CreatePolygon2D(HMM_Vec2 *vertices, size_t *vertex_count, unsigned int *indices, size_t index_count) {
  if (!vertices || !vertex_count)
    return NULL;
  BLB_Polygon2D *p = calloc(1, sizeof(*p));
  if (!p)
    return NULL;
  size_t n = *vertex_count;
  p->vertices = vertices;
  p->vertex_count = vertex_count;
  p->indices = indices;
  p->index_count = index_count;
  p->base_vertices = malloc(sizeof(HMM_Vec2) * n);
  if (!p->base_vertices) {
    free(p);
    return NULL;
  }
  memcpy(p->base_vertices, vertices, sizeof(HMM_Vec2) * n);
  return p;
}

void BLB_DestroyPolygon2D(BLB_Polygon2D *p) {
  if (!p)
    return;
  free(p->base_vertices);
  free(p);
}

void BLB_TriangulatePolygon2D(BLB_Polygon2D *p) { (void)p; }
