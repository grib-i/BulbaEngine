#include "lights_2d.h"

static void animate(Demo_Context *demo) {
  if (demo->scene->light2d_count)
    BLB_Object2D_SetPosition(demo->scene->lights2d[0]->object, BLB_V2(640 + 400 * sinf((float)demo->elapsed), 350));
}

int Demo_Lights2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "2D point light moving over textured tiles", settings))
    return Demo_Finish(&demo, NULL);

  BLB_Texture *texture = BLB_Texture_Load2D("assets/tests/textures/cc0/red_brick.png");

  if (!texture)
    demo.failed = true;

  for (int i = 0; i < (int)Demo_Count(&demo, 12) && texture; ++i) {
    BLB_Object2D *object = Demo_Square(&demo, 220 + (i % 6) * 165.0f, 280 + (i / 6) * 200.0f, 150, 180);
    BLB_Material *material = BLB_Material_Create2D();

    if (object && material) {
      BLB_Object2D_SetTexture(object, texture);
      BLB_Material_SetLighting(material, true);
      BLB_Material_SetUnlit(material, false);
      BLB_Object2D_SetMaterial(object, material);
    } else
      demo.failed = true;
    BLB_Material_Release(material);
  }
  BLB_Texture_Release(texture);
  BLB_Light2D *light = BLB_CreateLight2D(BLB_LIGHT_POINT, BLB_V2(640, 350), 0);

  if (!light || BLB_AddLight2D(demo.scene, light) != 0) {
    BLB_DestroyLight2D(light);
    demo.failed = true;
  } else {
    light->intensity = 1.4f * demo.settings.light_intensity;
    light->specular = .12f;
    light->ambient = 0.15f;
    light->range = 700;
    light->object->visible = false;
    BLB_Object2D_SetCameraAttached(light->object, true);
  }
  return Demo_Finish(&demo, animate);
}
