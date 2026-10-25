#include "solar_system.h"

typedef struct {
  Solar_State physics;
  BLB_Object3D *body[SOLAR_BODY_COUNT];
  BLB_Object3D *rings;
  BLB_Text2D *status;
  BLB_Text2D *labels[SOLAR_BODY_COUNT];
  double initial_energy;
} Solar_Demo;

static void label_planets(Demo_Context *demo) {
  Solar_Demo *system = demo->user_data;
  uint32_t width = 0, height = 0;

  BLB_RendererGetViewport(&demo->renderer, &width, &height);
  if (!width || !height)
    return;

  BLB_Mat4 projection = BLB_CameraProjection(demo->scene->camera, (float)width / height);
  BLB_Mat4 vp = BLB_MulM4(projection, BLB_CameraView(demo->scene->camera));
  BLB_Vec2 placed[SOLAR_BODY_COUNT];
  float label_width[SOLAR_BODY_COUNT];
  size_t placed_count = 0;

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    BLB_Object3D *body = system->body[i];
    BLB_Text2D *label = system->labels[i];
    BLB_Vec4 point = BLB_MulM4V4(vp, BLB_V4(body->position.x, body->position.y, body->position.z, 1));

    label->visible = point.w > .01f && fabsf(point.x) < point.w && fabsf(point.y) < point.w;
    if (!label->visible)
      continue;

    float radius = body->scale.x * .25f * fabsf(projection.Elements[1][1]) * height / point.w;
    float x = (point.x / point.w + 1) * width * .5f + radius + 8;
    float y = (point.y / point.w + 1) * height * .5f - 8;
    float text_width = (float)strlen(Solar_Planets[i].name) * 8.5f;

    x = fminf(fmaxf(x, 20), width - text_width - 20);
    y = fminf(fmaxf(y, 140), height - 130);
    for (unsigned attempt = 0; attempt < SOLAR_BODY_COUNT; ++attempt) {
      bool overlap = false;

      for (size_t j = 0; j < placed_count; ++j) {
        if (fabsf(y - placed[j].y) < 20 && x < placed[j].x + label_width[j] + 8 && x + text_width + 8 > placed[j].x) {
          y = placed[j].y + 22;
          overlap = true;
          break;
        }
      }
      if (!overlap)
        break;
    }

    label->position = BLB_V2(x, y);
    placed[placed_count] = label->position;
    label_width[placed_count++] = text_width;
  }
}

static void place_bodies(Demo_Context *demo) {
  Solar_Demo *system = demo->user_data;
  Solar_Vector sun = system->physics.position[0];

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    Solar_Vector r = system->physics.position[i];
    const Solar_Elements *p = &Solar_Planets[i];

    r.x -= sun.x;
    r.y -= sun.y;
    r.z -= sun.z;
    BLB_Object3D_SetPosition(system->body[i], Demo_OrbitDisplay(r, demo->settings.solar_distance_scale));
    BLB_Object3D_SetRotation(system->body[i], BLB_V3(0, (float)remainder(system->physics.days * 360 / p->spin_days, 360), (float)p->obliquity));
  }

  if (system->rings) {
    BLB_Object3D_SetPosition(system->rings, system->body[6]->position);
    BLB_Object3D_SetRotation(system->rings, BLB_V3(0, 0, 26.73f));
  }
}

static void animate(Demo_Context *demo) {
  Solar_Demo *system = demo->user_data;

  Solar_Advance(&system->physics, demo->dt * demo->settings.solar_days_per_second, demo->settings.solar_step_days);
  place_bodies(demo);

  char text[180];
  double error = fabs((Solar_Energy(&system->physics) - system->initial_energy) / system->initial_energy);

  snprintf(text, sizeof(text), "J2000 + %.1f days | %.1f days/s | Verlet dt %.3f day | relative energy drift %.2g", system->physics.days,
           demo->settings.solar_days_per_second, demo->settings.solar_step_days, error);
  BLB_SetText2D(system->status, text);
}

static void add_orbits(Demo_Context *demo) {
  /* Thin ribbons follow the initial inclined ellipses. They are reference paths,
   * not constraints: the integrator is free to precess under planet-planet gravity. */
  BLB_Object3D *orbits = Demo_Add3D(
      demo, BLB_LoadModelOBJ("assets/tests/models/saturn/orbits.obj",
                             BLB_V3(demo->settings.solar_distance_scale, demo->settings.solar_distance_scale, demo->settings.solar_distance_scale),
                             BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV));

  if (orbits) {
    Demo_Color(demo, orbits, BLB_V4(50, 71, 91, 255), 0, 1);
    BLB_Material_SetUnlit(orbits->material, true);
    BLB_Material_SetDoubleSided(orbits->material, true);
  }
}

int Demo_SolarSystem(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "SOLAR SYSTEM / Newtonian orbital mechanics", settings))
    return Demo_Finish(&demo, NULL);

  Solar_Demo *system = calloc(1, sizeof(*system));

  if (!system) {
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }

  demo.user_data = system;
  Solar_Initialize(&system->physics);
  system->initial_energy = Solar_Energy(&system->physics);
  Demo_Stars(&demo);

  float distance = demo.settings.solar_distance_scale;

  Demo_AimCamera(&demo, BLB_V3(0, 17 * distance, 23 * distance), BLB_V3(0, 0, 2 * distance));
  for (size_t i = 0; i < SOLAR_BODY_COUNT && !demo.failed; ++i) {
    float radius = i == 0 ? 1.0f : .23f + .65f * powf((float)(Solar_Planets[i].radius_km / 69911), .55f);

    radius *= demo.settings.solar_radius_scale;
    system->body[i] = Demo_Planet(&demo, Solar_Planets[i].texture, BLB_V3(0, 0, 0), radius);
    if (i == 6 && system->body[i])
      system->rings = Demo_SaturnRings(&demo, BLB_V3(0, 0, 0), radius);
  }

  if (demo.failed)
    return Demo_Finish(&demo, NULL);

  BLB_Material_SetUnlit(system->body[0]->material, true);
  /* A visible light-source sphere must not occlude the light it represents. */
  BLB_Material_SetRenderMode(system->body[0]->material, BLB_RENDER_TRANSPARENT);
  BLB_Material_SetAlphaMode(system->body[0]->material, BLB_ALPHA_BLEND);
  BLB_Light3D *sun = BLB_CreateLight3D(BLB_LIGHT_POINT, BLB_V3(0, 0, 0), BLB_V3(0, 0, 0));

  if (!sun || BLB_AddLight3D(demo.scene, sun) != 0) {
    BLB_DestroyLight3D(sun);
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }

  sun->object->visible = false;
  sun->intensity = 7 * demo.settings.light_intensity;
  sun->ambient = .055f;
  sun->range = 120 * distance;
  BLB_Object3D_SetColor(sun->object, BLB_V4(255, 248, 235, 255));

  if (demo.settings.solar_orbits)
    add_orbits(&demo);

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i)
    system->labels[i] = Demo_Text(&demo, Solar_Planets[i].name, 0, 0, 14);

  demo.after_camera = label_planets;
  system->status = Demo_Text(&demo, "Preparing nine-body integration...", 20, 620, 17);
  Demo_Text(&demo, "SUN  /  MERCURY  VENUS  EARTH  MARS  /  JUPITER  SATURN  URANUS  NEPTUNE", 20, 650, 17);
  Demo_Text(&demo, "AU/day physics; logarithmic display distances & enlarged radii. Not an ephemeris. Space: pause", 20, 680, 16);
  place_bodies(&demo);
  return Demo_Finish(&demo, animate);
}
