#include "primitives_3d.h"
#include "bulba/core/math3v/ray.h"

int Demo_Primitives3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "FORM STUDY / cube, sphere & torus", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 11);
  BLB_Object3D *cube = Demo_Cube(&demo, -2.5f, 1, 0, 1.5f);

  Demo_Color(&demo, cube, BLB_V4(200, 120, 60, 255), 0.7f, 0.32f);

  BLB_Object3D *sphere = Demo_Add3D(
      &demo, BLB_CreateSphere3D(BLB_V3(1.6f, 1.6f, 1.6f), BLB_V3(0, 1, 0),
                                demo.settings.detail, NULL, BLB_TEXMAP_UV));

  Demo_Color(&demo, sphere, BLB_V4(65, 185, 194, 255), 0.1f, 0.24f);

  BLB_Object3D *torus =
      Demo_Add3D(&demo, BLB_CreateTorus3D(BLB_V3(1, 1, 1), BLB_V3(2.5f, 1, 0),
                                          demo.settings.detail, 1, 0.4f, NULL,
                                          BLB_TEXMAP_UV));

  Demo_Color(&demo, torus, BLB_V4(225, 222, 210, 255), 0.7f, 0.2f);

  Demo_Text(&demo,
            "BRONZE / CERAMIC / SATIN METAL     |     --transforms on/off", 20,
            680, 18);
  Demo_Light(&demo);
  return Demo_Finish(&demo, Demo_Spin);
}
