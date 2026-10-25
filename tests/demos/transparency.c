#include "transparency.h"

int Demo_Transparency(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "Transparency: overlapping alpha-blended layers", settings))
    return Demo_Finish(&demo, NULL);

  for (int i = 0; i < (int)Demo_Count(&demo, 8); ++i) {
    BLB_Object2D *object = Demo_Square(&demo, 300 + i * 90.0f, 360, 250, 250);
    BLB_Material *material = BLB_Material_Create2D();

    if (object && material) {
      BLB_Material_SetBaseColor(material, i / 8.0f, 0.6f, 1 - i / 8.0f, demo.settings.opacity);
      BLB_Material_SetAlphaMode(material, BLB_ALPHA_BLEND);
      BLB_Material_SetRenderMode(material, BLB_RENDER_TRANSPARENT);
      BLB_Object2D_SetMaterial(object, material);
    } else
      demo.failed = true;
    BLB_Material_Release(material);
  }
  return Demo_Finish(&demo, NULL);
}
