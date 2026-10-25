#include "materials_3d.h"

int Demo_Materials3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "MATERIAL ATLAS / metallic x roughness", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 9);

  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 5; ++column) {
      BLB_Object3D *object = Demo_Add3D(&demo, BLB_CreateSphere3D(BLB_V3(1.1f, 1.1f, 1.1f), BLB_V3((column - 2) * 1.45f, 0.59f, (row - 1) * 1.35f),
                                                                  demo.settings.detail, NULL, BLB_TEXMAP_UV));
      BLB_Material *material = BLB_Material_Create3D();

      if (!object || !material)
        demo.failed = true;
      else {
        BLB_Material_SetBaseColor(material, 0.82f, 0.54f, 0.22f, 1);
        BLB_Material_SetPBR(material, row / 2.0f, 0.05f + column * 0.23f);
        BLB_Object3D_SetMaterial(object, material);
      }
      BLB_Material_Release(material);
    }
  }
  Demo_AimCamera(&demo, BLB_V3(3.7f, 5, 7.8f), BLB_V3(0, 0.35f, 0));
  Demo_Text(&demo, "METALLIC  0 / 0.5 / 1     |     ROUGHNESS  0.05 -> 0.97", 20, 680, 18);
  Demo_Light(&demo);
  return Demo_Finish(&demo, NULL);
}
