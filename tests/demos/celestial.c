#include "celestial.h"

BLB_Object3D *Demo_Planet(Demo_Context *demo, const char *name, BLB_Vec3 position, float radius) {
  char path[160];

  snprintf(path, sizeof(path), "assets/tests/textures/solar/%s.jpg", name);
  BLB_Texture *texture = BLB_Texture_Load2D(path);
  BLB_Object3D *body =
      Demo_Add3D(demo, BLB_CreateSphere3D(BLB_V3(radius * 2, radius * 2, radius * 2), position, demo->settings.detail, texture, BLB_TEXMAP_UV));

  if (!texture || !body)
    demo->failed = true;

  if (body) {
    Demo_Color(demo, body, BLB_V4(255, 255, 255, 255), 0, .92f);
    BLB_MaterialTexture_SetProjection(&body->material->base_color_texture, BLB_TEXMAP_UV);
  }

  BLB_Texture_Release(texture);
  return body;
}

BLB_Object3D *Demo_SaturnRings(Demo_Context *demo, BLB_Vec3 position, float radius) {
  BLB_Texture *texture = BLB_Texture_Load2D("assets/tests/textures/solar/saturn_ring_alpha.png");
  BLB_Object3D *rings =
      Demo_Add3D(demo, BLB_LoadModelOBJ("assets/tests/models/saturn/rings.obj", BLB_V3(radius, radius, radius), position, texture, BLB_TEXMAP_UV));

  if (!texture || !rings)
    demo->failed = true;

  if (rings) {
    Demo_Color(demo, rings, BLB_V4(255, 244, 225, 255), .02f, .88f);
    BLB_MaterialTexture_SetProjection(&rings->material->base_color_texture, BLB_TEXMAP_UV);
    BLB_Material_SetDoubleSided(rings->material, true);
    BLB_Material_SetAlphaMode(rings->material, BLB_ALPHA_BLEND);
    BLB_Material_SetRenderMode(rings->material, BLB_RENDER_TRANSPARENT);
    BLB_Material_SetDepth(rings->material, true, false);
  }

  BLB_Texture_Release(texture);
  return rings;
}

void Demo_Stars(Demo_Context *demo) {
  const char *path = demo->settings.skybox_path ? demo->settings.skybox_path : "assets/tests/textures/starmap_2020_2k.png";
  BLB_Skybox *sky = BLB_Skybox_Load2D(path);

  if (!sky)
    demo->failed = true;
  else
    BLB_SetSceneSkybox(demo->scene, sky);
}

BLB_Vec3 Demo_OrbitDisplay(Solar_Vector position, float scale) {
  double r = sqrt(position.x * position.x + position.y * position.y + position.z * position.z);
  double factor = r > 1e-12 ? scale * 3.5 * log1p(2 * r) / r : 0;

  return BLB_V3((float)(position.x * factor), (float)(position.y * factor), (float)(position.z * factor));
}
