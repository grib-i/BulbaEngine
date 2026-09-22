#include "tests.h"
#include "test_common.h"

int BLB_TestSpriteSheet(void) {
  BLB_TestContext app;
  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Sprite Sheet Test", HMM_V3(0.0f, 0.0f, 10.0f), 8, 8, 12) != 0)
    return -1;

  BLB_Texture **textures = BLB_SpriteListAuto_Load2D("assets/tests/textures/zta-stones.png", 1);
  if (!textures) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  size_t count = 0;
  while (textures[count])
    count++;

  if (count == 0) {
    BLB_SpriteList_Destroy(textures);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Object2D **objects = calloc(count, sizeof(*objects));
  if (!objects) {
    BLB_SpriteList_Destroy(textures);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  const size_t columns = 4;
  const float slot_width = 300.0f;
  const float slot_height = 270.0f;
  const float max_sprite_size = 230.0f;

  for (size_t i = 0; i < count; ++i) {
    uint32_t width = BLB_Texture_GetWidth(textures[i]);
    uint32_t height = BLB_Texture_GetHeight(textures[i]);
    float max_dim = (float)(width > height ? width : height);
    float scale = max_dim > 0.0f ? max_sprite_size / max_dim : 1.0f;
    float sprite_width = (float)width * scale;
    float sprite_height = (float)height * scale;
    size_t row = i / columns;
    size_t column = i % columns;
    HMM_Vec2 position = HMM_V2(slot_width * 0.5f + (float)column * slot_width,
                                70.0f + slot_height * 0.5f + (float)row * slot_height);

    objects[i] = BLB_CreateSquare2D(HMM_V2(sprite_width, sprite_height), position, textures[i], true);
    if (!objects[i]) {
      for (size_t j = 0; j < i; ++j)
        BLB_DestroySquare2D(objects[j]);
      free(objects);
      BLB_SpriteList_Destroy(textures);
      BLB_TestContext_Shutdown(&app);
      return -1;
    }

    objects[i]->layer = (unsigned short)(i + 1);
    BLB_Material_SetLighting(objects[i]->material, false);
    BLB_Material_SetUnlit(objects[i]->material, true);
    BLB_Material_SetDepth(objects[i]->material, false, false);
    BLB_Material_SetDoubleSided(objects[i]->material, true);
    BLB_Material_SetAlphaMode(objects[i]->material, BLB_ALPHA_BLEND);
    BLB_Material_SetRenderMode(objects[i]->material, BLB_RENDER_TRANSPARENT);
    BLB_AddObject2D(app.scene, objects[i]);
  }

  BLB_SpriteList_Destroy(textures);

  /*
  BLB_Texture *sprite = BLB_SpriteList_Load2D("assets/tests/textures/zta-stones.png", 0, 3, 569, 358, 1);
  if (sprite) {
    BLB_Object2D *object = BLB_CreateSquare2D(HMM_V2(220.0f, 140.0f), HMM_V2(160.0f, 160.0f), sprite, true);
    BLB_Texture_Release(sprite);
    if (object) {
      BLB_Material_SetLighting(object->material, false);
      BLB_Material_SetUnlit(object->material, true);
      BLB_Material_SetDepth(object->material, false, false);
      BLB_Material_SetDoubleSided(object->material, true);
      BLB_Material_SetAlphaMode(object->material, BLB_ALPHA_BLEND);
      BLB_Material_SetRenderMode(object->material, BLB_RENDER_TRANSPARENT);
      BLB_AddObject2D(app.scene, object);
    }
  }
  */

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &dt);
    if (frame < 0)
      break;
    if (frame > 0)
      continue;

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  for (size_t i = 0; i < count; ++i)
    BLB_DestroySquare2D(objects[i]);
  free(objects);
  BLB_TestContext_Shutdown(&app);
  return 0;
}
