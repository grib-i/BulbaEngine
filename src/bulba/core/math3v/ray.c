#include "bulba/core/math3v/ray.h"
#include <stdarg.h>
#include <stdint.h>

static BLB_RayHit3D *ray3d_math(BLB_Vec3 ray_start, BLB_Vec3 ray_end,
                                BLB_Object3D *object) {
  if (!object)
    return NULL;
  float dx = ray_end.x - ray_start.x;
  float dy = ray_end.y - ray_start.y;
  float dz = ray_end.z - ray_start.z;

  float min_x = object->position.x - fabsf(object->scale.x);
  float max_x = object->position.x + fabsf(object->scale.x);
  float min_y = object->position.y - fabsf(object->scale.y);
  float max_y = object->position.y + fabsf(object->scale.y);
  float min_z = object->position.z - fabsf(object->scale.z);
  float max_z = object->position.z + fabsf(object->scale.z);

  float tx1, tx2, ty1, ty2, tz1, tz2;

  if (fabsf(dx) < 1e-6f) {
    if (ray_start.x < min_x || ray_start.x > max_x)
      return NULL;
    tx1 = -INFINITY;
    tx2 = INFINITY;
  } else {
    tx1 = (min_x - ray_start.x) / dx;
    tx2 = (max_x - ray_start.x) / dx;

    if (tx1 > tx2) {
      float t = tx1;
      tx1 = tx2;
      tx2 = t;
    }
  }

  if (fabsf(dy) < 1e-6f) {
    if (ray_start.y < min_y || ray_start.y > max_y)
      return NULL;
    ty1 = -INFINITY;
    ty2 = INFINITY;
  } else {
    ty1 = (min_y - ray_start.y) / dy;
    ty2 = (max_y - ray_start.y) / dy;

    if (ty1 > ty2) {
      float t = ty1;
      ty1 = ty2;
      ty2 = t;
    }
  }

  if (fabsf(dz) < 1e-6f) {
    if (ray_start.z < min_z || ray_start.z > max_z)
      return NULL;
    tz1 = -INFINITY;
    tz2 = INFINITY;
  } else {
    tz1 = (min_z - ray_start.z) / dz;
    tz2 = (max_z - ray_start.z) / dz;

    if (tz1 > tz2) {
      float t = tz1;
      tz1 = tz2;
      tz2 = t;
    }
  }

  float tmin = fmaxf(fmaxf(tx1, ty1), tz1);
  float tmax = fminf(fminf(tx2, ty2), tz2);

  if (tmax < 0.0f || tmin > tmax || tmin > 1.0f)
    return NULL;

  float t = fmaxf(tmin, 0.0f);

  BLB_Vec3 normal = BLB_V3(0.0f, 0.0f, 0.0f);

  if (fabsf(t - tx1) < 1e-5f)
    normal = BLB_V3(dx > 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
  else if (fabsf(t - tx2) < 1e-5f)
    normal = BLB_V3(dx > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
  else if (fabsf(t - ty1) < 1e-5f)
    normal = BLB_V3(0.0f, dy > 0.0f ? -1.0f : 1.0f, 0.0f);
  else if (fabsf(t - ty2) < 1e-5f)
    normal = BLB_V3(0.0f, dy > 0.0f ? 1.0f : -1.0f, 0.0f);
  else if (fabsf(t - tz1) < 1e-5f)
    normal = BLB_V3(0.0f, 0.0f, dz > 0.0f ? -1.0f : 1.0f);
  else if (fabsf(t - tz2) < 1e-5f)
    normal = BLB_V3(0.0f, 0.0f, dz > 0.0f ? 1.0f : -1.0f);

  BLB_RayHit3D *r = calloc(1, sizeof(*r));

  if (!r)
    return NULL;

  r->hit = true;
  r->object = object;
  r->t = t;
  r->dx = dx;
  r->dy = dy;
  r->dz = dz;
  r->point =
      BLB_V3(ray_start.x + dx * t, ray_start.y + dy * t, ray_start.z + dz * t);
  r->normal = normal;
  r->distance = sqrtf(dx * dx + dy * dy + dz * dz) * t;

  return r;
}

static BLB_RayHit2D *ray2d_math(BLB_Vec2 ray_start, BLB_Vec2 ray_end,
                                BLB_Object2D *object) {
  if (!object)
    return NULL;
  float dx = ray_end.x - ray_start.x;
  float dy = ray_end.y - ray_start.y;

  float min_x = object->position.x - fabsf(object->scale.x);
  float max_x = object->position.x + fabsf(object->scale.x);
  float min_y = object->position.y - fabsf(object->scale.y);
  float max_y = object->position.y + fabsf(object->scale.y);

  float tx1, tx2, ty1, ty2;

  if (fabsf(dx) < 1e-6f) {
    if (ray_start.x < min_x || ray_start.x > max_x)
      return NULL;
    tx1 = -INFINITY;
    tx2 = INFINITY;
  } else {
    tx1 = (min_x - ray_start.x) / dx;
    tx2 = (max_x - ray_start.x) / dx;

    if (tx1 > tx2) {
      float t = tx1;
      tx1 = tx2;
      tx2 = t;
    }
  }

  if (fabsf(dy) < 1e-6f) {
    if (ray_start.y < min_y || ray_start.y > max_y)
      return NULL;
    ty1 = -INFINITY;
    ty2 = INFINITY;
  } else {
    ty1 = (min_y - ray_start.y) / dy;
    ty2 = (max_y - ray_start.y) / dy;

    if (ty1 > ty2) {
      float t = ty1;
      ty1 = ty2;
      ty2 = t;
    }
  }

  float tmin = fmaxf(tx1, ty1);
  float tmax = fminf(tx2, ty2);

  if (tmax < 0.0f || tmin > tmax || tmin > 1.0f)
    return NULL;

  float t = fmaxf(tmin, 0.0f);

  BLB_Vec2 normal = BLB_V2(0.0f, 0.0f);

  if (fabsf(t - tx1) < 1e-5f)
    normal = BLB_V2(dx > 0.0f ? -1.0f : 1.0f, 0.0f);
  else if (fabsf(t - tx2) < 1e-5f)
    normal = BLB_V2(dx > 0.0f ? 1.0f : -1.0f, 0.0f);
  else if (fabsf(t - ty1) < 1e-5f)
    normal = BLB_V2(0.0f, dy > 0.0f ? -1.0f : 1.0f);
  else if (fabsf(t - ty2) < 1e-5f)
    normal = BLB_V2(0.0f, dy > 0.0f ? 1.0f : -1.0f);

  BLB_RayHit2D *r = calloc(1, sizeof(*r));

  if (!r)
    return NULL;

  r->hit = true;
  r->object = object;
  r->t = t;
  r->dx = dx;
  r->dy = dy;
  r->point = BLB_V2(ray_start.x + dx * t, ray_start.y + dy * t);
  r->normal = normal;
  r->distance = sqrtf(dx * dx + dy * dy) * t;

  return r;
}

/*
 * ray2d signature:
 * x1 y1 - start point
 * x2 y2 - end point

 * FIRST_HIT — stop at the nearest one
 * ALL_HITS — collect all
 * FILTER — skip specific objects
 */
BLB_RayHit2D **BLB_Ray2D(BLB_Scene *scene, BLB_Vec2 ray_start, BLB_Vec2 ray_end,
                         BLB_RayMode mode, BLB_Object2D *filter, ...) {
  if (!scene || scene->object2d_count <= 0 || !scene->objects2d ||
      (mode != BLB_RAY_FIRST_HIT && mode != BLB_RAY_ALL_HITS &&
       mode != BLB_RAY_FILTER))
    return NULL;

  if (!isfinite(ray_start.x) || !isfinite(ray_end.x))
    return NULL;

  if (!isfinite(ray_start.y) || !isfinite(ray_end.y))
    return NULL;
  BLB_Object2D **excluded = NULL;
  size_t excluded_count = 0;

  if (filter) {
    va_list args;
    va_start(args, filter);
    BLB_Object2D *item = filter;

    while (item) {
      if (excluded_count >= SIZE_MAX / sizeof(*excluded)) {
        va_end(args);
        free(excluded);

        return NULL;
      }
      BLB_Object2D **next =
          realloc(excluded, (excluded_count + 1u) * sizeof(*excluded));

      if (!next) {
        va_end(args);
        free(excluded);

        return NULL;
      }
      excluded = next;
      excluded[excluded_count++] = item;
      item = va_arg(args, BLB_Object2D *);
    }
    va_end(args);
  }
  size_t capacity =
      mode == BLB_RAY_FIRST_HIT ? 1u : (size_t)scene->object2d_count;

  if (capacity > (SIZE_MAX - sizeof(size_t)) / sizeof(BLB_RayHit2D *) - 1u) {
    free(excluded);

    return NULL;
  }
  BLB_RayHit2D **results =
      calloc(1, (capacity + 1u) * sizeof(*results) + sizeof(size_t));

  if (!results) {
    free(excluded);

    return NULL;
  }
  size_t *count = (size_t *)(results + capacity + 1u);

  for (int i = 0; i < scene->object2d_count; ++i) {
    BLB_Object2D *object = scene->objects2d[i];
    bool skip = !object;

    for (size_t j = 0; j < excluded_count && !skip; ++j)
      skip = excluded[j] == object;

    if (skip)
      continue;
    BLB_RayHit2D *value = ray2d_math(ray_start, ray_end, object);

    if (!value)
      continue;
    value->count = count;

    if (mode == BLB_RAY_FIRST_HIT) {
      if (!results[0] || value->t < results[0]->t) {
        free(results[0]);
        results[0] = value;
        *count = 1;
      } else {
        free(value);
      }
    } else {
      results[(*count)++] = value;
    }
  }
  free(excluded);

  if (!*count) {
    free(results);

    return NULL;
  }

  return results;
}

void BLB_RayHits2D_Destroy(BLB_RayHit2D **hits) {
  if (!hits)
    return;

  for (size_t i = 0; hits[i]; ++i)
    free(hits[i]);
  free(hits);
}

/*
 * ray3d signature:
 * x1 y1 z1 - start point
 * x2 y2 z2 - end point

 * FIRST_HIT — stop at the nearest one
 * ALL_HITS — collect all
 * FILTER — skip specific objects
 */
BLB_RayHit3D **BLB_Ray3D(BLB_Scene *scene, BLB_Vec3 ray_start, BLB_Vec3 ray_end,
                         BLB_RayMode mode, BLB_Object3D *filter, ...) {
  if (!scene || scene->object3d_count <= 0 || !scene->objects3d ||
      (mode != BLB_RAY_FIRST_HIT && mode != BLB_RAY_ALL_HITS &&
       mode != BLB_RAY_FILTER))
    return NULL;

  if (!isfinite(ray_start.x) || !isfinite(ray_end.x))
    return NULL;

  if (!isfinite(ray_start.y) || !isfinite(ray_end.y))
    return NULL;

  if (!isfinite(ray_start.z) || !isfinite(ray_end.z))
    return NULL;
  BLB_Object3D **excluded = NULL;
  size_t excluded_count = 0;

  if (filter) {
    va_list args;
    va_start(args, filter);
    BLB_Object3D *item = filter;

    while (item) {
      if (excluded_count >= SIZE_MAX / sizeof(*excluded)) {
        va_end(args);
        free(excluded);

        return NULL;
      }
      BLB_Object3D **next =
          realloc(excluded, (excluded_count + 1u) * sizeof(*excluded));

      if (!next) {
        va_end(args);
        free(excluded);

        return NULL;
      }
      excluded = next;
      excluded[excluded_count++] = item;
      item = va_arg(args, BLB_Object3D *);
    }
    va_end(args);
  }
  size_t capacity =
      mode == BLB_RAY_FIRST_HIT ? 1u : (size_t)scene->object3d_count;

  if (capacity > (SIZE_MAX - sizeof(size_t)) / sizeof(BLB_RayHit3D *) - 1u) {
    free(excluded);

    return NULL;
  }
  BLB_RayHit3D **results =
      calloc(1, (capacity + 1u) * sizeof(*results) + sizeof(size_t));

  if (!results) {
    free(excluded);

    return NULL;
  }
  size_t *count = (size_t *)(results + capacity + 1u);

  for (int i = 0; i < scene->object3d_count; ++i) {
    BLB_Object3D *object = scene->objects3d[i];
    bool skip = !object;

    for (size_t j = 0; j < excluded_count && !skip; ++j)
      skip = excluded[j] == object;

    if (skip)
      continue;
    BLB_RayHit3D *value = ray3d_math(ray_start, ray_end, object);

    if (!value)
      continue;
    value->count = count;

    if (mode == BLB_RAY_FIRST_HIT) {
      if (!results[0] || value->t < results[0]->t) {
        free(results[0]);
        results[0] = value;
        *count = 1;
      } else {
        free(value);
      }
    } else {
      results[(*count)++] = value;
    }
  }
  free(excluded);

  if (!*count) {
    free(results);

    return NULL;
  }

  return results;
}

void BLB_RayHits3D_Destroy(BLB_RayHit3D **hits) {
  if (!hits)
    return;

  for (size_t i = 0; hits[i]; ++i)
    free(hits[i]);
  free(hits);
}
