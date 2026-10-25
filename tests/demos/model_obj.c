#include "model_obj.h"

int Demo_ModelObj(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "OBJ model import", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 7.5f);
  BLB_Object3D *object = Demo_Add3D(&demo, BLB_LoadModelOBJ(demo.settings.model_path ? demo.settings.model_path : "assets/tests/models/teapot.obj",
                                                            BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV));
  Demo_FitModel(object, demo.settings.model_size);
  Demo_Light(&demo);
  return Demo_Finish(&demo, Demo_Spin);
}
