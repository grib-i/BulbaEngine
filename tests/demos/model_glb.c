#include "model_glb.h"

int Demo_ModelGlb(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "GLB model import", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 7.5f);
  BLB_Object3D *object =
      Demo_Add3D(&demo, BLB_LoadModelGLB(demo.settings.model_path ? demo.settings.model_path : "assets/tests/models/music_player.glb",
                                         BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV));
  Demo_FitModel(object, demo.settings.model_size);
  float framing = demo.settings.model_size / 4.6f;

  Demo_AimCamera(&demo, BLB_V3(5 * framing, 3.8f * framing, 8 * framing), BLB_V3(0, 1.9f * framing, 0));
  Demo_Light(&demo);
  return Demo_Finish(&demo, Demo_Spin);
}
