#include "test_common.h"
#include "tests.h"

#include <debug.h>
#include <stdlib.h>

int BLB_TestSpriteSheet(void) {
  BLB_TestContext app;

  printd("[SpriteSheet] START\n");

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Sprite Sheet Test", HMM_V3(0.0f, 0.0f, 10.0f), 8, 8, 12) != 0) {

    printd("[SpriteSheet] FAIL: BLB_TestContext_Init()\n");
    return -1;
  }

  printd("[SpriteSheet] TestContext initialized\n");

  BLB_Texture **textures = BLB_SpriteListAuto_Load2D("assets/tests/textures/zta-stones.png", 1);

  printd("[SpriteSheet] BLB_SpriteListAuto_Load2D() -> %p\n", (void *)textures);

  if (!textures) {
    printd("[SpriteSheet] FAIL: textures == NULL\n");

    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  size_t count = 0;

  printd("[SpriteSheet] Counting textures...\n");

  while (textures[count]) {
    printd("[SpriteSheet] textures[%zu] = %p\n", count, (void *)textures[count]);

    count++;
  }

  printd("[SpriteSheet] textures[%zu] = NULL\n", count);
  printd("[SpriteSheet] texture count = %zu\n", count);

  if (count == 0) {
    printd("[SpriteSheet] FAIL: texture count == 0\n");

    BLB_SpriteList_Destroy(textures);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Object2D **objects = calloc(count, sizeof(*objects));

  printd("[SpriteSheet] calloc objects[%zu] -> %p\n", count, (void *)objects);

  if (!objects) {
    printd("[SpriteSheet] FAIL: calloc(objects) returned NULL\n");

    BLB_SpriteList_Destroy(textures);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  const size_t columns = 4;
  const float slot_width = 300.0f;
  const float slot_height = 270.0f;
  const float max_sprite_size = 230.0f;

  for (size_t i = 0; i < count; ++i) {
    printd("[SpriteSheet] ---- sprite %zu/%zu ----\n", i + 1, count);

    printd("[SpriteSheet] texture = %p\n", (void *)textures[i]);

    if (!textures[i]) {
      printd("[SpriteSheet] FAIL: textures[%zu] == NULL\n", i);

      for (size_t j = 0; j < i; ++j) {
        if (objects[j])
          BLB_DestroySquare2D(objects[j]);
      }

      free(objects);
      BLB_SpriteList_Destroy(textures);
      BLB_TestContext_Shutdown(&app);
      return -1;
    }

    uint32_t width = BLB_Texture_GetWidth(textures[i]);
    uint32_t height = BLB_Texture_GetHeight(textures[i]);

    printd("[SpriteSheet] texture[%zu] size = %ux%u\n", i, width, height);

    float max_dim = (float)(width > height ? width : height);

    float scale = max_dim > 0.0f ? max_sprite_size / max_dim : 1.0f;

    float sprite_width = (float)width * scale;

    float sprite_height = (float)height * scale;

    size_t row = i / columns;
    size_t column = i % columns;

    HMM_Vec2 position = HMM_V2(slot_width * 0.5f + (float)column * slot_width, 70.0f + slot_height * 0.5f + (float)row * slot_height);

    printd("[SpriteSheet] sprite[%zu]: row=%zu column=%zu "
           "scale=%f size=(%f,%f) position=(%f,%f)\n",
           i, row, column, scale, sprite_width, sprite_height, position.x, position.y);

    objects[i] = BLB_CreateSquare2D(HMM_V2(sprite_width, sprite_height), position, textures[i], true);

    printd("[SpriteSheet] BLB_CreateSquare2D[%zu] -> %p\n", i, (void *)objects[i]);

    if (!objects[i]) {
      printd("[SpriteSheet] FAIL: BLB_CreateSquare2D[%zu] returned NULL\n", i);

      for (size_t j = 0; j < i; ++j) {
        if (objects[j])
          BLB_DestroySquare2D(objects[j]);
      }

      free(objects);
      BLB_SpriteList_Destroy(textures);
      BLB_TestContext_Shutdown(&app);
      return -1;
    }

    objects[i]->layer = (unsigned short)(i + 1);

    printd("[SpriteSheet] object[%zu] layer = %u\n", i, objects[i]->layer);

    printd("[SpriteSheet] object[%zu] texture = %p\n", i, (void *)objects[i]->texture);

    printd("[SpriteSheet] object[%zu] polygon = %p\n", i, (void *)objects[i]->polygon);

    BLB_AddObject2D(app.scene, objects[i]);

    printd("[SpriteSheet] BLB_AddObject2D[%zu] done\n", i);
  }

  printd("[SpriteSheet] All %zu sprites created successfully\n", count);

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;

    int frame = BLB_TestContext_BeginFrame(&app, &dt);

    if (frame < 0) {
      printd("[SpriteSheet] FAIL: BLB_TestContext_BeginFrame() -> %d\n", frame);
      break;
    }

    if (frame > 0)
      continue;

    if (BLB_TestContext_Draw(&app) < 0) {
      printd("[SpriteSheet] FAIL: BLB_TestContext_Draw()\n");
      break;
    }
  }

  printd("[SpriteSheet] Destroying objects...\n");

  for (size_t i = 0; i < count; ++i) {
    if (objects[i]) {
      printd("[SpriteSheet] destroying object[%zu] = %p\n", i, (void *)objects[i]);

      BLB_DestroySquare2D(objects[i]);
    }
  }

  free(objects);

  printd("[SpriteSheet] Destroying texture list = %p\n", (void *)textures);

  BLB_SpriteList_Destroy(textures);

  printd("[SpriteSheet] Shutting down test context\n");

  BLB_TestContext_Shutdown(&app);

  printd("[SpriteSheet] SUCCESS\n");

  return 0;
}
