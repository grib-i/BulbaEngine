#include "test.h"

static unsigned bench_iterations;

static volatile float benchmark_sink;

static int compare_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;

  return (x > y) - (x < y);
}

static void bench_math(void) {
  BLB_Vec3 value = BLB_V3(1, 2, 3);

  for (unsigned i = 0; i < bench_iterations; ++i) {
    value = BLB_NormV3(BLB_AddV3(value, BLB_V3(0.01f, 0.02f, 0.03f)));
    benchmark_sink = value.x;
  }
}

static void bench_objects2d(void) {
  for (unsigned i = 0; i < bench_iterations; ++i) {
    BLB_Object2D *object = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);

    if (!object)
      abort();
    BLB_Object2D_SetPosition(object, BLB_V2((float)i, 0));
    BLB_DestroySquare2D(object);
  }
}

static void bench_objects3d(void) {
  for (unsigned i = 0; i < bench_iterations; ++i) {
    BLB_Object3D *object = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

    if (!object)
      abort();
    BLB_Object3D_SetPosition(object, BLB_V3((float)i, 0, 0));
    BLB_DestroyCube3D(object);
  }
}

static void bench_materials(void) {
  for (unsigned i = 0; i < bench_iterations; ++i) {
    BLB_Material *material = BLB_Material_Create3D();

    if (!material)
      abort();
    BLB_Material_SetPBR(material, 0.5f, 0.5f);
    BLB_Material *copy = BLB_Material_Clone(material);

    if (!copy)
      abort();
    BLB_Material_Release(copy);
    BLB_Material_Release(material);
  }
}

static void bench_particles2d(void) {
  BLB_ParticleSystem2D *system = BLB_CreateParticleSystem2D(BLB_V2(1, 1), BLB_V2(0, 0), 1024, 1024, 3, 0, NULL);

  if (!system)
    abort();

  for (unsigned i = 0; i < bench_iterations; ++i)
    BLB_ParticleSystem2D_Update(system, 1.0f / 120.0f);

  benchmark_sink = (float)system->count;
  BLB_DestroyParticleSystem2D(system);
}

static void bench_particles3d(void) {
  BLB_ParticleSystem3D *system = BLB_CreateParticleSystem3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), 1024, 1024, 3, 0, NULL);

  if (!system)
    abort();

  for (unsigned i = 0; i < bench_iterations; ++i)
    BLB_ParticleSystem3D_Update(system, 1.0f / 120.0f);

  benchmark_sink = (float)system->count;
  BLB_DestroyParticleSystem3D(system);
}

static void bench_transforms2d(void) {
  BLB_Object2D *object = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);

  if (!object)
    abort();

  for (unsigned i = 0; i < bench_iterations; ++i)
    BLB_Object2D_Transform(object, BLB_V2((float)i, 0), (float)i, BLB_V2(2, 3));
  benchmark_sink = object->position.x;
  BLB_DestroySquare2D(object);
}

static void bench_transforms3d(void) {
  BLB_Object3D *object = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

  if (!object)
    abort();

  for (unsigned i = 0; i < bench_iterations; ++i)
    BLB_Object3D_Transform(object, BLB_V3((float)i, 0, 0), BLB_V3(0, (float)i, 0), BLB_V3(2, 3, 4));
  benchmark_sink = object->position.x;
  BLB_DestroyCube3D(object);
}

static void bench_text_updates(void) {
  BLB_Text2D *text = BLB_CreateText2D("FPS: 0", Test_Font(), BLB_V2(0, 0), 20, true);

  if (!text)
    abort();

  for (unsigned i = 0; i < bench_iterations; ++i)
    BLB_SetText2D(text, i % 2 ? "FPS: 60.0" : "FPS: 120.0");
  BLB_DestroyText2D(text);
}

static void bench_texture(void) {
  unsigned char pixels[4 * 16 * 16] = {0};

  for (unsigned i = 0; i < bench_iterations; ++i) {
    BLB_Texture *texture = BLB_Texture_Create2D(16, 16, pixels, sizeof(pixels));

    if (!texture)
      abort();
    BLB_Texture_Release(texture);
  }
}

static void bench_camera(void) {
  BLB_Camera *camera = BLB_CreateCamera(BLB_V3(0, 0, 5));

  if (!camera)
    abort();

  for (unsigned i = 0; i < bench_iterations; ++i) {
    BLB_Mat4 view = BLB_CameraView(camera);
    BLB_Mat4 projection = BLB_CameraProjection(camera, 16.0f / 9.0f);

    benchmark_sink = view.Elements[0][0] + projection.Elements[1][1];
  }
  BLB_DestroyCamera(camera);
}

static void bench_scene(void) {
  for (unsigned i = 0; i < bench_iterations; ++i) {
    BLB_Scene *scene = BLB_CreateScene("benchmark");

    if (!scene)
      abort();
    BLB_DestroyScene(scene);
  }
}

int Test_Benchmarks(const Test_Settings *settings) {
  Test_Settings config = settings ? *settings : Test_DefaultSettings();

  if (!config.iterations || config.iterations > 1000000 || !config.samples || config.samples > 1000)
    return EXIT_FAILURE;

  bench_iterations = config.iterations;
  Test_Initialize();
  const Test_Case benchmarks[] = {{"transforms2d.batch_with_setup", bench_transforms2d},
                                  {"transforms3d.batch_with_setup", bench_transforms3d},
                                  {"text2d.set_with_font_setup", bench_text_updates},
                                  {"texture16x16.create_release", bench_texture},
                                  {"camera.view_projection_with_setup", bench_camera},
                                  {"scene.create_destroy", bench_scene},
                                  {"math.normalize_add", bench_math},
                                  {"objects2d.create_transform_destroy", bench_objects2d},
                                  {"objects3d.create_transform_destroy", bench_objects3d},
                                  {"materials.create_clone_release", bench_materials},
                                  {"particles2d.update_1024", bench_particles2d},
                                  {"particles3d.update_1024", bench_particles3d}};
  printf("# Wall-clock timings. Run Release without sanitizers for comparable results.\n");
  printf("benchmark,iterations,samples,min_ns_per_iteration,median_ns_per_iteration,p95_ns_per_iteration\n");

  for (size_t i = 0; i < sizeof(benchmarks) / sizeof(benchmarks[0]); ++i) {
    double samples[1000];

    benchmarks[i].function();

    for (size_t sample = 0; sample < config.samples; ++sample) {
      double started = BLB_Platform_TimeSeconds();

      benchmarks[i].function();
      samples[sample] = (BLB_Platform_TimeSeconds() - started) * 1e9 / bench_iterations;
    }

    qsort(samples, config.samples, sizeof(samples[0]), compare_double);
    printf("%s,%u,%u,%.1f,%.1f,%.1f\n", benchmarks[i].name, bench_iterations, config.samples, samples[0], samples[config.samples / 2],
           samples[(size_t)ceil(config.samples * 0.95) - 1]);
  }

  return EXIT_SUCCESS;
}
