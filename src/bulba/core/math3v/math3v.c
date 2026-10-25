#include "bulba/core/math3v/math3v.h"

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

#include <math.h>

BLB_Quat BLB_QuatFromEuler(BLB_Vec3 euler) {
  float cx = cosf(euler.x * 0.5f);
  float sx = sinf(euler.x * 0.5f);

  float cy = cosf(euler.y * 0.5f);
  float sy = sinf(euler.y * 0.5f);

  float cz = cosf(euler.z * 0.5f);
  float sz = sinf(euler.z * 0.5f);

  BLB_Quat q;

  q.x = sx * cy * cz - cx * sy * sz;
  q.y = cx * sy * cz + sx * cy * sz;
  q.z = cx * cy * sz - sx * sy * cz;
  q.w = cx * cy * cz + sx * sy * sz;

  return q;
}

BLB_Vec3 BLB_EulerFromQuat(BLB_Quat q) {
  float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
  float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);

  float x = atan2f(sinr_cosp, cosr_cosp);

  float sinp = 2.0f * (q.w * q.y - q.z * q.x);

  float y;

  if (fabsf(sinp) >= 1.0f)
    y = copysignf(1.57079632679f, sinp);
  else
    y = asinf(sinp);

  float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
  float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);

  float z = atan2f(siny_cosp, cosy_cosp);

  return BLB_V3(x, y, z);
}
