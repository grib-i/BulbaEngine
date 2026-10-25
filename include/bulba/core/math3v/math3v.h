#ifndef BULBA_CORE_MATH3V_H
#define BULBA_CORE_MATH3V_H

#include "vec_math.h"
#include <stdbool.h>
#include <stddef.h>

#define BLB_MIN(a, b) ((a) > (b) ? (b) : (a))
#define BLB_MAX(a, b) ((a) < (b) ? (b) : (a))
#define BLB_ABS(a) ((a) > 0 ? (a) : -(a))
#define BLB_MOD(a, m) (((a) % (m)) >= 0 ? ((a) % (m)) : (((a) % (m)) + (m)))
#define BLB_SQUARE(x) ((x) * (x))

#define BLB_END ((BLB_Object2D *)0)

int BLB_MaxInt(const int numbers, ...);
int BLB_MinInt(const int numbers, ...);

BLB_Quat BLB_QuatFromEuler(BLB_Vec3 euler);
BLB_Vec3 BLB_EulerFromQuat(BLB_Quat q);

typedef struct {
  BLB_Vec2 min;
  BLB_Vec2 max;
} AABB_2D;

typedef struct {
  BLB_Vec3 min;
  BLB_Vec3 max;
} AABB_3D;

#endif
