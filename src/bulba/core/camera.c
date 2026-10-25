#include "bulba/core/camera.h"
#include "bulba/core/utils/delta_time.h"

#include <math.h>
#include <stdlib.h>

BLB_Camera *BLB_CreateCamera(BLB_Vec3 p) {
  BLB_Camera *c = calloc(1, sizeof(*c));

  if (!c)
    return NULL;

  c->position = p;
  c->fov = 70.0f * BLB_PI / 180.0f;
  c->near_plane = .1f;
  c->far_plane = 1000;
  c->aspect = 16.0f / 9.0f;

  c->delta_time = malloc(sizeof(float));
  c->camera_cache = calloc(1, sizeof(BLB_CameraCache));

  if (!c->delta_time || !c->camera_cache) {
    free(c->delta_time);
    free(c->camera_cache);
    free(c);

    return NULL;
  }
  *c->delta_time = BLB_DeltaTime_Get();

  return c;
}

void BLB_DestroyCamera(BLB_Camera *c) {
  if (!c)
    return;
  free(c->delta_time);
  free(c->camera_cache);
  free(c);
}

BLB_Mat4 BLB_CameraView(const BLB_Camera *c) {
  if (!c)
    return BLB_M4D(1);

  float y = c->rotation.y * BLB_PI / 180.0f, p = c->rotation.x * BLB_PI / 180.0f;
  BLB_Vec3 dir = BLB_V3(cosf(p) * sinf(y), -sinf(p), -cosf(p) * cosf(y));
  BLB_Vec3 up = BLB_V3(sinf(y) * sinf(p), cosf(p), -cosf(y) * sinf(p));

  return BLB_LookAt_RH(c->position, BLB_AddV3(c->position, dir), up);
}

BLB_Mat4 BLB_CameraProjection(const BLB_Camera *c, float aspect) {
  BLB_Mat4 projection = BLB_Perspective_RH_ZO(c ? c->fov : 70.0f * BLB_PI / 180.0f, aspect, c ? c->near_plane : .1f, c ? c->far_plane : 1000.0f);

  /* Vulkan's positive-height viewport maps +NDC Y toward the bottom of the
   * framebuffer. Flip the camera projection so world-space +Y remains up on
   * screen (the conventional RH camera coordinate system). */
  projection.Elements[1][1] = -projection.Elements[1][1];

  return projection;
}

void BLB_Camera_Move(BLB_Camera *camera, BLB_Vec3 velocity) {
  if (!camera)
    return;

  if (camera->delta_time)
    velocity = BLB_MulV3F(velocity, *camera->delta_time);

  camera->position = BLB_AddV3(camera->position, velocity);
}

void BLB_Camera_SetPosition(BLB_Camera *camera, BLB_Vec3 position) {
  if (!camera)
    return;

  camera->position = position;
}

void BLB_Camera_Rotate(BLB_Camera *camera, BLB_Vec3 angular_velocity) {
  if (!camera)
    return;

  if (camera->delta_time)
    angular_velocity = BLB_MulV3F(angular_velocity, *camera->delta_time);

  camera->rotation = BLB_AddV3(camera->rotation, angular_velocity);
}

void BLB_Camera_SetRotation(BLB_Camera *camera, BLB_Vec3 rotation) {
  if (!camera)
    return;

  camera->rotation = rotation;
}

BLB_Mat4 BLB_CameraPixelProjection(float width, float height) {
  width = fmaxf(width, 1.0f);
  height = fmaxf(height, 1.0f);

  BLB_Mat4 result = BLB_M4D(1.0f);

  result.Elements[0][0] = 2.0f / width;
  result.Elements[1][1] = 2.0f / height;
  result.Elements[2][2] = -0.5f / fmaxf(width, height);
  result.Elements[3][0] = -1.0f;
  result.Elements[3][1] = -1.0f;
  result.Elements[3][2] = 0.5f;

  return result;
}

BLB_Mat4 BLB_CameraPixelToWorld(const BLB_CameraCache *cache) {
  if (!cache || !cache->valid)
    return BLB_M4D(1.0f);

  return BLB_MulM4(BLB_InvGeneralM4(cache->view_projection), BLB_CameraPixelProjection(cache->viewport_width, cache->viewport_height));
}
