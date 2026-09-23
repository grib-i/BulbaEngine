#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "test_common.h"
#include "tests.h"

#include <bulba/bulba.h>

int BLB_AnimationTest(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Objects", HMM_V3(0.0f, 0.0f, 10.0f), 7, 9, 15) != 0) {
    return -1;
  }

  BLB_Texture **anm = BLB_SpriteListAuto_Load2D("assets/tests/textures/zta-planim.png", 1);
  const float max_sprite_size = 230.0f;
  uint32_t width = BLB_Texture_GetWidth(anm[0]);
  uint32_t height = BLB_Texture_GetHeight(anm[0]);

  float max_dim = (float)(width > height ? width : height);
  float scale = max_dim > 0.0f ? max_sprite_size / max_dim : 1.0f;

  float sprite_width = (float)width * scale;
  float sprite_height = (float)height * scale;

  BLB_Object2D *player = BLB_CreateSquare2D(HMM_V2(5, 5), HMM_V2(0, 0), NULL, false);

  if (player) {
    BLB_SetAnimation(player, anm, 0.09, 4);
    BLB_StartAnimation(player);
    BLB_Object2D_FlipX(player);

    player->layer = 1;
    BLB_AddObject2D(app.scene, player);
  }

  while (!BLB_WindowShouldClose(app.window)) {
    float delta_time = 0.0f;

    int frame = BLB_TestContext_BeginFrame(&app, &delta_time);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroySquare2D(player);

  BLB_TestContext_Shutdown(&app);

  return 0;
}
