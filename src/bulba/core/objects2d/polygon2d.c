#include "bulba/core/objects2d/polygon2d.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static BLB_Polygon2D *polygon = NULL;

static void BLB_SquareFreePolygon(void) {
  if (polygon == NULL)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);

  polygon = NULL;
}

BLB_Object2D *BLB_CreatePolygon2D(BLB_Vec2 *vertices, size_t vertex_count, unsigned int *indices, size_t index_count) {
  if (!vertices)
    return NULL;

  BLB_Object2D *polygon = calloc(1, sizeof(*polygon));
  if (!polygon)
    return NULL;

  polygon->polygon->vertices = vertices;
  polygon->polygon->vertex_count = vertex_count;
  polygon->polygon->indices = indices;
  polygon->polygon->index_count = index_count;

  polygon->polygon->base_vertices = malloc(sizeof(BLB_Vec2) * vertex_count);

  if (!polygon->polygon->base_vertices) {
    free(polygon);
    return NULL;
  }

  memcpy(polygon->polygon->base_vertices, vertices, sizeof(BLB_Vec2) * vertex_count);
  return polygon;
}

void BLB_DestroyPolygon2D(BLB_Object2D *polygon) {
  if (!polygon)
    return;

  free(polygon->polygon->base_vertices);
  free(polygon);
}

void BLB_TriangulatePolygon2D(BLB_Object2D *polygon) {
  (void)polygon;
}
