#include "primitives_2d.h"

static void animate(Demo_Context *demo) {
  for (int i = 0; i < demo->scene->object2d_count; ++i) {
    BLB_Object2D *object = demo->scene->objects2d[i];

    BLB_Object2D_SetRotation(object, (float)demo->elapsed * (i % 2 ? 45 : -45));
    BLB_Object2D_SetPosition(object, BLB_V2(60 + (i + 0.5f) * 1160 / Demo_Count(demo, 8), 350 + 60 * sinf((float)demo->elapsed + i)));
  }
}

int Demo_Primitives2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "2D primitives: transforms and layers", settings))
    return Demo_Finish(&demo, NULL);

  for (int i = 0; i < (int)Demo_Count(&demo, 8); ++i) {
    BLB_Object2D *object = (i % 2) ? Demo_Square(&demo, 60 + (i + 0.5f) * 1160 / Demo_Count(&demo, 8), 350, 90, 90)
                                   : Demo_Add2D(&demo, BLB_CreateCircle2D(BLB_V2(95, 95), BLB_V2(60 + (i + 0.5f) * 1160 / Demo_Count(&demo, 8), 350),
                                                                          demo.settings.detail, NULL, true));
    if (!object)
      break;

    BLB_Object2D_SetColor(object, BLB_V4(50 + i * 25.0f, 180, 240 - i * 18.0f, 255));
  }
  return Demo_Finish(&demo, animate);
}
