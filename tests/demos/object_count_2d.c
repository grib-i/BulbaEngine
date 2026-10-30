#include "object_count_2d.h"

static void animate(Demo_Context *demo) {
  for (int i = 0; i < demo->scene->object2d_count; ++i) {
    BLB_Object2D *object = demo->scene->objects2d[i];

    BLB_Object2D_SetRotation(object, sinf((float)demo->elapsed + i * 0.015f) * 35);
  }
}

int Demo_ObjectCount2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "2D FIELD / batched colored tiles", settings))
    return Demo_Finish(&demo, NULL);

  unsigned count = Demo_Count(&demo, 2000);
  unsigned columns = (unsigned)ceilf(sqrtf(count * 2.2f));
  unsigned rows = (count + columns - 1) / columns;
  float dx = 1200.0f / columns, dy = 490.0f / rows;

  for (unsigned i = 0; i < count && !demo.failed; ++i) {
    BLB_Object2D *object = Demo_Square(&demo, 40 + (i % columns + 0.5f) * dx, 145 + (i / columns + 0.5f) * dy, dx * 0.65f, dy * 0.65f);

    if (object)
      BLB_Object2D_SetColor(object, BLB_ColorRGBA((unsigned char)(60 + i % columns * 150.0f / columns), 180, 235, 255));
  }

  Demo_Text(&demo, "Screen-space canvas / camera intentionally fixed / configurable tile count & transforms", 20, 680, 17);
  return Demo_Finish(&demo, animate);
}
