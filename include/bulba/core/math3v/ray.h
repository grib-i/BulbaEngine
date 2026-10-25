#ifndef BULBA_CORE_RAY_H
#define BULBA_CORE_RAY_H

#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/scene.h"

/*
 * hit - was there any contact
 * *object - which hit object
 * point - the point where the beam struck
 * normal - the orientation of the surface at this point. Needed, for example,
 * for reflection, bouncing, and lighting. distance - max distance whiching move
 * ray t - time count - count everony hits
 */
typedef struct {
  bool hit;
  BLB_Object2D *object;
  BLB_Vec2 point;
  BLB_Vec2 normal;
  float distance;
  float t;

  float dx;
  float dy;

  size_t *count;
} BLB_RayHit2D;

typedef struct {
  bool hit;
  BLB_Object3D *object;
  BLB_Vec3 point;
  BLB_Vec3 normal;
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
typedef enum {
  BLB_RAY_FIRST_HIT = 1,
  BLB_RAY_ALL_HITS = 2,
  BLB_RAY_FILTER = 3
} BLB_RayMode;

/**
 * ray2d signature:
 * BLB_V2(x1 y1) - start point
 * BLB_V2(x2 y2) - end point
 *
 */
BLB_RayHit2D **BLB_Ray2D(BLB_Scene *scene, BLB_Vec2 ray_start, BLB_Vec2 ray_end,
                         BLB_RayMode mode, BLB_Object2D *filter, ...);

/*
 * ray2d signature:
 * BLB_V3(x1 y1 z1) - start point
 * BLB_V3(x2 y2 z2) - end point
 *
 */
BLB_RayHit3D **BLB_Ray3D(BLB_Scene *scene, BLB_Vec3 ray_start, BLB_Vec3 ray_end,
                         BLB_RayMode mode, BLB_Object3D *filter, ...);
void BLB_RayHits2D_Destroy(BLB_RayHit2D **hits);
void BLB_RayHits3D_Destroy(BLB_RayHit3D **hits);

#endif
