#include "sprite_animation.h"

static void animate(Demo_Context *demo) {
  unsigned count = Demo_Count(demo, 5);
  float time = (float)demo->elapsed;
  BLB_Object2D **actors = demo->user_data;

  for (unsigned i = 0; i < count; ++i) {
    BLB_Object2D *actor = actors[i];
    float phase = time * 0.65f + i * 2.1f;
    float direction = cosf(phase) >= 0 ? 1 : -1;

    BLB_Vec2 position = BLB_V2(640 + sinf(phase) * 400, 470);
    if (i % 5 == 3) {
      position = BLB_V2(1120, 370 + sinf(phase * 2) * 65);
      direction = 1;
    } else if (i % 5 == 4) {
      position.y -= fabsf(sinf(phase * 2)) * 120;
    }

    BLB_Object2D_SetPosition(actor, position);
    BLB_Object2D_SetScale(actor, BLB_V2(128 * direction, 128));
  }
}

int Demo_SpriteAnimation(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "SPRITE LAB / atlas animation & character parade", settings))
    return Demo_Finish(&demo, NULL);

  BLB_SetSceneClear(demo.scene, true, BLB_ColorRGBA(18, 29, 49, 255));
  Demo_Text(&demo, "KENNEY CHARACTERS   /   WALK + CLIMB CYCLES", 250, 170, 22);
  Demo_Text(&demo, "Real RGBA atlas frames / mirrored movement / layered scenery", 250, 210, 17);

  /* Three depth bands provide an inexpensive parallax-style stage. */
  for (int row = 0; row < 3; ++row) {
    for (int i = 0; i < 12; ++i) {
      float height = 70 + (i * 47 + row * 31) % 150;
      BLB_Object2D *tower = Demo_Square(&demo, i * 115.0f, 490 - height * 0.5f - row * 22, 70, height);

      if (tower)
        BLB_Object2D_SetColor(tower, BLB_ColorRGBA((unsigned char)(22 + row * 8), (unsigned char)(42 + row * 10), (unsigned char)(65 + row * 15), 255));
    }
  }

  BLB_Object2D *ground = Demo_Square(&demo, 640, 600, 1280, 120);
  BLB_Object2D *edge = Demo_Square(&demo, 640, 540, 1280, 3);

  if (ground)
    BLB_Object2D_SetColor(ground, BLB_ColorRGBA(15, 23, 38, 255));
  if (edge)
    BLB_Object2D_SetColor(edge, BLB_ColorRGBA(68, 212, 187, 255));

  for (int i = 0; i < 10; ++i) {
    BLB_Object2D *rung = Demo_Square(&demo, 1120, 305 + i * 25, 72, 5);
    if (rung)
      BLB_Object2D_SetColor(rung, BLB_ColorRGBA(80, 128, 151, 255));
  }

  for (int side = -1; side <= 1; side += 2) {
    BLB_Object2D *rail = Demo_Square(&demo, 1120 + side * 32, 420, 5, 250);
    if (rail)
      BLB_Object2D_SetColor(rail, BLB_ColorRGBA(104, 168, 184, 255));
  }

  BLB_Object2D **actors = calloc(Demo_Count(&demo, 5), sizeof(*actors));

  if (!actors) {
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }

  demo.user_data = actors;
  const char *sheets[] = {"assets/tests/sprites/adventurer_sheet.png", "assets/tests/sprites/female_sheet.png",
                          "assets/tests/sprites/zombie_sheet.png"};
  unsigned count = Demo_Count(&demo, 5);

  for (unsigned i = 0; i < count && !demo.failed; ++i) {
    BLB_Texture *frames[2] = {0};
    unsigned first = i % 5 == 3 ? 2 : (i % 5 == 4 ? 4 : 0);

    for (unsigned frame = 0; frame < 2; ++frame) {
      frames[frame] = BLB_SpriteList_Load2D(sheets[i % 3], (first + frame) * 128, 0, 128, 128, 0);
      if (!frames[frame])
        demo.failed = true;
    }

    BLB_Object2D *actor = Demo_Square(&demo, 300 + i % 8 * 90, 470 - i % 3 * 28, 128, 128);

    actors[i] = actor;

    if (actor && !demo.failed) {
      BLB_Material_SetAlphaMode(actor->material, BLB_ALPHA_BLEND);
      BLB_Material_SetRenderMode(actor->material, BLB_RENDER_TRANSPARENT);
      BLB_SetAnimation(actor, frames, demo.settings.sprite_frame_time / fmaxf(demo.settings.animation_speed, 0.001f), 2);
      BLB_StartAnimation(actor);
    }

    for (unsigned frame = 0; frame < 2; ++frame)
      BLB_Texture_Release(frames[frame]);
  }

  /* Retain the color-frame API example as a separate, intentionally small swatch. */
  BLB_Texture *colors[8] = {0};

  for (unsigned i = 0; i < 8; ++i) {
    unsigned char pixel[] = {(unsigned char)(40 + i * 26), (unsigned char)(220 - i * 17), 180, 255};

    colors[i] = BLB_Texture_Create2D(1, 1, pixel, sizeof(pixel));
    if (!colors[i])
      demo.failed = true;
  }

  BLB_Object2D *swatch = Demo_Square(&demo, 300, 630, 42, 42);

  if (swatch && !demo.failed) {
    BLB_SetAnimation(swatch, colors, demo.settings.sprite_frame_time, 8);
    BLB_StartAnimation(swatch);
  }

  for (unsigned i = 0; i < 8; ++i)
    BLB_Texture_Release(colors[i]);

  Demo_Text(&demo, "Color-frame API sample", 345, 636, 18);
  return Demo_Finish(&demo, animate);
}
