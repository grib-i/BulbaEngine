#include "object_count_3d.h"

static void animate(Demo_Context *demo) {
  for (int i = 0; i < demo->scene->object3d_count; ++i) {
    BLB_Object3D *object = demo->scene->objects3d[i];
    float phase = (float)demo->elapsed + object->position.x * 0.13f + object->position.y * 0.23f + object->position.z * 0.17f;

    if (Demo_IsStage(demo, object))
      continue;

    BLB_Object3D_SetRotation(object, BLB_V3(sinf(phase) * 22, (float)demo->elapsed * 25, 0));
  }
}

int Demo_ObjectCount3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "STRESS FIELD / configurable object lattice", settings))
    return Demo_Finish(&demo, NULL);

  unsigned count = Demo_Count(&demo, 1000);
  unsigned edge = (unsigned)ceilf(cbrtf((float)count));
  float extent = edge * 0.8f;

  Demo_Stage(&demo, extent + 5);
  Demo_AimCamera(&demo, BLB_V3(extent * 1.1f, extent * 0.85f, extent * 1.6f), BLB_V3(0, extent * 0.4f, 0));

  for (unsigned i = 0; i < count && !demo.failed; ++i) {
    float x = ((float)(i % edge) - ((float)edge - 1) * 0.5f) * 0.8f;
    float y = 0.4f + (i / (edge * edge)) * 0.8f;
    float z = ((float)((i / edge) % edge) - ((float)edge - 1) * 0.5f) * 0.8f;
    BLB_Object3D *object = Demo_Cube(&demo, x, y, z, 0.45f);

    if (object)
      BLB_Object3D_SetColor(object, BLB_V4(65 + i % edge * 150.0f / edge, 175, 235 - (i / edge) % edge * 100.0f / edge, 255));
  }

  Demo_Light(&demo);
  char caption[128];

  snprintf(caption, sizeof(caption), "%u objects | transforms %s | --camera orbit/fly/fixed", count, demo.settings.transforms ? "ON" : "OFF");
  Demo_Text(&demo, caption, 20, 680, 18);
  return Demo_Finish(&demo, animate);
}
