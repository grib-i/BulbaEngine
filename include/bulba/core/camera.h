#ifndef BULBA_CORE_CAMERA_H
#define BULBA_CORE_CAMERA_H

#include "bulba/core/math3v/math3v.h"

typedef struct BLB_Camera {
  HMM_Vec3 position;
  HMM_Vec3 rotation;
  float fov, near_plane, far_plane;
  float aspect;
} BLB_Camera;

BLB_Camera *BLB_CreateCamera(HMM_Vec3 position);
void BLB_DestroyCamera(BLB_Camera *camera);

HMM_Mat4 BLB_CameraView(const BLB_Camera *camera);
HMM_Mat4 BLB_CameraProjection(const BLB_Camera *camera, float aspect);

#endif
