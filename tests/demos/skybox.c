#include "skybox.h"

typedef struct {
  BLB_Object3D *saturn;
} Saturn_Study;

static void animate(Demo_Context *demo) {
  Saturn_Study *study = demo->user_data;
  float time = (float)demo->elapsed;

  /* Rotate the surface around the tilted spin axis; rings keep their common plane. */
  BLB_Object3D_SetRotation(study->saturn, BLB_V3(0, time * 8, 26.73f));
}

int Demo_Skybox(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "SATURN / ice rings and distant sunlight", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stars(&demo);
  Demo_AimCamera(&demo, BLB_V3(0, 1.8f, 5.3f), BLB_V3(0, 0, 0));
  Demo_Light(&demo);

  if (demo.failed)
    return Demo_Finish(&demo, NULL);

  BLB_Light3D *sunlight = demo.scene->lights3d[0];

  sunlight->ambient = .055f;
  sunlight->intensity = 2.1f * demo.settings.light_intensity;
  BLB_Object3D_SetRotation(sunlight->object, BLB_V3(-30, -45, 0));
  BLB_Object3D_SetColor(sunlight->object, BLB_ColorRGBA(255, 246, 228, 255));

  float radius = 1.4f * demo.settings.solar_radius_scale;
  Saturn_Study *study = calloc(1, sizeof(*study));

  if (!study) {
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }

  demo.user_data = study;
  study->saturn = Demo_Planet(&demo, "saturn", BLB_V3(0, 0, 0), radius);
  BLB_Object3D *rings = Demo_SaturnRings(&demo, BLB_V3(0, 0, 0), radius);

  if (demo.failed)
    return Demo_Finish(&demo, NULL);

  /* Saturn's polar radius is about 90.2% of its equatorial radius. */
  BLB_Object3D_SetScale(study->saturn, BLB_V3(2 * radius, 1.804f * radius, 2 * radius));
  BLB_Object3D_SetRotation(study->saturn, BLB_V3(0, 0, 26.73f));
  BLB_Object3D_SetRotation(rings, BLB_V3(0, 0, 26.73f));

  /* Directional illumination approximates the Sun at Saturn's distance. */
  BLB_Vec3 direction = BLB_GetLightDirection3D(sunlight);
  BLB_Object3D *sun = Demo_Planet(&demo, "sun", BLB_MulV3F(direction, -32), .28f);

  if (sun) {
    BLB_Material_SetUnlit(sun->material, true);
    /* The light-source marker must not cast an eclipse onto its own subject. */
    BLB_Material_SetRenderMode(sun->material, BLB_RENDER_TRANSPARENT);
    BLB_Material_SetAlphaMode(sun->material, BLB_ALPHA_BLEND);
  }

  Demo_Text(&demo, "CASSINI DIVISION / translucent ice rings / 26.73 degree axial tilt", 20, 650, 18);
  Demo_Text(&demo, "Solar System Scope (CC BY 4.0) / NASA starfield / --radius-scale / --light / --camera", 20, 680, 16);
  return Demo_Finish(&demo, animate);
}
