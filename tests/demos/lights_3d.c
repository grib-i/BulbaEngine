#include "lights_3d.h"

typedef struct Light_Orbit {
  BLB_Light3D *lights[3];
  BLB_Object3D *markers[3];
} Light_Orbit;

static void animate(Demo_Context *demo) {
  Light_Orbit *orbit = demo->user_data;
  float time = (float)demo->elapsed;

  for (int i = 0; i < 3; ++i) {
    float angle = time * 0.8f + i * 2.094f;
    BLB_Vec3 position = BLB_V3(sinf(angle) * 4, 2.4f + sinf(time + i) * 0.8f, cosf(angle) * 3);

    BLB_Object3D_SetPosition(orbit->lights[i]->object, position);
    BLB_Object3D_SetPosition(orbit->markers[i], position);
  }
}

int Demo_Lights3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "LIGHT STUDIO / moving key lights & sculpted materials", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Stage(&demo, 10.5f);
  Demo_Light(&demo);
  if (demo.failed)
    return Demo_Finish(&demo, NULL);

  demo.scene->lights3d[0]->intensity *= 0.3f;
  demo.scene->lights3d[0]->ambient = 0.08f;
  unsigned count = Demo_Count(&demo, 9);

  for (unsigned i = 0; i < count; ++i) {
    float angle = i * 2 * BLB_PI / count;
    BLB_Object3D *object = Demo_Add3D(
        &demo, BLB_CreateSphere3D(BLB_V3(1, 1, 1), BLB_V3(sinf(angle) * 3, 0.65f, cosf(angle) * 2.5f), demo.settings.detail, NULL, BLB_TEXMAP_UV));
    Demo_Color(&demo, object, BLB_V4(75 + i % 3 * 70, 190 - i % 3 * 45, 220, 255), i % 2 ? 0.85f : 0.1f, 0.12f + i % 4 * 0.18f);
  }

  BLB_Object3D *ring =
      Demo_Add3D(&demo, BLB_CreateTorus3D(BLB_V3(1, 1, 1), BLB_V3(0, 1.3f, 0), demo.settings.detail, 1.3f, 0.95f, NULL, BLB_TEXMAP_UV));
  Demo_Color(&demo, ring, BLB_V4(245, 185, 72, 255), 0.85f, 0.15f);

  Light_Orbit *orbit = calloc(1, sizeof(*orbit));

  if (!orbit) {
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }

  demo.user_data = orbit;

  for (int i = 0; i < 3; ++i) {
    BLB_Light3D *light = BLB_CreateLight3D(BLB_LIGHT_POINT, BLB_V3(0, 3, 2), BLB_V3(0, 0, 0));

    if (!light || BLB_AddLight3D(demo.scene, light) != 0) {
      BLB_DestroyLight3D(light);
      demo.failed = true;
      break;
    }

    orbit->lights[i] = light;

    light->intensity = 3 * demo.settings.light_intensity;
    light->range = 12;
    light->ambient = 0;
    light->object->visible = false;

    BLB_Object3D *marker = Demo_Add3D(&demo, BLB_CreateSphere3D(BLB_V3(0.16f, 0.16f, 0.16f), BLB_V3(0, 3, 2), 12, NULL, BLB_TEXMAP_UV));

    Demo_Color(&demo, marker, BLB_V4(255, 213, 125, 255), 0, 0.5f);
    orbit->markers[i] = marker;

    if (marker) {
      BLB_Material_SetUnlit(marker->material, true);
      BLB_Material_SetEmission(marker->material, 1, 0.75f, 0.3f, 1, 2);
    }
  }

  Demo_Text(&demo, "3 moving point lights / matte, metallic and polished surfaces", 20, 680, 18);
  return Demo_Finish(&demo, animate);
}
