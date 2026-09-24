#ifndef BULBA_CORE_CAMERA_H
#define BULBA_CORE_CAMERA_H

#include "bulba/core/math3v/math3v.h"

#include <stdbool.h>

typedef struct {
  bool valid;
  HMM_Mat4 view;
  HMM_Mat4 projection;
  HMM_Mat4 view_projection;
} BLB_CameraCache;

typedef struct BLB_Camera {
  HMM_Vec3 position;
  HMM_Vec3 rotation;
  float fov, near_plane, far_plane;
  float aspect;

  float *delta_time;

  BLB_CameraCache *camera_cache;
} BLB_Camera;

BLB_Camera *BLB_CreateCamera(HMM_Vec3 position);
void BLB_DestroyCamera(BLB_Camera *camera);

HMM_Mat4 BLB_CameraView(const BLB_Camera *camera);
HMM_Mat4 BLB_CameraProjection(const BLB_Camera *camera, float aspect);

void BLB_Camera_Move(BLB_Camera *camera, HMM_Vec3 velocity);

void BLB_Camera_SetPosition(BLB_Camera *camera, HMM_Vec3 position);

void BLB_Camera_Rotate(BLB_Camera *camera, HMM_Vec3 angular_velocity);

void BLB_Camera_SetRotation(BLB_Camera *camera, HMM_Vec3 rotation);

#endif
