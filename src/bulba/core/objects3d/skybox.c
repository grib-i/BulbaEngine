#include "bulba/core/objects3d/skybox.h"

#include "bulba/core/objects3d/cube.h"
#include "bulba/core/render/material.h"

#include <stdlib.h>

BLB_Skybox *BLB_Skybox_Create(BLB_Texture *texture) {
  if (!texture || (texture->type != BLB_TEXTURE_2D && texture->type != BLB_TEXTURE_3D))
    return NULL;

  BLB_Skybox *skybox = calloc(1, sizeof(*skybox));

  if (!skybox)
    return NULL;

  skybox->object = BLB_CreateCube3D(BLB_V3(1.0f, 1.0f, 1.0f), BLB_V3(0.0f, 0.0f, 0.0f), texture, BLB_TEXMAP_STRETCH);

  if (!skybox->object) {
    free(skybox);

    return NULL;
  }

  BLB_Material *material = skybox->object->material;

  if (!material) {
    BLB_DestroyCube3D(skybox->object);
    free(skybox);

    return NULL;
  }

  skybox->fov_scale = 0.5f;

  BLB_Material_SetLighting(material, false);
  BLB_Material_SetUnlit(material, true);
  BLB_Material_SetDepth(material, false, false);
  BLB_Material_SetDoubleSided(material, true);
  BLB_Material_SetAlphaMode(material, BLB_ALPHA_OPAQUE);
  BLB_Material_SetRenderMode(material, BLB_RENDER_OPAQUE);
  BLB_Material_SetBaseColor(material, 1.0f, 1.0f, 1.0f, 1.0f);

  if (texture->type == BLB_TEXTURE_2D) {
    BLB_MaterialTexture_SetSampler(&material->base_color_texture, BLB_TEXTURE_WRAP_REPEAT, BLB_TEXTURE_WRAP_CLAMP_TO_EDGE, BLB_TEXTURE_FILTER_LINEAR,
                                   BLB_TEXTURE_FILTER_LINEAR);
  }

  return skybox;
}

BLB_Skybox *BLB_Skybox_Load2D(const char *path) {
  BLB_Texture *texture = BLB_Texture_Load2D(path);

  if (!texture)
    return NULL;

  BLB_Skybox *skybox = BLB_Skybox_Create(texture);
  BLB_Texture_Release(texture);

  return skybox;
}

BLB_Skybox *BLB_Skybox_Load3D(const char *path, uint32_t width, uint32_t height, uint32_t depth) {
  BLB_Texture *texture = BLB_Texture_Load3D(path, width, height, depth);

  if (!texture)
    return NULL;

  BLB_Skybox *skybox = BLB_Skybox_Create(texture);
  BLB_Texture_Release(texture);

  return skybox;
}

void BLB_Skybox_Destroy(BLB_Skybox *skybox) {
  if (!skybox)
    return;

  BLB_DestroyCube3D(skybox->object);
  free(skybox);
}

void BLB_Skybox_SetFovScale(BLB_Skybox *skybox, float fov_scale) {
  if (!skybox)
    return;

  skybox->fov_scale = fov_scale;
}
