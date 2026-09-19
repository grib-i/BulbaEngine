#include "bulba/core/math3v/lights.h"

#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/objects3d/cube.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/debug.h"

#include "light_256dp.h"
#include "light_off_256dp.h"

#include <math.h>
#include <stdlib.h>

static void BLB_ReleaseObject3DContents(BLB_Object3D *object) {

  if (object)
    BLB_DestroyCube3D(object);
}

static void BLB_ReleaseObject2DContents(BLB_Object2D *object) {

  if (object)
    BLB_DestroySquare2D(object);
}

BLB_Light3D *BLB_CreateLight3D(BLB_LightType type, HMM_Vec3 position, HMM_Vec3 rotation) {

  BLB_Light3D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->type = type;
  light->enabled = true;

  light->light_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_256dp_width, BLB_TEXTURE_light_256dp_height, BLB_TEXTURE_light_256dp_pixels,
                                                    BLB_TEXTURE_light_256dp_pixel_size);

  if (!light->light_debug_texture) {
    free(light);
    return NULL;
  }

  light->light_off_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_off_256dp_width, BLB_TEXTURE_light_off_256dp_height,
                                                        BLB_TEXTURE_light_off_256dp_pixels, BLB_TEXTURE_light_off_256dp_pixel_size);

  if (!light->light_off_debug_texture) {
    BLB_Texture_Release(light->light_debug_texture);
    free(light);
    return NULL;
  }

  light->object = BLB_CreateCube3D(HMM_V3(1.0f, 1.0f, 0.001f), position, light->light_debug_texture);

  if (!light->object) {
    BLB_Texture_Release(light->light_debug_texture);
    BLB_Texture_Release(light->light_off_debug_texture);
    free(light);
    return NULL;
  }

  light->object->visible = BLB_DEBUG;

  light->object->rotation = rotation;

  return light;
}

BLB_Light2D *BLB_CreateLight2D(BLB_LightType type, HMM_Vec2 position, float rotation) {

  BLB_Light2D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->type = type;
  light->enabled = true;

  light->light_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_256dp_width, BLB_TEXTURE_light_256dp_height, BLB_TEXTURE_light_256dp_pixels,
                                                    BLB_TEXTURE_light_256dp_pixel_size);

  if (!light->light_debug_texture) {
    free(light);
    return NULL;
  }

  light->light_off_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_off_256dp_width, BLB_TEXTURE_light_off_256dp_height,
                                                        BLB_TEXTURE_light_off_256dp_pixels, BLB_TEXTURE_light_off_256dp_pixel_size);

  if (!light->light_off_debug_texture) {
    BLB_Texture_Release(light->light_debug_texture);
    free(light);
    return NULL;
  }

  light->object = BLB_CreateSquare2D(HMM_V2(1.0f, 1.0f), position, light->light_debug_texture, false);

  if (!light->object) {
    BLB_Texture_Release(light->light_debug_texture);
    BLB_Texture_Release(light->light_off_debug_texture);
    free(light);
    return NULL;
  }

  light->object->visible = BLB_DEBUG;

  light->object->rotation = rotation;

  return light;
}

void BLB_DestroyLight3D(BLB_Light3D *light) {

  if (!light)
    return;

  BLB_ReleaseObject3DContents(light->object);

  BLB_Texture_Release(light->light_debug_texture);

  BLB_Texture_Release(light->light_off_debug_texture);

  free(light);
}

void BLB_DestroyLight2D(BLB_Light2D *light) {

  if (!light)
    return;

  BLB_ReleaseObject2DContents(light->object);

  BLB_Texture_Release(light->light_debug_texture);

  BLB_Texture_Release(light->light_off_debug_texture);

  free(light);
}

HMM_Vec3 BLB_GetLightDirection3D(const BLB_Light3D *light) {

  if (!light || !light->object)
    return HMM_V3(0.0f, 0.0f, -1.0f);

  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(light->object->rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));

  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(light->object->rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));

  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(light->object->rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));

  HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));

  HMM_Vec4 forward = HMM_V4(0.0f, 0.0f, -1.0f, 0.0f);

  HMM_Vec4 direction = HMM_MulM4V4(rotation, forward);

  return HMM_NormV3(HMM_V3(direction.x, direction.y, direction.z));
}

HMM_Vec2 BLB_GetLightDirection2D(const BLB_Light2D *light) {

  if (!light || !light->object)
    return HMM_V2(1.0f, 0.0f);

  float angle = HMM_AngleDeg(light->object->rotation);

  return HMM_NormV2(HMM_V2(cosf(angle), sinf(angle)));
}
