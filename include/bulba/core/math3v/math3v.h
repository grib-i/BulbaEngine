#ifndef BULBA_CORE_MATH3V_H
#define BULBA_CORE_MATH3V_H

#include "HandmadeMath.h"
#include <stdbool.h>
#include <stddef.h>

#define BLB_END ((BLB_Object2D *)0)

int BLB_MaxInt(const int numbers, ...);
int BLB_MinInt(const int numbers, ...);

typedef struct {
  HMM_Vec2 min;
  HMM_Vec2 max;
} AABB_2D;

typedef struct {
  HMM_Vec3 min;
  HMM_Vec3 max;
} AABB_3D;

/*
 * for avoid errors:
 * In included file:
 *  main file cannot be included recursively when building a preamble
 */
typedef struct BLB_Scene BLB_Scene;
typedef struct BLB_Object2D BLB_Object2D;
typedef struct BLB_Object3D BLB_Object3D;

/*
 * hit - was there any contact
 * *object - which hit object
 * point - the point where the beam struck
 * normal - the orientation of the surface at this point. Needed, for example, for reflection, bouncing, and lighting.
 * distance - max distance whiching move ray
 * t - time
 * count - count everony hits
 */
typedef struct {
  bool hit;
  BLB_Object2D *object;
  HMM_Vec2 point;
  HMM_Vec2 normal;
  float distance;
  float t;

  float dx;
  float dy;

  size_t *count;
} BLB_RayHit2D;

typedef struct {
  bool hit;
  BLB_Object3D *object;
  HMM_Vec3 point;
  HMM_Vec3 normal;
  float distance;
  float t;

  float dx;
  float dy;
  float dz;

  size_t *count;
} BLB_RayHit3D;

/*
 * FIRST_HIT — stop at the nearest one
 * ALL_HITS — collect all
 * FILTER — skip specific objects
 */
typedef enum { BLB_RAY_FIRST_HIT = 1, BLB_RAY_ALL_HITS = 2, BLB_RAY_FILTER = 3 } BLB_RayMode;

/*
 * ray2d signature:
 * x1 y1 - start point
 * x2 y2 - end point
 */
BLB_RayHit2D **ray2d(BLB_Scene *scene, HMM_Vec2 ray_start, HMM_Vec2 ray_end, BLB_RayMode mode, BLB_Object2D *filter, ...);

/*
 * ray2d signature:
 * x1 y1 z1 - start point
 * x2 y2 z2 - end point
 */
BLB_RayHit3D **ray3d(BLB_Scene *scene, HMM_Vec3 ray_start, HMM_Vec3 ray_end, BLB_RayMode mode, BLB_Object3D *filter, ...);

#endif
