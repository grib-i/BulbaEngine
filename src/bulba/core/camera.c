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

  c->delta_time = malloc(sizeof(float));
  c->camera_cache = malloc(sizeof(BLB_CameraCache));

  if (!c->delta_time || !c->camera_cache) {
    free(c->delta_time);
    free(c->camera_cache);
    free(c);
    return NULL;
  }
  *c->delta_time = 1.0f / 60.0f;

  return c;
}

void BLB_DestroyCamera(BLB_Camera *c) {
  free(c->camera_cache);
  free(c);
}

HMM_Mat4 BLB_CameraView(const BLB_Camera *c) {
  if (!c)
    return HMM_M4D(1);
  float y = c->rotation.y * HMM_PI / 180.0f, p = c->rotation.x * HMM_PI / 180.0f;
  HMM_Vec3 dir = HMM_V3(cosf(p) * sinf(y), -sinf(p), -cosf(p) * cosf(y));
  HMM_Vec3 up = HMM_V3(sinf(y) * sinf(p), cosf(p), -cosf(y) * sinf(p));
  return HMM_LookAt_RH(c->position, HMM_AddV3(c->position, dir), up);
}

HMM_Mat4 BLB_CameraProjection(const BLB_Camera *c, float aspect) {
  HMM_Mat4 projection = HMM_Perspective_RH_ZO(c ? c->fov : 70.0f * HMM_PI / 180.0f, aspect, c ? c->near_plane : .1f, c ? c->far_plane : 1000.0f);

  /* Vulkan's positive-height viewport maps +NDC Y toward the bottom of the
   * framebuffer. Flip the camera projection so world-space +Y remains up on
   * screen (the conventional RH camera coordinate system). */
  projection.Elements[1][1] = -projection.Elements[1][1];
  return projection;
}

void BLB_Camera_Move(BLB_Camera *camera, HMM_Vec3 velocity) {
  if (!camera)
    return;

  if (camera->delta_time)
    velocity = HMM_MulV3F(velocity, *camera->delta_time);

  camera->position = HMM_AddV3(camera->position, velocity);
}

void BLB_Camera_SetPosition(BLB_Camera *camera, HMM_Vec3 position) {
  if (!camera)
    return;

  camera->position = position;
}

void BLB_Camera_Rotate(BLB_Camera *camera, HMM_Vec3 angular_velocity) {
  if (!camera)
    return;

  if (camera->delta_time)
    angular_velocity = HMM_MulV3F(angular_velocity, *camera->delta_time);

  camera->rotation = HMM_AddV3(camera->rotation, angular_velocity);
}

void BLB_Camera_SetRotation(BLB_Camera *camera, HMM_Vec3 rotation) {
  if (!camera)
    return;

  camera->rotation = rotation;
}
