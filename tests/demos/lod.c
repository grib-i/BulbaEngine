#include "lod.h"

int Demo_Lod(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "LOD: rotating spheres with explicit levels", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 11);

  for (int i = 0; i < (int)Demo_Count(&demo, 4); ++i) {
    BLB_Object3D *object =
        Demo_Add3D(&demo, BLB_CreateSphere3D(BLB_V3(1.4f, 1.4f, 1.4f), BLB_V3((i - 1.5f) * 2, 1, 0), demo.settings.detail, NULL, BLB_TEXMAP_UV));
    if (object) {
      if (!BLB_Object3D_SetLODLevel(object, i % BLB_OBJECT_LOD_LEVELS))
        demo.failed = true;
      BLB_Object3D_SetColor(object, BLB_ColorRGBA((unsigned char)(70 + i * 50.0f), 180, 230, 255));
    }
  }
  Demo_Light(&demo);
  return Demo_Finish(&demo, Demo_Spin);
}
