#include "bulba/core/objects3d/model_loader.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/object.h"

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLB_MODEL_CACHE_INITIAL_CAPACITY 8u
#define BLB_MODEL_LINE_SIZE 16384u

typedef struct {
  char *path;
  BLB_Polygon3D *polygon;
  uint64_t geometry_id;
  size_t refs;
} BLB_ModelGeometry;

typedef struct {
  HMM_Vec3 position;
  HMM_Vec2 uv;
  HMM_Vec3 normal;
  bool has_uv;
  bool has_normal;
} BLB_OBJFaceVertex;

typedef struct {
  HMM_Vec3 *data;
  size_t count;
  size_t capacity;
} BLB_OBJVec3Array;

typedef struct {
  HMM_Vec2 *data;
  size_t count;
  size_t capacity;
} BLB_OBJVec2Array;

typedef struct {
  BLB_OBJFaceVertex *data;
  size_t count;
  size_t capacity;
} BLB_OBJFaceArray;

static BLB_ModelGeometry *model_cache = NULL;
static size_t model_cache_count = 0;
static size_t model_cache_capacity = 0;

static bool BLB_ModelArrayReserve(void **data, size_t *capacity, size_t required, size_t element_size) {
  if (*capacity >= required)
    return true;

  size_t new_capacity = *capacity ? *capacity : 8u;

  while (new_capacity < required) {
    if (new_capacity > SIZE_MAX / 2u)
      return false;

    new_capacity *= 2u;
  }

  if (new_capacity > SIZE_MAX / element_size)
    return false;

  void *new_data = realloc(*data, new_capacity * element_size);

  if (!new_data)
    return false;

  *data = new_data;
  *capacity = new_capacity;

  return true;
}

static char *BLB_ModelDuplicateString(const char *string) {
  if (!string)
    return NULL;

  size_t length = strlen(string);
  char *copy = malloc(length + 1u);

  if (!copy)
    return NULL;

  memcpy(copy, string, length + 1u);

  return copy;
}

static uint64_t BLB_ModelHashPath(const char *path) {
  uint64_t hash = UINT64_C(1469598103934665603);

  for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
    hash ^= (uint64_t)*p;
    hash *= UINT64_C(1099511628211);
  }

  return hash ? hash : UINT64_C(1);
}

static void BLB_ModelFreePolygon(BLB_Polygon3D *polygon) {
  if (!polygon)
    return;

  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->normals);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
}

static bool BLB_ModelCacheReserve(size_t required) {
  if (model_cache_capacity >= required)
    return true;

  size_t capacity = model_cache_capacity ? model_cache_capacity : BLB_MODEL_CACHE_INITIAL_CAPACITY;

  while (capacity < required) {
    if (capacity > SIZE_MAX / 2u)
      return false;

    capacity *= 2u;
  }

  if (capacity > SIZE_MAX / sizeof(*model_cache))
    return false;

  BLB_ModelGeometry *new_cache = realloc(model_cache, capacity * sizeof(*model_cache));

  if (!new_cache)
    return false;

  model_cache = new_cache;
  model_cache_capacity = capacity;

  return true;
}

static BLB_ModelGeometry *BLB_ModelCacheFind(const char *path) {
  if (!path)
    return NULL;

  for (size_t i = 0; i < model_cache_count; ++i) {
    if (strcmp(model_cache[i].path, path) == 0)
      return &model_cache[i];
  }

  return NULL;
}

static BLB_ModelGeometry *BLB_ModelCacheAdd(const char *path, BLB_Polygon3D *polygon) {
  if (!path || !polygon)
    return NULL;

  if (!BLB_ModelCacheReserve(model_cache_count + 1u))
    return NULL;

  char *path_copy = BLB_ModelDuplicateString(path);

  if (!path_copy)
    return NULL;

  BLB_ModelGeometry *geometry = &model_cache[model_cache_count++];

  geometry->path = path_copy;
  geometry->polygon = polygon;
  geometry->geometry_id = BLB_ModelHashPath(path);
  geometry->refs = 1u;

  return geometry;
}

static void BLB_ModelCacheRetain(BLB_ModelGeometry *geometry) {
  if (!geometry)
    return;

  ++geometry->refs;
}

static void BLB_ModelCacheRelease(BLB_Object3D *object) {
  if (!object || !object->polygon)
    return;

  for (size_t i = 0; i < model_cache_count; ++i) {
    BLB_ModelGeometry *geometry = &model_cache[i];

    if (geometry->polygon != object->polygon)
      continue;

    if (geometry->refs > 0)
      --geometry->refs;

    if (geometry->refs != 0)
      return;

    BLB_ModelFreePolygon(geometry->polygon);
    free(geometry->path);

    size_t last = model_cache_count - 1u;

    if (i != last)
      model_cache[i] = model_cache[last];

    --model_cache_count;

    return;
  }
}

static bool BLB_OBJVec3Push(BLB_OBJVec3Array *array, HMM_Vec3 value) {
  if (!BLB_ModelArrayReserve((void **)&array->data, &array->capacity, array->count + 1u, sizeof(*array->data)))
    return false;

  array->data[array->count++] = value;
  return true;
}

static bool BLB_OBJVec2Push(BLB_OBJVec2Array *array, HMM_Vec2 value) {
  if (!BLB_ModelArrayReserve((void **)&array->data, &array->capacity, array->count + 1u, sizeof(*array->data)))
    return false;

  array->data[array->count++] = value;
  return true;
}

static bool BLB_OBJFacePush(BLB_OBJFaceArray *array, BLB_OBJFaceVertex value) {
  if (!BLB_ModelArrayReserve((void **)&array->data, &array->capacity, array->count + 1u, sizeof(*array->data)))
    return false;

  array->data[array->count++] = value;
  return true;
}

static bool BLB_OBJResolveIndex(long index, size_t count, size_t *result) {
  if (index == 0)
    return false;

  if (index > 0) {
    if ((unsigned long)index > count)
      return false;

    *result = (size_t)(index - 1);
    return true;
  }

  long resolved = (long)count + index;

  if (resolved < 0 || (size_t)resolved >= count)
    return false;

  *result = (size_t)resolved;

  return true;
}

static bool BLB_OBJParseFaceVertex(const char *token, const BLB_OBJVec3Array *positions, const BLB_OBJVec2Array *uvs, const BLB_OBJVec3Array *normals,
                                   BLB_OBJFaceVertex *result) {
  char *end = NULL;
  const char *cursor = token;
  long position_index;
  size_t resolved_position = 0;

  memset(result, 0, sizeof(*result));

  position_index = strtol(cursor, &end, 10);

  if (end == cursor || position_index == 0)
    return false;

  if (!BLB_OBJResolveIndex(position_index, positions->count, &resolved_position))
    return false;

  result->position = positions->data[resolved_position];

  cursor = end;

  if (*cursor == '/') {
    ++cursor;

    if (*cursor != '/') {
      long uv_index;
      size_t resolved_uv = 0;

      uv_index = strtol(cursor, &end, 10);

      if (end == cursor)
        return false;

      if (!BLB_OBJResolveIndex(uv_index, uvs->count, &resolved_uv))
        return false;

      result->uv = uvs->data[resolved_uv];
      result->has_uv = true;

      cursor = end;
    }

    if (*cursor == '/') {
      long normal_index;
      size_t resolved_normal = 0;

      ++cursor;

      normal_index = strtol(cursor, &end, 10);

      if (end == cursor)
        return false;

      if (!BLB_OBJResolveIndex(normal_index, normals->count, &resolved_normal))
        return false;

      result->normal = normals->data[resolved_normal];
      result->has_normal = true;

      cursor = end;
    }
  }

  return *cursor == '\0';
}

static HMM_Vec3 BLB_OBJCalculateFaceNormal(HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c) {
  HMM_Vec3 ab = HMM_V3(b.x - a.x, b.y - a.y, b.z - a.z);
  HMM_Vec3 ac = HMM_V3(c.x - a.x, c.y - a.y, c.z - a.z);
  HMM_Vec3 cross = HMM_V3(ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x);
  float length_squared = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;

  if (length_squared <= 0.00000001f)
    return HMM_V3(0.0f, 1.0f, 0.0f);

  return HMM_NormV3(cross);
}

static BLB_Polygon3D *BLB_LoadOBJPolygon(const char *path) {
  FILE *file = fopen(path, "r");

  if (!file)
    return NULL;

  BLB_Polygon3D *polygon = calloc(1, sizeof(*polygon));

  if (!polygon) {
    fclose(file);
    return NULL;
  }

  BLB_OBJVec3Array positions = {0};
  BLB_OBJVec2Array uvs = {0};
  BLB_OBJVec3Array normals = {0};
  BLB_OBJFaceArray face = {0};

  char line[BLB_MODEL_LINE_SIZE];
  bool failed = false;

  while (fgets(line, sizeof(line), file)) {
    char *cursor = line;

    while (isspace((unsigned char)*cursor))
      ++cursor;

    if (*cursor == '\0' || *cursor == '#')
      continue;

    if (cursor[0] == 'v' && isspace((unsigned char)cursor[1])) {
      float x;
      float y;
      float z;

      if (sscanf(cursor + 1, "%f %f %f", &x, &y, &z) != 3) {
        failed = true;
        break;
      }

      if (!BLB_OBJVec3Push(&positions, HMM_V3(x, y, z))) {
        failed = true;
        break;
      }

      continue;
    }

    if (cursor[0] == 'v' && cursor[1] == 't' && isspace((unsigned char)cursor[2])) {
      float u;
      float v;

      if (sscanf(cursor + 2, "%f %f", &u, &v) < 2) {
        failed = true;
        break;
      }

      if (!BLB_OBJVec2Push(&uvs, HMM_V2(u, v))) {
        failed = true;
        break;
      }

      continue;
    }

    if (cursor[0] == 'v' && cursor[1] == 'n' && isspace((unsigned char)cursor[2])) {
      float x;
      float y;
      float z;

      if (sscanf(cursor + 2, "%f %f %f", &x, &y, &z) != 3) {
        failed = true;
        break;
      }

      if (!BLB_OBJVec3Push(&normals, HMM_V3(x, y, z))) {
        failed = true;
        break;
      }

      continue;
    }

    if (cursor[0] == 'f' && isspace((unsigned char)cursor[1])) {
      face.count = 0;

      char *token = strtok(cursor + 1, " \t\r\n");

      while (token) {
        BLB_OBJFaceVertex vertex;

        if (!BLB_OBJParseFaceVertex(token, &positions, &uvs, &normals, &vertex)) {
          failed = true;
          break;
        }

        if (!BLB_OBJFacePush(&face, vertex)) {
          failed = true;
          break;
        }

        token = strtok(NULL, " \t\r\n");
      }

      if (failed)
        break;

      if (face.count < 3)
        continue;

      if (polygon->vertex_count > SIZE_MAX - face.count) {
        failed = true;
        break;
      }

      if (polygon->index_count > SIZE_MAX - (face.count - 2u) * 3u) {
        failed = true;
        break;
      }

      size_t vertex_start = polygon->vertex_count;
      size_t new_vertex_count = polygon->vertex_count + face.count;
      size_t new_index_count = polygon->index_count + (face.count - 2u) * 3u;

      if (new_vertex_count > UINT_MAX) {
        failed = true;
        break;
      }

      HMM_Vec3 face_normal = BLB_OBJCalculateFaceNormal(face.data[0].position, face.data[1].position, face.data[2].position);

      HMM_Vec3 *vertices = realloc(polygon->vertices, sizeof(*vertices) * new_vertex_count);

      if (!vertices) {
        failed = true;
        break;
      }

      polygon->vertices = vertices;

      HMM_Vec3 *base_vertices = realloc(polygon->base_vertices, sizeof(*base_vertices) * new_vertex_count);

      if (!base_vertices) {
        failed = true;
        break;
      }

      polygon->base_vertices = base_vertices;

      HMM_Vec3 *polygon_normals = realloc(polygon->normals, sizeof(*polygon_normals) * new_vertex_count);

      if (!polygon_normals) {
        failed = true;
        break;
      }

      polygon->normals = polygon_normals;

      HMM_Vec2 *polygon_uvs = realloc(polygon->uvs, sizeof(*polygon_uvs) * new_vertex_count);

      if (!polygon_uvs) {
        failed = true;
        break;
      }

      polygon->uvs = polygon_uvs;

      unsigned int *indices = realloc(polygon->indices, sizeof(*indices) * new_index_count);

      if (!indices) {
        failed = true;
        break;
      }

      polygon->indices = indices;

      for (size_t i = 0; i < face.count; ++i) {
        polygon->vertices[vertex_start + i] = face.data[i].position;
        polygon->base_vertices[vertex_start + i] = face.data[i].position;
        polygon->uvs[vertex_start + i] = face.data[i].has_uv ? face.data[i].uv : HMM_V2(0.0f, 0.0f);
        polygon->normals[vertex_start + i] = face.data[i].has_normal ? face.data[i].normal : face_normal;
      }

      for (size_t i = 1; i + 1 < face.count; ++i) {
        polygon->indices[polygon->index_count++] = (unsigned int)vertex_start;
        polygon->indices[polygon->index_count++] = (unsigned int)(vertex_start + i);
        polygon->indices[polygon->index_count++] = (unsigned int)(vertex_start + i + 1u);
      }

      polygon->vertex_count = new_vertex_count;
    }
  }

  fclose(file);

  free(positions.data);
  free(uvs.data);
  free(normals.data);
  free(face.data);

  if (failed || polygon->vertex_count == 0 || polygon->index_count == 0) {
    BLB_ModelFreePolygon(polygon);
    return NULL;
  }

  return polygon;
}

BLB_Object3D *BLB_LoadModelOBJ(const char *path, HMM_Vec3 scale, HMM_Vec3 position, BLB_Texture *texture) {
  if (!path)
    return NULL;

  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (!object)
    return NULL;

  object->delta_time = calloc(1, sizeof(*object->delta_time));

  if (!object->delta_time)
    goto fail;

  object->type = BLB_OBJECT_CUSTOM_MODEL;

  if (!BLB_OBJECTS_ID || object->type >= BLB_OBJECTS_ID_COUNT)
    goto fail;

  object->id = &BLB_OBJECTS_ID[object->type];

  if (object->id->id == BLB_INVALID_OBJECT_ID)
    goto fail;

  BLB_ModelGeometry *geometry = BLB_ModelCacheFind(path);

  if (geometry) {
    BLB_ModelCacheRetain(geometry);
  } else {
    BLB_Polygon3D *polygon = BLB_LoadOBJPolygon(path);

    if (!polygon)
      goto fail;

    geometry = BLB_ModelCacheAdd(path, polygon);

    if (!geometry) {
      BLB_ModelFreePolygon(polygon);
      goto fail;
    }
  }

  object->geometry_id = geometry->geometry_id;
  object->polygon = geometry->polygon;

  ++object->id->id;

  object->mesh.vertices = object->polygon->vertices;
  object->mesh.normals = object->polygon->normals;
  object->mesh.uvs = object->polygon->uvs;
  object->mesh.vertex_count = object->polygon->vertex_count;
  object->mesh.indices = object->polygon->indices;
  object->mesh.index_count = object->polygon->index_count;

  object->position = position;
  object->rotation = HMM_V3(0.0f, 0.0f, 0.0f);
  object->scale = scale;

  object->render_mode = BLB_RENDER_OPAQUE;

  object->color[0] = 255;
  object->color[1] = 255;
  object->color[2] = 255;
  object->color[3] = 255;

  object->visible = true;
  object->entity_id = BLB_INVALID_ENTITY_ID;
  object->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;

  object->material = BLB_Material_Create3D();

  if (!object->material) {
    BLB_ModelCacheRelease(object);
    object->polygon = NULL;
    object->geometry_id = 0;

    if (object->id->id > 0)
      --object->id->id;

    goto fail;
  }

  BLB_Object3D_Transform(object, position, object->rotation, scale);

  if (texture)
    BLB_Object3D_SetTexture(object, texture);

  return object;
fail:
  free(object->delta_time);
  free(object);
  return NULL;
}

void BLB_DestroyModelOBJ(BLB_Object3D *object) {
  if (!object)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  BLB_ModelCacheRelease(object);

  if (object->id && object->id->id > 0)
    --object->id->id;

  object->polygon = NULL;
  object->geometry_id = 0;

  free(object);
}
