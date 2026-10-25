#include "demo_common.h"

Demo_Settings Demo_DefaultSettings(void) {
  return (Demo_Settings){.particle_count = 1200,
                         .detail = 32,
                         .animate = true,
                         .transforms = true,
                         .bloom = false,
                         .speed = 1.0f,
                         .animation_speed = 1.0f,
                         .model_size = 4.6f,
                         .layout_samples = 961,
                         .camera_speed = 0.22f,
                         .camera_distance = 10.0f,
                         .camera_height = 4.0f,
                         .light_intensity = 1.0f,
                         .opacity = 0.36f,
                         .emission_rate = 250.0f,
                         .sprite_frame_time = 0.14f,
                         .solar_days_per_second = 20,
                         .solar_step_days = .125f,
                         .solar_distance_scale = 1,
                         .solar_radius_scale = 1,
                         .solar_orbits = true,
                         .camera_mode = DEMO_CAMERA_SCENE};
}

unsigned Demo_Count(const Demo_Context *demo, unsigned fallback) {
  return demo->settings.object_count ? demo->settings.object_count : fallback;
}

const char *Demo_Font(void) {
  return Test_Font();
}

BLB_Text2D *Demo_Text(Demo_Context *demo, const char *value, float x, float y,
                      float size) {
  BLB_Text2D *text =
      BLB_CreateText2D(value, Demo_Font(), BLB_V2(x, y), size, true);

  if (!text || !text->font_loaded || BLB_AddText2D(demo->scene, text) != 0) {
    BLB_DestroyText2D(text);
    demo->failed = true;
    return NULL;
  }
  /* Glyph coverage must be blended, rather than written as opaque fragments. */
  BLB_Material_SetRenderMode(text->material, BLB_RENDER_TRANSPARENT);
  BLB_Material_SetAlphaMode(text->material, BLB_ALPHA_BLEND);
  text->layer = 60000;
  return text;
}

bool Demo_Open(Demo_Context *demo, const char *title,
               const Demo_Settings *settings) {
  memset(demo, 0, sizeof(*demo));
  demo->settings = settings ? *settings : Demo_DefaultSettings();
  demo->camera_target = BLB_V3(0, 1, 0);

  const Demo_Settings *config = &demo->settings;

  if (!isfinite(config->solar_days_per_second) ||
      config->solar_days_per_second < 0 ||
      config->solar_days_per_second > 1000 ||
      !isfinite(config->solar_step_days) || config->solar_step_days < .01f ||
      config->solar_step_days > .5f ||
      !isfinite(config->solar_distance_scale) ||
      config->solar_distance_scale < .5f || config->solar_distance_scale > 3 ||
      !isfinite(config->solar_radius_scale) ||
      config->solar_radius_scale < .25f || config->solar_radius_scale > 2 ||
      !isfinite(config->model_size) || config->model_size < 0.1f ||
      config->model_size > 100 || config->layout_samples < 32 ||
      config->layout_samples > 4096 || config->object_count > 20000 ||
      !config->particle_count || config->particle_count > 100000 ||
      config->detail < 8 || config->detail > 128 || !isfinite(config->speed) ||
      config->speed < 0 || config->speed > 10 ||
      !isfinite(config->animation_speed) || config->animation_speed <= 0 ||
      config->animation_speed > 10 || !isfinite(config->camera_distance) ||
      config->camera_distance <= 0 || config->camera_distance > 1000 ||
      !isfinite(config->camera_height) || fabsf(config->camera_height) > 1000 ||
      !isfinite(config->camera_speed) || config->camera_speed < 0 ||
      config->camera_speed > 10 || !isfinite(config->opacity) ||
      config->opacity < 0 || config->opacity > 1 ||
      !isfinite(config->light_intensity) || config->light_intensity < 0 ||
      config->light_intensity > 20 || !isfinite(config->emission_rate) ||
      config->emission_rate < 0 || config->emission_rate > 100000 ||
      !isfinite(config->sprite_frame_time) ||
      config->sprite_frame_time < 0.01f || config->sprite_frame_time > 10 ||
      config->camera_mode < DEMO_CAMERA_SCENE ||
      config->camera_mode > DEMO_CAMERA_FLY) {
    fputs("Invalid Demo_Settings. Start from Demo_DefaultSettings().\n",
          stderr);
    goto fail;
  }

  Test_Initialize();
  BLB_POSTPROCESS = demo->settings.bloom;
  BLB_POSTPROCESS_FXAA = false;
  BLB_POSTPROCESS_BLOOM = demo->settings.bloom;
  BLB_POSTPROCESS_BLOOM_STRENGTH = 0.12f;
  BLB_POSTPROCESS_BLOOM_THRESHOLD = 1.1f;
  BLB_POSTPROCESS_VIGNETTE = 0.12f;
  demo->window = BLB_CreateWindow(1280, 720, title);
  demo->scene = BLB_CreateScene(title);
  if (!demo->window || !demo->scene)
    goto fail;

  demo->scene->camera = BLB_CreateCamera(BLB_V3(0, 1.0f, 8));
  if (!demo->scene->camera ||
      BLB_Renderer_Create(&demo->renderer, BLB_RENDER_API_VULKAN,
                          demo->window->handle) != 0)
    goto fail;

  BLB_SetSceneClear(demo->scene, true, 2, 3, 7, 255);
  Demo_Text(demo, title, 20, 32, 24);
  Demo_Text(demo, "Esc: exit  |  F11: fullscreen  |  fly: WASD / QE, IJKL", 20,
            62, 17);
  demo->fps = Demo_Text(demo, "FPS: measuring...", 20, 91, 20);
  if (demo->failed)
    goto fail;

  return true;

fail:
  demo->failed = true;
  return false;
}

BLB_Object2D *Demo_Add2D(Demo_Context *demo, BLB_Object2D *object) {
  if (!object || BLB_AddObject2D(demo->scene, object) != 0) {
    Test_Destroy2D(object);
    demo->failed = true;
    return NULL;
  }
  return object;
}

BLB_Object3D *Demo_Add3D(Demo_Context *demo, BLB_Object3D *object) {
  demo->is_3d = true;

  if (!object || BLB_AddObject3D(demo->scene, object) != 0) {
    Test_Destroy3D(object);
    demo->failed = true;
    return NULL;
  }
  return object;
}

BLB_Object2D *Demo_Square(Demo_Context *demo, float x, float y, float width,
                          float height) {
  return Demo_Add2D(demo, BLB_CreateSquare2D(BLB_V2(width, height),
                                             BLB_V2(x, y), NULL, true));
}

BLB_Object3D *Demo_Cube(Demo_Context *demo, float x, float y, float z,
                        float size) {
  return Demo_Add3D(demo,
                    BLB_CreateCube3D(BLB_V3(size, size, size), BLB_V3(x, y, z),
                                     NULL, BLB_TEXMAP_UV));
}

void Demo_Light(Demo_Context *demo) {
  BLB_Light3D *light = BLB_CreateLight3D(BLB_LIGHT_DIRECTIONAL, BLB_V3(2, 5, 4),
                                         BLB_V3(-35, -25, 0));

  if (!light || BLB_AddLight3D(demo->scene, light) != 0) {
    BLB_DestroyLight3D(light);
    demo->failed = true;
    return;
  }
  light->ambient = 0.25f;
  light->intensity = 1.5f * demo->settings.light_intensity;
  light->object->visible = false;
}

void Demo_Spin(Demo_Context *demo) {
  for (int i = 0; i < demo->scene->object3d_count; ++i) {
    BLB_Object3D *object = demo->scene->objects3d[i];

    if (Demo_IsStage(demo, object))
      continue;

    BLB_Object3D_SetRotation(object, BLB_V3(15 * sinf((float)demo->elapsed),
                                            (float)demo->elapsed * 25, 0));
    Demo_GroundObject(object, 0, 0.04f);
  }
}

bool Demo_Bounds3D(const BLB_Object3D *object, bool world, BLB_Vec3 *minimum,
                   BLB_Vec3 *maximum) {
  if (!object || !minimum || !maximum)
    return false;

  *minimum = BLB_V3(FLT_MAX, FLT_MAX, FLT_MAX);
  *maximum = BLB_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

  float x = object->rotation.x * BLB_PI / 180,
        y = object->rotation.y * BLB_PI / 180,
        z = object->rotation.z * BLB_PI / 180;
  float sx = sinf(x), cx = cosf(x), sy = sinf(y), cy = cosf(y), sz = sinf(z),
        cz = cosf(z);
  size_t meshes = object->submesh_count ? object->submesh_count : 1;
  bool found = false;

  for (size_t mesh = 0; mesh < meshes; ++mesh) {
    const BLB_Polygon3D *polygon = object->submesh_count
                                       ? object->submeshes[mesh].polygon
                                       : object->polygon;

    if (!polygon || !polygon->vertices)
      continue;

    for (size_t i = 0; i < polygon->vertex_count; ++i) {
      BLB_Vec3 v = polygon->vertices[i];

      if (world) {
        v = BLB_V3(v.x * object->scale.x, v.y * object->scale.y,
                   v.z * object->scale.z);
        v = BLB_V3(cz * cy * v.x + (cz * sy * sx - sz * cx) * v.y +
                       (cz * sy * cx + sz * sx) * v.z,
                   sz * cy * v.x + (sz * sy * sx + cz * cx) * v.y +
                       (sz * sy * cx - cz * sx) * v.z,
                   -sy * v.x + cy * sx * v.y + cy * cx * v.z);
        v = BLB_AddV3(v, object->position);
      }

      if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z))
        return false;

      minimum->x = fminf(minimum->x, v.x);
      minimum->y = fminf(minimum->y, v.y);
      minimum->z = fminf(minimum->z, v.z);
      maximum->x = fmaxf(maximum->x, v.x);
      maximum->y = fmaxf(maximum->y, v.y);
      maximum->z = fmaxf(maximum->z, v.z);
      found = true;
    }
  }

  return found;
}

void Demo_GroundObject(BLB_Object3D *object, float surface, float clearance) {
  BLB_Vec3 minimum, maximum;

  if (!Demo_Bounds3D(object, true, &minimum, &maximum))
    return;

  BLB_Vec3 position = object->position;

  position.y += surface + clearance - minimum.y;
  BLB_Object3D_SetPosition(object, position);
}

static void fit_bounds(BLB_Object3D *object, float size, BLB_Vec3 minimum,
                       BLB_Vec3 maximum) {
  float extent = fmaxf(fmaxf(maximum.x - minimum.x, maximum.y - minimum.y),
                       maximum.z - minimum.z);
  float scale = extent > 0 ? size / extent : 1;
  BLB_Vec3 center = BLB_MulV3F(BLB_AddV3(minimum, maximum), 0.5f);

  BLB_Object3D_SetScale(object, BLB_V3(scale, scale, scale));
  BLB_Object3D_SetPosition(
      object,
      BLB_V3(-center.x * scale, 0.04f - minimum.y * scale, -center.z * scale));
}

void Demo_FitModel(BLB_Object3D *object, float size) {
  BLB_Vec3 minimum, maximum;

  if (Demo_Bounds3D(object, false, &minimum, &maximum))
    fit_bounds(object, size, minimum, maximum);
}

bool Demo_FitAnimatedModel(BLB_Object3D *object, float size, unsigned samples) {
  float duration = BLB_Model_GetAnimationDuration(object);

  if (!(duration > 0) || samples < 2 || samples > 4096)
    return false;

  float saved_time = BLB_Model_GetAnimationTime(object);
  BLB_Vec3 minimum = BLB_V3(FLT_MAX, FLT_MAX, FLT_MAX);
  BLB_Vec3 maximum = BLB_V3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  bool valid = true;

  /* A fixed clip envelope preserves root motion and jumps; per-frame grounding
   * would cancel them and make the character's body bob unnaturally. */
  for (unsigned sample = 0; sample < samples && valid; ++sample) {
    BLB_Vec3 pose_minimum, pose_maximum;

    valid = BLB_Model_SetAnimationTime(object, duration * sample /
                                                   (samples - 1)) == 0 &&
            Demo_Bounds3D(object, false, &pose_minimum, &pose_maximum);
    if (!valid)
      break;

    minimum.x = fminf(minimum.x, pose_minimum.x);
    minimum.y = fminf(minimum.y, pose_minimum.y);
    minimum.z = fminf(minimum.z, pose_minimum.z);
    maximum.x = fmaxf(maximum.x, pose_maximum.x);
    maximum.y = fmaxf(maximum.y, pose_maximum.y);
    maximum.z = fmaxf(maximum.z, pose_maximum.z);
  }

  if (BLB_Model_SetAnimationTime(object, saved_time) != 0)
    valid = false;
  if (valid)
    fit_bounds(object, size, minimum, maximum);

  return valid;
}

void Demo_AimCamera(Demo_Context *demo, BLB_Vec3 position, BLB_Vec3 target) {
  BLB_Vec3 direction = BLB_SubV3(target, position);
  float horizontal =
      sqrtf(direction.x * direction.x + direction.z * direction.z);

  demo->camera_target = target;
  BLB_Camera_SetPosition(demo->scene->camera, position);
  BLB_Camera_SetRotation(
      demo->scene->camera,
      BLB_V3(-atan2f(direction.y, horizontal) * 180 / BLB_PI,
             atan2f(direction.x, -direction.z) * 180 / BLB_PI, 0));
}

void Demo_Color(Demo_Context *demo, BLB_Object3D *object, BLB_Vec4 color,
                float metallic, float roughness) {
  BLB_Material *material = BLB_Material_Create3D();

  if (!object || !material) {
    BLB_Material_Release(material);
    demo->failed = true;
    return;
  }

  BLB_Material_SetBaseColor(material, color.x / 255, color.y / 255,
                            color.z / 255, color.w / 255);
  BLB_Material_SetPBR(material, metallic, roughness);
  BLB_Object3D_SetMaterial(object, material);
  BLB_Material_Release(material);
}

bool Demo_IsStage(const Demo_Context *demo, const BLB_Object3D *object) {
  return object == demo->stage[0] || object == demo->stage[1] ||
         object == demo->stage[2];
}

void Demo_Stage(Demo_Context *demo, float width) {
  BLB_Object3D *floor = Demo_Cube(demo, 0, -0.12f, 0, 1);

  demo->stage[0] = floor;

  if (floor) {
    BLB_Object3D_SetScale(floor, BLB_V3(width, 0.24f, width * 0.65f));
    Demo_Color(demo, floor, BLB_V4(15, 22, 32, 255), 0.15f, 0.72f);
  }

  for (int side = -1; side <= 1; side += 2) {
    BLB_Object3D *strip = Demo_Cube(demo, 0, 0.009f, side * width * 0.27f, 1);

    demo->stage[side < 0 ? 1 : 2] = strip;

    if (strip) {
      BLB_Object3D_SetScale(strip, BLB_V3(width * 0.86f, 0.012f, 0.018f));
      Demo_Color(demo, strip, BLB_V4(50, 190, 220, 255), 0.1f, 0.25f);
    }
  }

  Demo_AimCamera(demo, BLB_V3(width * 0.46f, width * 0.42f, width * 0.78f),
                 BLB_V3(0, 0.8f, 0));
}

static void update_camera(Demo_Context *demo) {
  if (!demo->is_3d || demo->settings.camera_mode == DEMO_CAMERA_SCENE)
    return;

  Demo_Settings *settings = &demo->settings;

  if (settings->camera_mode == DEMO_CAMERA_FIXED ||
      settings->camera_mode == DEMO_CAMERA_ORBIT) {
    float angle = settings->camera_mode == DEMO_CAMERA_ORBIT
                      ? (float)demo->elapsed * settings->camera_speed
                      : 0;
    BLB_Vec3 target = demo->camera_target;

    Demo_AimCamera(demo,
                   BLB_V3(target.x + sinf(angle) * settings->camera_distance,
                          target.y + settings->camera_height,
                          target.z + cosf(angle) * settings->camera_distance),
                   target);
    return;
  }

  BLB_Camera *camera = demo->scene->camera;
  float step = demo->dt * fmaxf(settings->camera_speed * 15, 0.2f);
  float yaw = camera->rotation.y * BLB_PI / 180;
  BLB_Vec3 position = camera->position;
  BLB_Vec3 rotation = camera->rotation;
  float forward = BLB_KeyboardDown(&demo->window->handlers, 'W') -
                  BLB_KeyboardDown(&demo->window->handlers, 'S');
  float right = BLB_KeyboardDown(&demo->window->handlers, 'D') -
                BLB_KeyboardDown(&demo->window->handlers, 'A');

  position.x += (sinf(yaw) * forward + cosf(yaw) * right) * step;
  position.z += (-cosf(yaw) * forward + sinf(yaw) * right) * step;
  position.y += (BLB_KeyboardDown(&demo->window->handlers, 'E') -
                 BLB_KeyboardDown(&demo->window->handlers, 'Q')) *
                step;
  rotation.y += (BLB_KeyboardDown(&demo->window->handlers, 'L') -
                 BLB_KeyboardDown(&demo->window->handlers, 'J')) *
                step * 20;
  rotation.x += (BLB_KeyboardDown(&demo->window->handlers, 'K') -
                 BLB_KeyboardDown(&demo->window->handlers, 'I')) *
                step * 20;
  rotation.x = fmaxf(-85, fminf(85, rotation.x));

  BLB_Camera_SetPosition(camera, position);
  BLB_Camera_SetRotation(camera, rotation);
}

static void close_demo(Demo_Context *demo) {
  /* Destroying the renderer waits for GPU work before releasing scene
   * resources. */
  BLB_Renderer_Destroy(&demo->renderer);
  BLB_Scene *scene = demo->scene;

  if (scene) {
    while (scene->object2d_count) {
      BLB_Object2D *object = scene->objects2d[scene->object2d_count - 1];

      BLB_RemoveObject2D(scene, object);
      Test_Destroy2D(object);
    }

    while (scene->object3d_count) {
      BLB_Object3D *object = scene->objects3d[scene->object3d_count - 1];

      BLB_RemoveObject3D(scene, object);
      Test_Destroy3D(object);
    }

    for (int i = 0; i < scene->text2d_count; ++i)
      BLB_DestroyText2D(scene->text2d[i]);

    for (int i = 0; i < scene->light3d_count; ++i)
      BLB_DestroyLight3D(scene->lights3d[i]);

    for (int i = 0; i < scene->light2d_count; ++i)
      BLB_DestroyLight2D(scene->lights2d[i]);

    for (int i = 0; i < scene->particle_system2d_count; ++i)
      BLB_DestroyParticleSystem2D(scene->particle_systems2d[i]);

    for (int i = 0; i < scene->particle_system3d_count; ++i)
      BLB_DestroyParticleSystem3D(scene->particle_systems3d[i]);

    for (int i = 0; i < scene->audio2d_count; ++i)
      BLB_DestroyAudio2D(demo->audio, scene->audio2d[i]);

    for (int i = 0; i < scene->audio3d_count; ++i)
      BLB_DestroyAudio3D(demo->audio, scene->audio3d[i]);
    BLB_DestroyCamera(scene->camera);
    BLB_DestroyScene(scene);
  }
  free(demo->user_data);
  BLB_ShutdownAudio(demo->audio);
  BLB_DestroyWindow(demo->window);
  BLB_ObjectLODClearCache();
}

int Demo_Finish(Demo_Context *demo, Demo_Update update) {
  if (!demo->failed) {
    const char *controls =
        demo->is_3d
            ? "Esc: exit | F11: fullscreen | camera: --camera "
              "scene/fixed/orbit/fly"
            : "Esc: exit | F11: fullscreen | --speed N | --animate on/off";
    if (demo->settings.camera_mode == DEMO_CAMERA_FLY && demo->is_3d)
      controls =
          "Esc: exit | F11: fullscreen | WASD: move | QE: height | IJKL: look";

    BLB_SetText2D(demo->scene->text2d[1], controls);
  }

  double previous = BLB_Platform_TimeSeconds(), fps_time = 0;
  unsigned long frames = 0, fps_frames = 0;

  while (!demo->failed && !BLB_WindowShouldClose(demo->window) &&
         (!demo->settings.frame_limit || frames < demo->settings.frame_limit)) {
    double now = BLB_Platform_TimeSeconds();
    double frame_time = now - previous;

    previous = now;
    BLB_WindowPollEvents(demo->window);
    if (BLB_KeyboardPressed(&demo->window->handlers, BLB_KEY_SPACE))
      demo->settings.animate = !demo->settings.animate;

    demo->dt =
        demo->settings.animate
            ? (float)fmin(fmin(fmax(frame_time, 0), 0.1) * demo->settings.speed,
                          0.1)
            : 0;
    demo->elapsed += demo->dt;
    BLB_DeltaTime_Set(demo->dt);
    BLB_SetSceneDeltaTime(demo->scene, demo->dt);
    BLB_WindowProcessHandlers(demo->window, demo->scene);
    if (BLB_KeyboardDown(&demo->window->handlers, BLB_KEY_ESCAPE) ||
        BLB_KeyboardPressed(&demo->window->handlers, BLB_KEY_ESCAPE))
      break;

    fps_time += frame_time;
    ++fps_frames;
    if (fps_time >= 0.25) {
      char value[96];

      snprintf(value, sizeof(value), "FPS: %.1f | frame: %.2f ms",
               fps_frames / fps_time, fps_time * 1000 / fps_frames);
      BLB_SetText2D(demo->fps, value);
      fps_time = 0;
      fps_frames = 0;
    }
    if (update && demo->settings.transforms)
      update(demo);

    update_camera(demo);
    if (demo->after_camera)
      demo->after_camera(demo);

    int prepared = BLB_RendererPrepareFrame(&demo->renderer, demo->window);

    if (prepared < 0) {
      demo->failed = true;
      break;
    }
    if (prepared > 0)
      continue;

    if (BLB_DrawScene(demo->scene, &demo->renderer) < 0)
      demo->failed = true;

    if (demo->animation_status && demo->animation_subject) {
      char status[256];
      BLB_Object3D *subject = demo->animation_subject;

      snprintf(
          status, sizeof(status), "%s | %zu bones | %.2f / %.2f s | Space: %s",
          demo->settings.animation_name ? demo->settings.animation_name : "Run",
          BLB_Model_GetBoneCount(subject), BLB_Model_GetAnimationTime(subject),
          BLB_Model_GetAnimationDuration(subject),
          demo->settings.animate ? "pause" : "resume");
      BLB_SetText2D(demo->animation_status, status);
    }

    ++frames;
  }

  if (demo->failed)
    fprintf(
        stderr,
        "Demo failed: initialization, resource loading, or rendering error.\n");
  close_demo(demo);
  return demo->failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
