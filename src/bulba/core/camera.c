#include "bulba/core/camera.h"

#include <math.h>
#include <stdlib.h>

BLB_Camera *BLB_CreateCamera(HMM_Vec3 p) {
  BLB_Camera *c = calloc(1, sizeof(*c));
  if (!c)
    return NULL;
  c->position = p;
  c->fov = 70.0f * HMM_PI / 180.0f;
  c->near_plane = .1f;
  c->far_plane = 1000;
  c->aspect = 16.0f / 9.0f;
  return c;
}

void BLB_DestroyCamera(BLB_Camera *c) { free(c); }

HMM_Mat4 BLB_CameraView(const BLB_Camera *c) {
  if (!c)
    return HMM_M4D(1);
  float y = c->rotation.y * HMM_PI / 180.0f, p = c->rotation.x * HMM_PI / 180.0f;
  HMM_Vec3 dir = HMM_V3(cosf(p) * sinf(y), -sinf(p), -cosf(p) * cosf(y));
  return HMM_LookAt_RH(c->position, HMM_AddV3(c->position, dir), HMM_V3(0, 1, 0));
}

HMM_Mat4 BLB_CameraProjection(const BLB_Camera *c, float aspect) {
  return HMM_Perspective_RH_ZO(c ? c->fov : 70.0f * HMM_PI / 180.0f, aspect, c ? c->near_plane : .1f, c ? c->far_plane : 1000.0f);
}
