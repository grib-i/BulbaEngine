#include "textures_3d.h"

int Demo_Textures3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "3D textures: brick, wood and Mars", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 11);

  const char *paths[] = {"assets/tests/textures/cc0/red_brick.png", "assets/tests/textures/cc0/wood_planks.png",
                         "assets/tests/textures/planets/mars.png"};

  for (int i = 0; i < 3; ++i) {
    BLB_Texture *texture = BLB_Texture_Load2D(paths[i]);
    BLB_Object3D *object =
        i == 2 ? Demo_Add3D(&demo, BLB_CreateSphere3D(BLB_V3(1.6f, 1.6f, 1.6f), BLB_V3(2.5f, 1, 0), demo.settings.detail, NULL, BLB_TEXMAP_UV))
               : Demo_Cube(&demo, (i - 1) * 2.5f, 1, 0, 1.6f);
    if (!texture || !object)
      demo.failed = true;
    else
      BLB_Object3D_SetTexture(object, texture);
    BLB_Texture_Release(texture);
  }
  Demo_Light(&demo);
  return Demo_Finish(&demo, Demo_Spin);
}
