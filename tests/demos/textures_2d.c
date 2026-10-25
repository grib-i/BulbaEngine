#include "textures_2d.h"

int Demo_Textures2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "2D textures from assets/tests", settings))
    return Demo_Finish(&demo, NULL);

  const char *paths[] = {"assets/tests/textures/cc0/red_brick.png", "assets/tests/textures/cc0/wood_planks.png",
                         "assets/tests/textures/planets/mars.png"};

  for (size_t i = 0; i < 3; ++i) {
    BLB_Texture *texture = BLB_Texture_Load2D(paths[i]);
    BLB_Object2D *object = Demo_Square(&demo, 260 + i * 360.0f, 370, 280, 280);

    if (!texture || !object)
      demo.failed = true;
    else
      BLB_Object2D_SetTexture(object, texture);
    BLB_Texture_Release(texture);
  }
  return Demo_Finish(&demo, NULL);
}
