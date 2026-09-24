#include "bulba/core/scene.h"
#include "test_common.h"
#include "tests.h"

#include <math.h>

typedef struct {
  float a;
  float e;
  float inclination;
  float ascending_node;
  float periapsis;
  float mean_anomaly;
  float spin_speed;
  float axial_tilt;
} SN_Orbit;

#define SN_PI 3.14159265358979323846f

static float sn_deg(float v) { return v * SN_PI / 180.0f; }

static float sn_rad_to_deg(float v) { return v * 180.0f / SN_PI; }

static float sn_wrap_deg(float v) {
  while (v > 180.0f)
    v -= 360.0f;

  while (v < -180.0f)
    v += 360.0f;

  return v;
}

static float sn_smooth_factor(float dt, float speed) {
  if (dt <= 0.0f)
    return 1.0f;

  float k = 1.0f - expf(-speed * dt);

  if (k < 0.0f)
    k = 0.0f;

  if (k > 1.0f)
    k = 1.0f;

  return k;
}

static float sn_smooth_angle(float current, float target, float dt, float speed) {

  float delta = sn_wrap_deg(target - current);
  float k = sn_smooth_factor(dt, speed);

  return current + delta * k;
}

static HMM_Vec3 sn_look_rotation(HMM_Vec3 from, HMM_Vec3 target) {

  HMM_Vec3 d = HMM_SubV3(target, from);

  float len = HMM_LenV3(d);

  if (len <= 0.000001f)
    return HMM_V3(0.0f, 0.0f, 0.0f);

  d = HMM_MulV3F(d, 1.0f / len);

  float pitch = -asinf(fmaxf(-1.0f, fminf(1.0f, d.y)));

  float yaw = atan2f(d.x, -d.z);

  return HMM_V3(sn_rad_to_deg(pitch), sn_rad_to_deg(yaw), 0.0f);
}

static HMM_Vec3 sn_cam_orbit(HMM_Vec3 target, float distance, float angle, float height) {

  return HMM_AddV3(target, HMM_V3(cosf(angle) * distance, height, sinf(angle) * distance));
}

static float sn_orbit_mean_motion(const SN_Orbit *orbit) {

  const float mu = 120.0f;

  if (!orbit || orbit->a <= 0.0001f)
    return 0.0f;

  return sqrtf(mu / (orbit->a * orbit->a * orbit->a));
}

static HMM_Vec3 sn_orbit_position(const SN_Orbit *orbit, float time) {

  if (!orbit)
    return HMM_V3(0.0f, 0.0f, 0.0f);

  float n = sn_orbit_mean_motion(orbit);

  float M = orbit->mean_anomaly + n * time * 1.35f;

  while (M > SN_PI)
    M -= 2.0f * SN_PI;

  while (M < -SN_PI)
    M += 2.0f * SN_PI;

  float E = M;

  for (int i = 0; i < 10; i++) {
    float f = E - orbit->e * sinf(E) - M;

    float fp = 1.0f - orbit->e * cosf(E);

    if (fabsf(fp) < 0.000001f)
      break;

    E -= f / fp;
  }

  float cos_E = cosf(E);
  float sin_E = sinf(E);

  float sqrt_one_minus_e2 = sqrtf(fmaxf(0.0f, 1.0f - orbit->e * orbit->e));

  float x = orbit->a * (cos_E - orbit->e);

  float z = orbit->a * sqrt_one_minus_e2 * sin_E;

  float cos_w = cosf(orbit->periapsis);

  float sin_w = sinf(orbit->periapsis);

  float x1 = x * cos_w - z * sin_w;

  float z1 = x * sin_w + z * cos_w;

  float cos_i = cosf(orbit->inclination);

  float sin_i = sinf(orbit->inclination);

  float y2 = -z1 * sin_i;

  float z2 = z1 * cos_i;

  float cos_n = cosf(orbit->ascending_node);

  float sin_n = sinf(orbit->ascending_node);

  float world_x = x1 * cos_n - y2 * sin_n;

  float world_y = x1 * sin_n + y2 * cos_n;

  float world_z = z2;

  return HMM_V3(world_x, world_y, world_z);
}

static BLB_Material *sn_make_planet_material(float roughness, float specular) {

  BLB_Material *m = BLB_Material_Create(BLB_MATERIAL_3D);

  if (!m)
    return NULL;

  BLB_Material_SetBaseColor(m, 1.0f, 1.0f, 1.0f, 1.0f);

  BLB_Material_SetPBR(m, 0.0f, roughness);

  BLB_Material_SetNormal(m, 0.0f);

  BLB_Material_SetOcclusion(m, 1.0f);

  BLB_Material_SetSpecular(m, specular, specular, specular, 0.15f);

  BLB_Material_SetSheen(m, 0.0f, 0.0f, 0.0f, 0.0f);

  BLB_Material_SetClearcoat(m, 0.0f, 0.0f, 1.0f);

  BLB_Material_SetGlow(m, 0.0f, 0.0f, 0.0f);

  BLB_Material_SetLighting(m, true);

  BLB_Material_SetUnlit(m, false);

  return m;
}

static void sn_apply_material(BLB_Object3D *object, BLB_Material *material) {

  if (!object || !material)
    return;

  object->material = material;
}

static BLB_Object3D *sn_create_planet(HMM_Vec3 scale, const char *texture_path, float roughness, float specular) {

  BLB_Texture *texture = BLB_Texture_Load2D(texture_path);

  if (!texture)
    return NULL;

  BLB_Object3D *planet = BLB_CreateSphere3D(scale, HMM_V3(0.0f, 0.0f, 0.0f), 5, texture);

  if (!planet) {
    BLB_Texture_Release(texture);
    return NULL;
  }

  BLB_Texture_Release(texture);

  BLB_Material *material = sn_make_planet_material(roughness, specular);

  if (!material) {
    BLB_DestroySphere3D(planet);
    return NULL;
  }

  sn_apply_material(planet, material);

  return planet;
}

static void sn_setup_star_material(BLB_Object3D *star, float r, float g, float b, float emission_power, float emission_strength, float glow_r,
                                   float glow_g, float glow_b) {

  if (!star)
    return;

  BLB_Material *m = BLB_Material_Create(BLB_MATERIAL_3D);

  if (!m)
    return;

  BLB_Material_SetBaseColor(m, r, g, b, 1.0f);

  BLB_Material_SetLighting(m, false);

  BLB_Material_SetUnlit(m, true);

  BLB_Material_SetEmission(m, r, g, b, emission_power, emission_strength);

  BLB_Material_SetGlow(m, glow_r, glow_g, glow_b);

  BLB_Material_SetIridescence(m, 1.0f, 1.38f, 140.0f, 900.0f);

  BLB_Material_SetAnisotropy(m, 0.88f, 0.95f);

  BLB_Material_SetDispersion(m, 0.55f);

  sn_apply_material(star, m);

  BLB_Material_SetTemperature(m, 9000.0f);
}

static void sn_update_camera(BLB_Camera *camera, BLB_Object3D *mercury, BLB_Object3D *mars, BLB_Object3D *jupiter, BLB_Object3D *uranus, float time,
                             float dt) {
  if (!camera)
    return;

  const float STAR_TIME = 5.0f;
  const float MERCURY_TIME = 4.5f;
  const float MARS_TIME = 4.5f;
  const float JUPITER_TIME = 5.5f;
  const float URANUS_TIME = 5.0f;
  const float WIDE_TIME = 7.0f;

  const float TOTAL = STAR_TIME + MERCURY_TIME + MARS_TIME + JUPITER_TIME + URANUS_TIME + WIDE_TIME;

  float t = fmodf(time, TOTAL);

  HMM_Vec3 desired_position = HMM_V3(0.0f, 5.0f, 30.0f);

  HMM_Vec3 desired_target = HMM_V3(0.0f, 0.0f, 0.0f);

  float rotation_speed = 14.0f;

  if (t < STAR_TIME) {

    float local = t / STAR_TIME;

    float smooth = local * local * (3.0f - 2.0f * local);

    float angle = -1.0f + smooth * 0.9f;

    float distance = 12.0f - smooth * 4.0f;

    desired_target = HMM_V3(0.0f, 0.0f, 0.0f);

    desired_position = sn_cam_orbit(desired_target, distance, angle, 1.5f + smooth * 1.2f);

    rotation_speed = 10.0f;

  } else if (t < STAR_TIME + MERCURY_TIME && mercury) {

    float local = (t - STAR_TIME) / MERCURY_TIME;

    float smooth = local * local * (3.0f - 2.0f * local);

    desired_target = mercury->position;

    desired_position = sn_cam_orbit(desired_target, 5.7f - smooth * 1.4f, -1.1f + smooth * 1.1f, 1.2f + sinf(local * SN_PI) * 0.35f);

    rotation_speed = 18.0f;

  } else if (t < STAR_TIME + MERCURY_TIME + MARS_TIME && mars) {

    float local = (t - STAR_TIME - MERCURY_TIME) / MARS_TIME;

    float smooth = local * local * (3.0f - 2.0f * local);

    desired_target = mars->position;

    desired_position = sn_cam_orbit(desired_target, 7.8f - smooth * 1.8f, 1.7f - smooth * 1.2f, 1.5f + sinf(local * SN_PI) * 0.45f);

    rotation_speed = 18.0f;

  } else if (t < STAR_TIME + MERCURY_TIME + MARS_TIME + JUPITER_TIME && jupiter) {

    float local = (t - STAR_TIME - MERCURY_TIME - MARS_TIME) / JUPITER_TIME;

    float smooth = local * local * (3.0f - 2.0f * local);

    desired_target = jupiter->position;

    desired_position = sn_cam_orbit(desired_target, 14.0f - smooth * 4.0f, -0.9f + smooth * 1.0f, 3.2f + sinf(local * SN_PI) * 1.1f);

    rotation_speed = 16.0f;

  } else if (t < STAR_TIME + MERCURY_TIME + MARS_TIME + JUPITER_TIME + URANUS_TIME && uranus) {

    float local = (t - STAR_TIME - MERCURY_TIME - MARS_TIME - JUPITER_TIME) / URANUS_TIME;

    float smooth = local * local * (3.0f - 2.0f * local);

    desired_target = uranus->position;

    desired_position = sn_cam_orbit(desired_target, 12.0f - smooth * 2.5f, 0.9f - smooth * 0.9f, 3.5f + sinf(local * SN_PI) * 0.65f);

    rotation_speed = 17.0f;

  } else {

    float local = (t - STAR_TIME - MERCURY_TIME - MARS_TIME - JUPITER_TIME - URANUS_TIME) / WIDE_TIME;

    float smooth = local * local * (3.0f - 2.0f * local);

    float angle = -0.9f + smooth * 0.9f;

    float distance = 30.0f + smooth * 30.0f;

    desired_target = HMM_V3(0.0f, 0.0f, 0.0f);

    desired_position = sn_cam_orbit(desired_target, distance, angle, 7.0f + smooth * 5.0f);

    rotation_speed = 9.0f;
  }

  camera->position = desired_position;

  HMM_Vec3 desired_rotation = sn_look_rotation(camera->position, desired_target);

  camera->rotation.x = sn_smooth_angle(camera->rotation.x, desired_rotation.x, dt, rotation_speed);

  camera->rotation.y = sn_smooth_angle(camera->rotation.y, desired_rotation.y, dt, rotation_speed);

  camera->rotation.z = 0.0f;
}

int BLB_TestSuperNova(void) {

  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - SuperNova", HMM_V3(0.0f, 5.0f, 30.0f), 7, 9, 15) != 0) {
    return -1;
  }

  BLB_Light3D *light = BLB_CreateLight3D(BLB_LIGHT_POINT, HMM_V3(0.0f, 0.0f, 0.0f), HMM_V3(0.0f, -1.0f, 0.0f));

  if (light) {
    light->intensity = 6.0f;
    light->ambient = 0.003f;
    light->specular = 0.25f;
    light->shininess = 32.0f;
    light->range = 90.0f;

    BLB_AddLight3D(app.scene, light);
  }

  BLB_Object3D *star = BLB_CreateSphere3D(HMM_V3(3.5f, 3.5f, 3.5f), HMM_V3(0.0f, 0.0f, 0.0f), 5, NULL);

  BLB_Object3D *core = BLB_CreateSphere3D(HMM_V3(2.3f, 2.3f, 2.3f), HMM_V3(0.0f, 0.0f, 0.0f), 5, NULL);

  if (star) {
    sn_setup_star_material(star, 1.0f, 0.20f, 0.015f, 32500.0f, 43.4f, 0.7f, 0.015f, 4.0f);

    BLB_AddObject3D(app.scene, star);
  }

  if (core) {
    sn_setup_star_material(core, 1.0f, 0.90f, 0.60f, 6500.0f, 2.8f, 0.5f, 0.08f, 3.0f);

    BLB_AddObject3D(app.scene, core);
  }

  BLB_Object3D *mercury = sn_create_planet(HMM_V3(0.72f, 0.72f, 0.72f), "assets/tests/textures/planets/mercury.png", 0.90f, 0.18f);

  BLB_Object3D *mars = sn_create_planet(HMM_V3(1.0f, 1.0f, 1.0f), "assets/tests/textures/planets/mars.png", 0.84f, 0.20f);

  BLB_Object3D *jupiter = sn_create_planet(HMM_V3(2.15f, 2.15f, 2.15f), "assets/tests/textures/planets/jupiter.png", 0.64f, 0.28f);

  BLB_Object3D *uranus = sn_create_planet(HMM_V3(1.55f, 1.55f, 1.55f), "assets/tests/textures/planets/uranus.png", 0.54f, 0.28f);

  if (mercury)
    BLB_AddObject3D(app.scene, mercury);

  if (mars)
    BLB_AddObject3D(app.scene, mars);

  if (jupiter)
    BLB_AddObject3D(app.scene, jupiter);

  if (uranus)
    BLB_AddObject3D(app.scene, uranus);

  SN_Orbit mercury_orbit = {6.0f, 0.10f, sn_deg(7.0f), sn_deg(25.0f), sn_deg(45.0f), sn_deg(15.0f), 140.0f, 0.0f};
  SN_Orbit mars_orbit = {10.0f, 0.07f, sn_deg(4.0f), sn_deg(75.0f), sn_deg(15.0f), sn_deg(160.0f), 95.0f, 24.0f};
  SN_Orbit jupiter_orbit = {16.0f, 0.04f, sn_deg(9.0f), sn_deg(145.0f), sn_deg(120.0f), sn_deg(245.0f), 55.0f, 3.1f};
  SN_Orbit uranus_orbit = {24.0f, 0.03f, sn_deg(14.0f), sn_deg(230.0f), sn_deg(65.0f), sn_deg(300.0f), 35.0f, 97.8f};

  float time = 0.0f;

  app.scene->clear_color[0] = 0;
  app.scene->clear_color[1] = 0;
  app.scene->clear_color[2] = 0;
  app.scene->clear_color[3] = 255;

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;

    BLB_TestContext_BeginFrame(&app, &dt);

    if (dt > 0.1f)
      dt = 0.1f;

    time += dt;

    if (star) {
      star->position = HMM_V3(0.0f, 0.0f, 0.0f);

      star->rotation = HMM_V3(0.0f, time * 2.0f, 0.0f);
    }

    if (core) {
      core->position = HMM_V3(0.0f, 0.0f, 0.0f);

      core->rotation = HMM_V3(0.0f, -time * 12.0f, 0.0f);
    }

    if (mercury) {
      mercury->position = sn_orbit_position(&mercury_orbit, time);

      mercury->rotation = HMM_V3(mercury_orbit.axial_tilt, mercury_orbit.spin_speed * time, 0.0f);
    }

    if (mars) {
      mars->position = sn_orbit_position(&mars_orbit, time);

      mars->rotation = HMM_V3(mars_orbit.axial_tilt, mars_orbit.spin_speed * time, 0.0f);
    }

    if (jupiter) {
      jupiter->position = sn_orbit_position(&jupiter_orbit, time);

      jupiter->rotation = HMM_V3(jupiter_orbit.axial_tilt, jupiter_orbit.spin_speed * time, 0.0f);
    }

    if (uranus) {
      uranus->position = sn_orbit_position(&uranus_orbit, time);

      uranus->rotation = HMM_V3(uranus_orbit.axial_tilt, uranus_orbit.spin_speed * time, 0.0f);
    }

    if (light) {
      light->object->position = HMM_V3(0.0f, 0.0f, 0.0f);
    }

    sn_update_camera(app.scene->camera, mercury, mars, jupiter, uranus, time, dt);

    BLB_TestContext_Draw(&app);
  }

  if (star) {
    BLB_RemoveObject3D(app.scene, star);
    BLB_DestroySphere3D(star);
  }

  if (core) {
    BLB_RemoveObject3D(app.scene, core);
    BLB_DestroySphere3D(core);
  }

  if (mercury) {
    BLB_RemoveObject3D(app.scene, mercury);
    BLB_DestroySphere3D(mercury);
  }

  if (mars) {
    BLB_RemoveObject3D(app.scene, star);
    BLB_DestroySphere3D(mars);
  }

  if (jupiter) {
    BLB_RemoveObject3D(app.scene, mars);
    BLB_DestroySphere3D(jupiter);
  }

  if (uranus) {
    BLB_RemoveObject3D(app.scene, uranus);
    BLB_DestroySphere3D(uranus);
  }

  if (light) {
    BLB_RemoveLight3D(app.scene, light);
    BLB_DestroyLight3D(light);
  }

  BLB_TestContext_Shutdown(&app);

  return 0;
}
