#include "../tests.h"
#include <errno.h>
#include <limits.h>

typedef struct {
  const char *name;

  int (*run)(const Demo_Settings *settings);
  const char *description;
} Demo_Entry;

static const Demo_Entry demos[] = {
    {"primitives_2d", Demo_Primitives2d, "2D primitives: transforms and layers"},
    {"textures_2d", Demo_Textures2d, "2D textures from assets/tests"},
    {"sprite_animation", Demo_SpriteAnimation, "Sprite animation: walking characters from sprite sheets"},
    {"text_2d", Demo_Text2d, "Text2D: size, color and animated content"},
    {"particles_2d", Demo_Particles2d, "2D particles: emission, gravity and color fade"},
    {"particles_3d", Demo_Particles3d, "3D particles: emission, gravity and color fade"},
    {"primitives_3d", Demo_Primitives3d, "3D primitives: cube, sphere and torus"},
    {"textures_3d", Demo_Textures3d, "3D textures: brick, wood and Mars"},
    {"materials_3d", Demo_Materials3d, "PBR: metallic and roughness grid"},
    {"lights_3d", Demo_Lights3d, "Moving point light over textured objects"},
    {"physics_2d", Demo_Physics2d, "2D physics: falling boxes and static ground"},
    {"physics_3d", Demo_Physics3d, "3D physics: falling boxes and static ground"},
    {"model_obj", Demo_ModelObj, "OBJ model import"},
    {"model_glb", Demo_ModelGlb, "GLB model import"},
    {"model_fbx", Demo_ModelFbx, "FBX model import"},
    {"model_gltf", Demo_ModelGltf, "GLTF model import and animation"},
    {"skybox", Demo_Skybox, "Skybox and slowly rotating camera"},
    {"object_count_2d", Demo_ObjectCount2d, "2D object count: 2000 squares"},
    {"object_count_3d", Demo_ObjectCount3d, "3D object count: 1000 cubes"},
    {"transparency", Demo_Transparency, "Transparency: overlapping alpha-blended layers"},
    {"lod", Demo_Lod, "LOD: rotating spheres with explicit levels"},
    {"input", Demo_Input, "Keyboard and mouse: WASD moves the square"},
    {"lights_2d", Demo_Lights2d, "2D point light moving over textured tiles"},
    {"transparency_3d", Demo_Transparency3d, "3D glass panels with configurable alpha"},
    {"solar_system", Demo_SolarSystem, "Newtonian Sun + eight planets; AU/day integration, scaled display"},
    {"audio", Demo_Audio, "Audio: looping MP3 from assets/tests"},
};

static void list_tests(void) {
  puts("Usage: BulbaEngine [--list | unit [suite|all] | bench | memory | demo NAME [--frames N]]");
  puts("Demo options (space-separated pairs):");
  puts("  --count 1..20000  --particles 1..100000  --detail 8..128  --frames N");
  puts("  --camera scene|fixed|orbit|fly  --distance N  --height N  --camera-speed N");
  puts("  --animate on|off  --transforms on|off  --bloom on|off  --speed 0..10");
  puts("  --days-per-second 0..1000  --orbit-step 0.01..0.5  --orbit-scale 0.5..3  --radius-scale 0.25..2  --orbits on|off");
  puts("  --model-size 0.1..100  --layout-samples 32..4096");
  puts("  --animation-speed 0.01..10  --clip NAME  --model PATH  --skybox PATH");
  puts("  --light 0..20  --opacity 0..1  --emission N  --frame-time SECONDS");
  puts("  Camera options apply to 3D; count/asset options apply to their matching scenes.");
  puts("Checks: unit [suite] --repeat N | bench --samples N --iterations N | memory --cycles N --ticks N");
  puts("\nFunctional suites:");
  size_t count;
  const Test_Suite *suites = Test_GetSuites(&count);

  for (size_t i = 0; i < count; ++i)
    printf("  %-20s %zu cases\n", suites[i].name, suites[i].count);
  puts("\nDemos (each has a Text2D FPS overlay):");

  for (size_t i = 0; i < sizeof(demos) / sizeof(demos[0]); ++i)
    printf("  %-20s %s\n", demos[i].name, demos[i].description);
}

static bool integer(const char *text, unsigned long maximum, unsigned long *value) {
  if (!text || !*text || *text == '-' || *text == '+')
    return false;

  char *end;

  errno = 0;
  unsigned long parsed = strtoul(text, &end, 10);

  if (*end || errno || !parsed || parsed > maximum)
    return false;

  *value = parsed;
  return true;
}

static bool real_number(const char *text, float minimum, float maximum, float *value) {
  char *end;

  errno = 0;
  float parsed = strtof(text, &end);

  if (!*text || *end || errno || !isfinite(parsed) || parsed < minimum || parsed > maximum)
    return false;

  *value = parsed;
  return true;
}

static bool toggle(const char *text, bool *value) {
  if (strcmp(text, "on") && strcmp(text, "off"))
    return false;

  *value = !strcmp(text, "on");
  return true;
}

static bool demo_option(Demo_Settings *config, const char *name, const char *value) {
  unsigned long number;

  if (!strcmp(name, "--frames")) {
    if (!integer(value, ULONG_MAX, &number))
      return false;

    config->frame_limit = number;
  } else if (!strcmp(name, "--count")) {
    if (!integer(value, 20000, &number))
      return false;

    config->object_count = (unsigned)number;
  } else if (!strcmp(name, "--particles")) {
    if (!integer(value, 100000, &number))
      return false;

    config->particle_count = (unsigned)number;
  } else if (!strcmp(name, "--detail")) {
    if (!integer(value, 128, &number) || number < 8)
      return false;

    config->detail = (unsigned)number;
  } else if (!strcmp(name, "--layout-samples")) {
    if (!integer(value, 4096, &number) || number < 32)
      return false;

    config->layout_samples = (unsigned)number;
  } else if (!strcmp(name, "--model-size"))
    return real_number(value, 0.1f, 100, &config->model_size);
  else if (!strcmp(name, "--camera")) {
    if (!strcmp(value, "scene"))
      config->camera_mode = DEMO_CAMERA_SCENE;
    else if (!strcmp(value, "fixed"))
      config->camera_mode = DEMO_CAMERA_FIXED;
    else if (!strcmp(value, "orbit"))
      config->camera_mode = DEMO_CAMERA_ORBIT;
    else if (!strcmp(value, "fly"))
      config->camera_mode = DEMO_CAMERA_FLY;
    else
      return false;
  } else if (!strcmp(name, "--animate"))
    return toggle(value, &config->animate);
  else if (!strcmp(name, "--transforms"))
    return toggle(value, &config->transforms);
  else if (!strcmp(name, "--bloom"))
    return toggle(value, &config->bloom);
  else if (!strcmp(name, "--speed"))
    return real_number(value, 0, 10, &config->speed);
  else if (!strcmp(name, "--animation-speed"))
    return real_number(value, 0.01f, 10, &config->animation_speed);
  else if (!strcmp(name, "--camera-speed"))
    return real_number(value, 0, 10, &config->camera_speed);
  else if (!strcmp(name, "--distance"))
    return real_number(value, 0.1f, 1000, &config->camera_distance);
  else if (!strcmp(name, "--height"))
    return real_number(value, -1000, 1000, &config->camera_height);
  else if (!strcmp(name, "--light"))
    return real_number(value, 0, 20, &config->light_intensity);
  else if (!strcmp(name, "--opacity"))
    return real_number(value, 0, 1, &config->opacity);
  else if (!strcmp(name, "--emission"))
    return real_number(value, 0, 100000, &config->emission_rate);
  else if (!strcmp(name, "--frame-time"))
    return real_number(value, 0.01f, 10, &config->sprite_frame_time);
  else if (!strcmp(name, "--days-per-second"))
    return real_number(value, 0, 1000, &config->solar_days_per_second);
  else if (!strcmp(name, "--orbit-step"))
    return real_number(value, .01f, .5f, &config->solar_step_days);
  else if (!strcmp(name, "--orbit-scale"))
    return real_number(value, .5f, 3, &config->solar_distance_scale);
  else if (!strcmp(name, "--radius-scale"))
    return real_number(value, .25f, 2, &config->solar_radius_scale);
  else if (!strcmp(name, "--orbits"))
    return toggle(value, &config->solar_orbits);
  else if (!strcmp(name, "--model"))
    config->model_path = value;
  else if (!strcmp(name, "--clip"))
    config->animation_name = value;
  else if (!strcmp(name, "--skybox"))
    config->skybox_path = value;
  else
    return false;

  return true;
}

static bool check_option(Test_Settings *config, const char *name, const char *value, const char *mode) {
  unsigned long number;

  if (!integer(value, 1000000, &number))
    return false;

  if (!strcmp(mode, "unit") && !strcmp(name, "--repeat") && number <= 10000)
    config->repetitions = (unsigned)number;
  else if (!strcmp(mode, "bench") && !strcmp(name, "--samples") && number <= 1000)
    config->samples = (unsigned)number;
  else if (!strcmp(mode, "bench") && !strcmp(name, "--iterations"))
    config->iterations = (unsigned)number;
  else if (!strcmp(mode, "memory") && !strcmp(name, "--cycles"))
    config->cycles = (unsigned)number;
  else if (!strcmp(mode, "memory") && !strcmp(name, "--ticks") && number <= 100000)
    config->ticks = (unsigned)number;
  else
    return false;

  return true;
}

int Tests_Main(int argc, char **argv) {
  if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--list"))) {
    list_tests();
    return EXIT_SUCCESS;
  }

  if (argc >= 3 && !strcmp(argv[1], "demo")) {
    Demo_Settings config = Demo_DefaultSettings();

    if ((argc - 3) % 2)
      goto invalid;

    for (int i = 3; i < argc; i += 2) {
      if (!demo_option(&config, argv[i], argv[i + 1]))
        goto invalid;
    }

    for (size_t i = 0; i < sizeof(demos) / sizeof(demos[0]); ++i) {
      if (!strcmp(argv[2], demos[i].name))
        return demos[i].run(&config);
    }
    goto invalid;
  }

  if (!strcmp(argv[1], "unit") || !strcmp(argv[1], "bench") || !strcmp(argv[1], "memory")) {
    Test_Settings config = Test_DefaultSettings();
    const char *suite = "all";
    int first_option = 2;

    if (!strcmp(argv[1], "unit") && argc > 2 && strncmp(argv[2], "--", 2)) {
      suite = argv[2];
      first_option = 3;
    }

    if ((argc - first_option) % 2)
      goto invalid;

    for (int i = first_option; i < argc; i += 2) {
      if (!check_option(&config, argv[i], argv[i + 1], argv[1]))
        goto invalid;
    }

    if (!strcmp(argv[1], "unit"))
      return Test_RunConfigured(suite, &config);

    if (!strcmp(argv[1], "bench"))
      return Test_Benchmarks(&config);

    return Test_Lifecycle(&config);
  }

invalid:
  fputs("Unknown command, option, or out-of-range value. Use --list for help.\n", stderr);
  return EXIT_FAILURE;
}
