#include "bulba/core/math3v/math3v.h"

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/scene.h"

#include <limits.h>
#include <stdarg.h>

int BLB_MaxInt(const int numbers, ...) {
  int max_val = numbers;
  va_list args;
  va_start(args, numbers);

  int next;
  while ((next = va_arg(args, int)) != INT_MIN) {
    if (next > max_val) {
      max_val = next;
    }
  }

  va_end(args);
  return max_val;
}

int BLB_MinInt(const int numbers, ...) {
  int min_val = numbers;
  va_list args;
  va_start(args, numbers);

  int next;
  while ((next = va_arg(args, int)) != INT_MAX) {
    if (next < min_val) {
      min_val = next;
    }
  }

  va_end(args);
  return min_val;
}

static BLB_RayHit3D *ray3d_math(HMM_Vec3 ray_start, HMM_Vec3 ray_end, BLB_Object3D *object) {
  float dx = ray_end.x - ray_start.x;
  float dy = ray_end.y - ray_start.y;
  float dz = ray_end.z - ray_start.z;

  float min_x = object->position.x - object->scale.y;
  float max_x = object->position.x + object->scale.y;
  float min_y = object->position.y - object->scale.y;
  float max_y = object->position.y + object->scale.y;
  float min_z = object->position.z - object->scale.z;
  float max_z = object->position.z + object->scale.z;

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

  float t = tmin >= 0.0f ? tmin : tmax;

  HMM_Vec3 normal = HMM_V3(0.0f, 0.0f, 0.0f);

  if (fabsf(t - tx1) < 1e-5f)
    normal = HMM_V3(dx > 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
  else if (fabsf(t - tx2) < 1e-5f)
    normal = HMM_V3(dx > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
  else if (fabsf(t - ty1) < 1e-5f)
    normal = HMM_V3(0.0f, dy > 0.0f ? -1.0f : 1.0f, 0.0f);
  else if (fabsf(t - ty2) < 1e-5f)
    normal = HMM_V3(0.0f, dy > 0.0f ? 1.0f : -1.0f, 0.0f);
  else if (fabsf(t - tz1) < 1e-5f)
    normal = HMM_V3(0.0f, 0.0f, dz > 0.0f ? -1.0f : 1.0f);
  else if (fabsf(t - tz2) < 1e-5f)
    normal = HMM_V3(0.0f, 0.0f, dz > 0.0f ? 1.0f : -1.0f);

  BLB_RayHit3D *r = malloc(sizeof(BLB_RayHit3D));

  r->hit = true;
  r->object = object;
  r->t = t;
  r->dx = dx;
  r->dy = dy;
  r->dz = dz;
  r->point = HMM_V3(ray_start.x + dx * t, ray_start.y + dy * t, ray_start.z + dz * t);
  r->normal = normal;
  r->distance = sqrtf(dx * dx + dy * dy + dz * dz) * t;

  return r;
}

static BLB_RayHit2D *ray2d_math(HMM_Vec2 ray_start, HMM_Vec2 ray_end, BLB_Object2D *object) {
  float dx = ray_end.x - ray_start.x;
  float dy = ray_end.y - ray_start.y;

  float min_x = object->position.x - object->scale.y;
  float max_x = object->position.x + object->scale.y;
  float min_y = object->position.y - object->scale.y;
  float max_y = object->position.y + object->scale.y;

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

  float t = tmin >= 0.0f ? tmin : tmax;

  HMM_Vec2 normal = HMM_V2(0.0f, 0.0f);

  if (fabsf(t - tx1) < 1e-5f)
    normal = HMM_V2(dx > 0.0f ? -1.0f : 1.0f, 0.0f);
  else if (fabsf(t - tx2) < 1e-5f)
    normal = HMM_V2(dx > 0.0f ? 1.0f : -1.0f, 0.0f);
  else if (fabsf(t - ty1) < 1e-5f)
    normal = HMM_V2(0.0f, dy > 0.0f ? -1.0f : 1.0f);
  else if (fabsf(t - ty2) < 1e-5f)
    normal = HMM_V2(0.0f, dy > 0.0f ? 1.0f : -1.0f);

  BLB_RayHit2D *r = malloc(sizeof(BLB_RayHit2D));

  r->hit = true;
  r->object = object;
  r->t = t;
  r->dx = dx;
  r->dy = dy;
  r->point = HMM_V2(ray_start.x + dx * t, ray_start.y + dy * t);
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

BLB_RayHit2D **ray2d(BLB_Scene *scene, HMM_Vec2 ray_start, HMM_Vec2 ray_end, BLB_RayMode mode, BLB_Object2D *filter, ...) {
  if (!scene || !mode)
    return NULL;

  BLB_RayHit2D **rays = malloc(sizeof(BLB_RayHit2D));
  if (mode == BLB_RAY_FILTER || filter) {
    va_list args;
    va_start(args, filter);

    BLB_Object2D *filter;
    while ((filter = va_arg(args, BLB_Object2D *)) != NULL) {
      rays = realloc(rays, ((size_t)args + 1) * sizeof(BLB_RayHit2D *));
      rays[(size_t)args] = ray2d_math(ray_start, ray_end, filter);
    }

    va_end(args);
    return rays;
  }

  for (size_t i = 0; i < scene->object2d_count; i++) {
    if (mode == BLB_RAY_ALL_HITS) {
      rays = realloc(rays, (i + 1) * sizeof(BLB_RayHit2D *));
      rays[i] = ray2d_math(ray_start, ray_end, scene->objects2d[i]);
    }

    if (mode == BLB_RAY_FIRST_HIT) {
      rays = realloc(rays, (i + 1) * sizeof(BLB_RayHit2D *));
      rays[0] = ray2d_math(ray_start, ray_end, scene->objects2d[i]);
      return rays;
    }
  }
  return NULL;
}

/*
 * ray2d signature:
 * x1 y1 z1 - start point
 * x2 y2 z2 - end point

 * FIRST_HIT — stop at the nearest one
 * ALL_HITS — collect all
 * FILTER — skip specific objects
 */
BLB_RayHit3D **ray3d(BLB_Scene *scene, HMM_Vec3 ray_start, HMM_Vec3 ray_end, BLB_RayMode mode, BLB_Object3D *filter, ...) {
  if (!scene || !mode)
    return NULL;

  BLB_RayHit3D **rays = malloc(sizeof(BLB_RayHit3D));
  if (mode == BLB_RAY_FILTER || filter) {
    va_list args;
    va_start(args, filter);

    BLB_Object3D *filter;
    while ((filter = va_arg(args, BLB_Object3D *)) != NULL) {
      rays = realloc(rays, ((size_t)args + 1) * sizeof(BLB_RayHit3D *));
      rays[(size_t)args] = ray3d_math(ray_start, ray_end, filter);
    }

    va_end(args);
    return rays;
  }

  for (size_t i = 0; i < scene->object3d_count; i++) {
    if (mode == BLB_RAY_ALL_HITS) {
      rays = realloc(rays, (i + 1) * sizeof(BLB_RayHit3D *));
      rays[i] = ray3d_math(ray_start, ray_end, scene->objects3d[i]);
    }

    if (mode == BLB_RAY_FIRST_HIT) {
      rays = realloc(rays, (i + 1) * sizeof(BLB_RayHit3D *));
      rays[0] = ray3d_math(ray_start, ray_end, scene->objects3d[i]);
      return rays;
    }
  }
  return NULL;
}
