#include "transparency_3d.h"

static void animate(Demo_Context *demo) {
  for (int i = 0; i < demo->scene->object3d_count; ++i) {
    if (Demo_IsStage(demo, demo->scene->objects3d[i]))
      continue;

    /* Scene arrays are sorted by the renderer; use a stable spatial phase. */
    float angle = (float)demo->elapsed * 18 + demo->scene->objects3d[i]->position.x * 34;

    BLB_Object3D_SetRotation(demo->scene->objects3d[i], BLB_V3(0, angle, 12));
    Demo_GroundObject(demo->scene->objects3d[i], 0, 0.04f);
  }
}

int Demo_Transparency3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "CHROMATIC GLASS / 3D alpha blending", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 12);
  Demo_Light(&demo);
  unsigned count = Demo_Count(&demo, 7);

  for (unsigned i = 0; i < count; ++i) {
    BLB_Object3D *pane = Demo_Cube(&demo, ((float)i - ((float)count - 1) * 0.5f) * 0.7f, 1.5f, 0, 1);

    if (!pane)
      break;

    BLB_Object3D_SetScale(pane, BLB_V3(1.9f, 2.7f, 0.12f));
    Demo_Color(&demo, pane, BLB_V4(65 + i % 4 * 50, 215 - i % 3 * 45, 235, 255), 0.1f, 0.16f);
    BLB_Material_SetBaseColor(pane->material, (65 + i % 4 * 50) / 255.0f, (215 - i % 3 * 45) / 255.0f, 0.92f, demo.settings.opacity);
    BLB_Material_SetAlphaMode(pane->material, BLB_ALPHA_BLEND);
    BLB_Material_SetRenderMode(pane->material, BLB_RENDER_TRANSPARENT);
    BLB_Material_SetDepth(pane->material, true, false);
    BLB_Material_SetDoubleSided(pane->material, true);
  }

  Demo_Text(&demo, "Raster alpha blending, not physical refraction / --opacity 0..1 / --camera orbit", 20, 680, 17);
  return Demo_Finish(&demo, animate);
}
