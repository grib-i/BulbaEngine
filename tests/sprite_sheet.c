#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "test_common.h"
#include "tests.h"

#include <stdlib.h>

int BLB_TestSpriteSheet(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Sprite Sheet Test", HMM_V3(0.0f, 0.0f, 10.0f), 8, 8, 12) != 0)
    return -1;

  BLB_Texture **textures = BLB_SpriteListAuto_Load2D("assets/tests/textures/zta-stones.png", 1);
  BLB_Texture *sheet_texture = BLB_Texture_Load2D("assets/tests/textures/zta-stones.png");

  if (!textures || !sheet_texture) {
    BLB_SpriteList_Destroy(textures);
    BLB_Texture_Destroy(sheet_texture);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  size_t count = 0;
  while (textures[count])
    count++;

  if (count == 0) {
    BLB_SpriteList_Destroy(textures);
    BLB_Texture_Destroy(sheet_texture);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Object2D **objects = calloc(count, sizeof(*objects));
  if (!objects) {
    BLB_SpriteList_Destroy(textures);
    BLB_Texture_Destroy(sheet_texture);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  const size_t columns = 4;
  const float slot_width = 0.38f;
  const float slot_height = 0.27f;
  const float max_sprite_size = 0.22f;

  for (size_t i = 0; i < count; ++i) {
    uint32_t width = BLB_Texture_GetWidth(textures[i]);
    uint32_t height = BLB_Texture_GetHeight(textures[i]);

    if (width == 0 || height == 0)
      goto fail;

    float max_dim = (float)(width > height ? width : height);
    float scale = max_dim > 0.0f ? max_sprite_size / max_dim : 1.0f;
    float sprite_width = (float)width * scale;
    float sprite_height = (float)height * scale;

    size_t row = i / columns;
    size_t column = i % columns;

    HMM_Vec2 position = HMM_V2(-0.57f + (float)column * slot_width, 0.405f - (float)row * slot_height);

    objects[i] = BLB_CreateSquare2D(HMM_V2(sprite_width, sprite_height), position, textures[i], true);
    if (!objects[i])
      goto fail;

    objects[i]->layer = (unsigned short)(i + 1);
    BLB_AddObject2D(app.scene, objects[i]);
  }

  {
    uint32_t width = BLB_Texture_GetWidth(sheet_texture);
    uint32_t height = BLB_Texture_GetHeight(sheet_texture);

    float max_dim = (float)(width > height ? width : height);
    float scale = max_dim > 0.0f ? 0.10f / max_dim : 1.0f;

    BLB_Object2D *sheet = BLB_CreateSquare2D(HMM_V2((float)width * scale, (float)height * scale), HMM_V2(0.0f, -0.60f), sheet_texture, true);
    if (!sheet)
      goto fail;

    sheet->layer = 1000;
    BLB_AddObject2D(app.scene, sheet);
  }

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

  for (size_t i = 0; i < count; ++i) {
    if (objects[i])
      BLB_DestroySquare2D(objects[i]);
  }

  free(objects);
  BLB_Texture_Destroy(sheet_texture);
  BLB_SpriteList_Destroy(textures);
  BLB_TestContext_Shutdown(&app);

  return 0;

fail:
  for (size_t i = 0; i < count; ++i) {
    if (objects[i])
      BLB_DestroySquare2D(objects[i]);
  }

  free(objects);
  BLB_Texture_Destroy(sheet_texture);
  BLB_SpriteList_Destroy(textures);
  BLB_TestContext_Shutdown(&app);

  return -1;
}
