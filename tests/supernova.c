#include "test_common.h"
#include "tests.h"

#include <math.h>

static void configure_advanced(BLB_Material *m) {
  BLB_Material_SetBaseColor(m, 0.5f, 0.5f, 0.5f, 1.0f);
  BLB_Material_SetPBR(m, 0.0f, 0.7f);
  BLB_Material_SetNormal(m, 1.0f);
  BLB_Material_SetOcclusion(m, 1.0f);
  BLB_Material_SetSpecular(m, 0.55f, 0.55f, 0.55f, 0.25f);
  BLB_Material_SetIOR(m, 1.45f);
  BLB_Material_SetTransmission(m, 0.0f);
  BLB_Material_SetVolume(m, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
  BLB_Material_SetClearcoat(m, 0.0f, 0.0f, 1.0f);
  BLB_Material_SetSheen(m, 0.0f, 0.0f, 0.0f, 0.0f);
  BLB_Material_SetIridescence(m, 0.0f, 1.35f, 100.0f, 700.0f);
  BLB_Material_SetAnisotropy(m, 0.0f, 0.0f);
  BLB_Material_SetDispersion(m, 0.0f);
  BLB_Material_SetEmission(m, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
  BLB_Material_SetGlow(m, 0.0f, 0.0f, 0.0f);
  BLB_Material_SetAlphaMode(m, BLB_ALPHA_OPAQUE);
  BLB_Material_SetDepth(m, true, true);
  BLB_Material_SetLighting(m, true);
  BLB_Material_SetDoubleSided(m, false);
  BLB_Material_SetUnlit(m, false);
}

static BLB_Material *make_material(const char *name) {
  BLB_Material *m = BLB_Material_Build(BLB_MATERIAL_3D, configure_advanced);

  if (!m)
    return NULL;

  BLB_Material_SetName(m, name);
  return m;
}

static void replace_material3d(BLB_Object3D *object, BLB_Material *material) {
  if (!object || !material)
    return;

  if (object->material)
    BLB_Material_Release(object->material);

  object->material = material;
}

static BLB_Object3D *make_basalt_planet(void) {
  BLB_Object3D *planet = BLB_CreateSphere3D(HMM_V3(0.72f, 0.72f, 0.72f), HMM_V3(0.0f, 0.0f, 0.0f), 6, NULL);

  if (!planet)
    return NULL;

  BLB_Material *m = make_material("basalt");

  if (m) {
    BLB_Material_SetBaseColor(m, 0.015f, 0.022f, 0.030f, 1.0f);

    BLB_Material_SetPBR(m, 0.0f, 0.98f);

    BLB_Material_SetNormal(m, 2.6f);

    BLB_Material_SetOcclusion(m, 1.0f);

    BLB_Material_SetSpecular(m, 0.12f, 0.13f, 0.15f, 0.08f);

    BLB_Material_SetSheen(m, 0.01f, 0.012f, 0.015f, 0.06f);

    replace_material3d(planet, m);
  }

  planet->layer = 1;

  return planet;
}

static BLB_Object3D *make_moon_planet(void) {
  BLB_Object3D *planet = BLB_CreateSphere3D(HMM_V3(1.0f, 1.0f, 1.0f), HMM_V3(0.0f, 0.0f, 0.0f), 6, NULL);

  if (!planet)
    return NULL;

  BLB_Material *m = make_material("moon");

  if (m) {
    BLB_Material_SetBaseColor(m, 0.46f, 0.49f, 0.54f, 1.0f);

    BLB_Material_SetPBR(m, 0.0f, 0.82f);

    BLB_Material_SetNormal(m, 2.15f);

    BLB_Material_SetOcclusion(m, 0.94f);

    BLB_Material_SetSpecular(m, 0.38f, 0.40f, 0.44f, 0.22f);

    BLB_Material_SetSheen(m, 0.025f, 0.028f, 0.035f, 0.10f);

    replace_material3d(planet, m);
  }

  planet->layer = 1;

  return planet;
}

static BLB_Object3D *make_iron_planet(void) {
  BLB_Object3D *planet = BLB_CreateSphere3D(HMM_V3(0.86f, 0.86f, 0.86f), HMM_V3(0.0f, 0.0f, 0.0f), 6, NULL);

  if (!planet)
    return NULL;

  BLB_Material *m = make_material("iron_world");

  if (m) {
    BLB_Material_SetBaseColor(m, 0.48f, 0.075f, 0.018f, 1.0f);

    BLB_Material_SetPBR(m, 0.78f, 0.30f);

    BLB_Material_SetNormal(m, 2.1f);

    BLB_Material_SetOcclusion(m, 0.92f);

    BLB_Material_SetSpecular(m, 0.92f, 0.48f, 0.25f, 0.55f);

    BLB_Material_SetClearcoat(m, 0.35f, 0.08f, 1.0f);

    BLB_Material_SetSheen(m, 0.03f, 0.008f, 0.003f, 0.08f);

    replace_material3d(planet, m);
  }

  planet->layer = 1;

  return planet;
}

static BLB_Object3D *make_gas_planet(void) {
  BLB_Object3D *planet = BLB_CreateSphere3D(HMM_V3(2.15f, 2.15f, 2.15f), HMM_V3(0.0f, 0.0f, 0.0f), 6, NULL);

  if (!planet)
    return NULL;

  BLB_Material *m = make_material("gas_giant");

  if (m) {
    BLB_Material_SetBaseColor(m, 0.055f, 0.18f, 0.42f, 1.0f);

    BLB_Material_SetPBR(m, 0.0f, 0.48f);

    BLB_Material_SetNormal(m, 0.75f);

    BLB_Material_SetOcclusion(m, 0.84f);

    BLB_Material_SetSpecular(m, 0.68f, 0.72f, 0.78f, 0.42f);

    BLB_Material_SetSheen(m, 0.12f, 0.18f, 0.32f, 0.58f);

    BLB_Material_SetClearcoat(m, 0.12f, 0.10f, 0.86f);

    replace_material3d(planet, m);
  }

  planet->layer = 1;

  return planet;
}

static void update_orbit(BLB_Object3D *planet, float *angle, float delta_time, float radius, float speed, float tilt, float phase) {
  if (!planet || !angle)
    return;

  *angle += speed * delta_time;

  float a = *angle + phase;

  planet->position = HMM_V3(cosf(a) * radius, sinf(a * 0.73f) * radius * tilt, sinf(a) * radius);
}

static float smoothstep(float t) {
  t = fmaxf(0.0f, fminf(1.0f, t));
  return t * t * (3.0f - 2.0f * t);
}

static float ease_in_out(float t) {
  t = fmaxf(0.0f, fminf(1.0f, t));

  if (t < 0.5f)
    return 2.0f * t * t;

  float x = -2.0f * t + 2.0f;
  return 1.0f - x * x * 0.5f;
}

static HMM_Vec3 camera_destination(HMM_Vec3 target, float distance, float height) { return HMM_V3(target.x, target.y + height, target.z + distance); }

static HMM_Vec3 smooth_camera_position(HMM_Vec3 current, HMM_Vec3 target, float delta_time) {
  float dt = fminf(delta_time, 0.05f);

  float response = 5.5f;

  float alpha = 1.0f - expf(-response * dt);

  return HMM_AddV3(current, HMM_MulV3F(HMM_SubV3(target, current), alpha));
}

enum { SHOT_STAR = 0, SHOT_ROCK_1, SHOT_ROCK_2, SHOT_GAS, SHOT_ROCK_3, SHOT_WIDE, SHOT_COUNT };

static int cinematic_shot(float time, float *local_time) {
  const float durations[SHOT_COUNT] = {4.5f, 4.0f, 4.0f, 5.0f, 4.0f, 7.0f};

  float total = 0.0f;

  for (int i = 0; i < SHOT_COUNT; ++i)
    total += durations[i];

  float t = fmodf(time, total);

  for (int i = 0; i < SHOT_COUNT; ++i) {
    if (t < durations[i]) {
      *local_time = t;
      return i;
    }

    t -= durations[i];
  }

  *local_time = 0.0f;

  return SHOT_STAR;
}

static void set_black_space(BLB_Scene *scene) {
  if (!scene)
    return;

  scene->clear_enabled = true;
  scene->clear_color[0] = 0;
  scene->clear_color[1] = 0;
  scene->clear_color[2] = 0;
  scene->clear_color[3] = 255;
}

static void set_star_material(BLB_Object3D *star, bool core) {
  if (!star)
    return;

  BLB_Material *m = make_material(core ? "star_core" : "star_surface");

  if (!m)
    return;

  if (core) {
    BLB_Material_SetBaseColor(m, 1.0f, 0.90f, 0.65f, 1.0f);

    BLB_Material_SetPBR(m, 0.0f, 0.0f);

    BLB_Material_SetEmission(m, 18.0f, 10.0f, 3.0f, 255.0f, 10.0f);

    BLB_Material_SetGlow(m, 1.0f, 0.0f, 9.0f);
  } else {
    BLB_Material_SetBaseColor(m, 1.0f, 0.08f, 0.005f, 1.0f);

    BLB_Material_SetPBR(m, 0.0f, 0.12f);

    BLB_Material_SetEmission(m, 7.0f, 0.42f, 0.01f, 255.0f, 10.0f);

    BLB_Material_SetGlow(m, 1.0f, 0.08f, 6.0f);
  }

  BLB_Material_SetAlphaMode(m, BLB_ALPHA_OPAQUE);

  BLB_Material_SetRenderMode(m, BLB_RENDER_OPAQUE);

  BLB_Material_SetDepth(m, true, true);

  BLB_Material_SetLighting(m, true);

  BLB_Material_SetUnlit(m, false);

  replace_material3d(star, m);
}

int BLB_TestSuperNova(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Cinematic Space", HMM_V3(0.0f, 0.0f, 12.0f), 7, 9, 15) != 0) {
    return -1;
  }

  set_black_space(app.scene);

  BLB_Light3D *star_light = BLB_CreateLight3D(BLB_LIGHT_POINT, HMM_V3(0.0f, 0.0f, 0.0f), HMM_V3(0.0f, 0.0f, 0.0f));

  if (star_light) {
    star_light->intensity = 7.0f;
    star_light->ambient = 0.004f;
    star_light->specular = 0.65f;
    star_light->shininess = 48.0f;
    star_light->range = 110.0f;
    star_light->enabled = true;

    BLB_AddLight3D(app.scene, star_light);
  }

  BLB_Object3D *star = BLB_CreateSphere3D(HMM_V3(2.65f, 2.65f, 2.65f), HMM_V3(0.0f, 0.0f, 0.0f), 6, NULL);

  BLB_Object3D *core = BLB_CreateSphere3D(HMM_V3(1.0f, 1.0f, 1.0f), HMM_V3(0.0f, 0.0f, 0.0f), 6, NULL);

  BLB_Object3D *rock1 = make_basalt_planet();

  BLB_Object3D *rock2 = make_moon_planet();

  BLB_Object3D *rock3 = make_iron_planet();

  BLB_Object3D *gas = make_gas_planet();

  if (star) {
    set_star_material(star, false);

    star->layer = 1;

    BLB_AddObject3D(app.scene, star);
  }

  if (core) {
    set_star_material(core, true);

    core->layer = 2;

    BLB_AddObject3D(app.scene, core);
  }

  if (rock1)
    BLB_AddObject3D(app.scene, rock1);

  if (rock2)
    BLB_AddObject3D(app.scene, rock2);

  if (rock3)
    BLB_AddObject3D(app.scene, rock3);

  if (gas)
    BLB_AddObject3D(app.scene, gas);

  float orbit1 = 0.4f;
  float orbit2 = 1.9f;
  float orbit3 = 3.2f;
  float orbit4 = 5.0f;

  float cinematic_time = 0.0f;

  HMM_Vec3 camera_position = HMM_V3(0.0f, 0.0f, 12.0f);

  while (!BLB_WindowShouldClose(app.window)) {
    float delta_time = 0.0f;

    int frame = BLB_TestContext_BeginFrame(&app, &delta_time);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    float dt = fminf(delta_time, 0.05f);

    cinematic_time += dt;

    update_orbit(rock1, &orbit1, delta_time, 10.0f, 0.38f, 0.12f, 0.0f);

    update_orbit(rock2, &orbit2, delta_time, 16.0f, 0.22f, -0.10f, 1.7f);

    update_orbit(rock3, &orbit3, delta_time, 23.0f, 0.15f, 0.18f, 3.0f);

    update_orbit(gas, &orbit4, delta_time, 34.0f, 0.075f, -0.07f, 5.0f);

    if (star) {
      BLB_Object3D_Rotate(star, HMM_V3(8.0f * delta_time, 13.0f * delta_time, 5.0f * delta_time));
    }

    if (core) {
      BLB_Object3D_Rotate(core, HMM_V3(12.0f * delta_time, 18.0f * delta_time, 7.0f * delta_time));
    }

    if (rock1) {
      BLB_Object3D_Rotate(rock1, HMM_V3(18.0f * delta_time, 31.0f * delta_time, 7.0f * delta_time));
    }

    if (rock2) {
      BLB_Object3D_Rotate(rock2, HMM_V3(11.0f * delta_time, 23.0f * delta_time, 4.0f * delta_time));
    }

    if (rock3) {
      BLB_Object3D_Rotate(rock3, HMM_V3(21.0f * delta_time, 17.0f * delta_time, 9.0f * delta_time));
    }

    if (gas) {
      BLB_Object3D_Rotate(gas, HMM_V3(5.0f * delta_time, 8.0f * delta_time, 3.0f * delta_time));
    }

    float local_time = 0.0f;

    int shot = cinematic_shot(cinematic_time, &local_time);

    HMM_Vec3 desired_camera = camera_position;

    if (shot == SHOT_STAR) {
      float t = smoothstep(local_time / 4.5f);

      float distance = 11.5f - 4.8f * t;

      float height = 0.15f + 0.35f * sinf(local_time * 0.45f);

      desired_camera = camera_destination(HMM_V3(0.0f, 0.0f, 0.0f), distance, height);

    } else if (shot == SHOT_ROCK_1 && rock1) {
      float t = ease_in_out(local_time / 4.0f);

      float distance = 7.5f - 3.5f * t;

      float height = 0.20f + 0.18f * sinf(local_time * 0.7f);

      desired_camera = camera_destination(rock1->position, distance, height);

    } else if (shot == SHOT_ROCK_2 && rock2) {
      float t = ease_in_out(local_time / 4.0f);

      float distance = 8.5f - 4.0f * t;

      float height = 0.15f + 0.30f * sinf(local_time * 0.55f);

      desired_camera = camera_destination(rock2->position, distance, height);

    } else if (shot == SHOT_GAS && gas) {
      float t = ease_in_out(local_time / 5.0f);

      float distance = 16.0f - 8.0f * t;

      float height = 0.65f + 0.55f * sinf(local_time * 0.38f);

      desired_camera = camera_destination(gas->position, distance, height);

    } else if (shot == SHOT_ROCK_3 && rock3) {
      float t = ease_in_out(local_time / 4.0f);

      float distance = 7.8f - 4.0f * t;

      float height = -0.25f + 0.20f * sinf(local_time * 0.65f);

      desired_camera = camera_destination(rock3->position, distance, height);

    } else if (shot == SHOT_WIDE) {
      float t = ease_in_out(local_time / 7.0f);

      desired_camera = HMM_V3(0.0f, 0.0f, 30.0f + 38.0f * t);
    }

    camera_position = smooth_camera_position(camera_position, desired_camera, dt);

    if (app.scene->camera)
      app.scene->camera->position = camera_position;

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroySphere3D(rock1);
  BLB_DestroySphere3D(rock2);
  BLB_DestroySphere3D(rock3);
  BLB_DestroySphere3D(gas);
  BLB_DestroySphere3D(core);
  BLB_DestroySphere3D(star);

  if (star_light)
    BLB_DestroyLight3D(star_light);

  BLB_TestContext_Shutdown(&app);

  return 0;
}
