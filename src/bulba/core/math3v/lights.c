#include "bulba/core/math3v/lights.h"

#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/objects3d/cube.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/config.h"

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

BLB_Light3D *BLB_CreateLight3D(BLB_LightType type, BLB_Vec3 position, BLB_Vec3 rotation) {
  BLB_Light3D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->type = type;
  light->enabled = true;
  light->intensity = 1.0f;
  light->ambient = 0.05f;
  light->specular = 1.0f;
  light->shininess = 32.0f;
  light->range = 20.0f;
  light->inner_cone = cosf(BLB_AngleDeg(20.0f));
  light->outer_cone = cosf(BLB_AngleDeg(30.0f));

  light->light_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_256dp_width, BLB_TEXTURE_light_256dp_height, BLB_TEXTURE_light_256dp_pixels,
                                                    BLB_TEXTURE_light_256dp_pixel_size);

  if (!light->light_debug_texture) {
    free(light);

    return NULL;
  }

  light->light_debug_texture->clamp_to_edge = true;
  light->light_off_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_off_256dp_width, BLB_TEXTURE_light_off_256dp_height,
                                                        BLB_TEXTURE_light_off_256dp_pixels, BLB_TEXTURE_light_off_256dp_pixel_size);

  if (!light->light_off_debug_texture) {
    BLB_Texture_Release(light->light_debug_texture);
    free(light);

    return NULL;
  }

  light->light_off_debug_texture->clamp_to_edge = true;
  light->object = BLB_CreateCube3D(BLB_V3(1.0f, 1.0f, 0.001f), position, light->light_debug_texture, BLB_TEXMAP_STRETCH);

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

BLB_Light2D *BLB_CreateLight2D(BLB_LightType type, BLB_Vec2 position, float rotation) {
  BLB_Light2D *light = calloc(1, sizeof(*light));

  if (!light)
    return NULL;

  light->type = type;
  light->enabled = true;
  light->intensity = 1.0f;
  light->ambient = 0.05f;
  light->specular = 1.0f;
  light->shininess = 32.0f;
  light->range = 20.0f;
  light->inner_cone = cosf(BLB_AngleDeg(20.0f));
  light->outer_cone = cosf(BLB_AngleDeg(30.0f));

  light->light_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_256dp_width, BLB_TEXTURE_light_256dp_height, BLB_TEXTURE_light_256dp_pixels,
                                                    BLB_TEXTURE_light_256dp_pixel_size);

  if (!light->light_debug_texture) {
    free(light);

    return NULL;
  }

  light->light_debug_texture->clamp_to_edge = true;
  light->light_off_debug_texture = BLB_Texture_Create2D(BLB_TEXTURE_light_off_256dp_width, BLB_TEXTURE_light_off_256dp_height,
                                                        BLB_TEXTURE_light_off_256dp_pixels, BLB_TEXTURE_light_off_256dp_pixel_size);

  if (!light->light_off_debug_texture) {
    BLB_Texture_Release(light->light_debug_texture);
    free(light);

    return NULL;
  }

  light->light_off_debug_texture->clamp_to_edge = true;

  light->object = BLB_CreateSquare2D(BLB_V2(1.0f, 1.0f), position, light->light_debug_texture, false);

  if (!light->object) {
    BLB_Texture_Release(light->light_debug_texture);
    BLB_Texture_Release(light->light_off_debug_texture);
    free(light);

    return NULL;
  }

  light->object->visible = BLB_DEBUG;
  BLB_Material_SetLighting(light->object->material, false);
  BLB_Material_SetUnlit(light->object->material, true);
  BLB_Material_SetDoubleSided(light->object->material, true);
  BLB_Material_SetRenderMode(light->object->material, BLB_RENDER_TRANSPARENT);

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

BLB_Vec3 BLB_GetLightDirection3D(const BLB_Light3D *light) {

  if (!light || !light->object)
    return BLB_V3(0.0f, 0.0f, -1.0f);

  BLB_Mat4 rx = BLB_Rotate_RH(BLB_AngleDeg(light->object->rotation.x), BLB_V3(1.0f, 0.0f, 0.0f));
  BLB_Mat4 ry = BLB_Rotate_RH(BLB_AngleDeg(light->object->rotation.y), BLB_V3(0.0f, 1.0f, 0.0f));
  BLB_Mat4 rz = BLB_Rotate_RH(BLB_AngleDeg(light->object->rotation.z), BLB_V3(0.0f, 0.0f, 1.0f));

  BLB_Mat4 rotation = BLB_MulM4(rz, BLB_MulM4(ry, rx));
  BLB_Vec4 forward = BLB_V4(0.0f, 0.0f, -1.0f, 0.0f);
  BLB_Vec4 direction = BLB_MulM4V4(rotation, forward);

  return BLB_NormV3(BLB_V3(direction.x, direction.y, direction.z));
}

BLB_Vec2 BLB_GetLightDirection2D(const BLB_Light2D *light) {

  if (!light || !light->object)
    return BLB_V2(1.0f, 0.0f);

  float angle = BLB_AngleDeg(light->object->rotation);

  return BLB_NormV2(BLB_V2(cosf(angle), sinf(angle)));
}
