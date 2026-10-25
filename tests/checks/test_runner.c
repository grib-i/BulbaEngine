#include "test.h"

static size_t checks;
static size_t failures;

void Test_Initialize(void) {
  static bool initialized;

  if (!initialized) {
    BLB_Init();
    initialized = true;
  }
}

bool Test_Check(bool success, const char *expression, const char *file, int line) {
  ++checks;

  if (!success) {
    ++failures;
    fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, expression);
  }

  return success;
}

void Test_Destroy2D(BLB_Object2D *object) {
  if (!object)
    return;

  if (object->type == BLB_OBJECT_CIRCLE2D)
    BLB_DestroyCircle2D(object);
  else
    BLB_DestroySquare2D(object);
}

void Test_Destroy3D(BLB_Object3D *object) {
  if (!object)
    return;

  switch (object->type) {
  case BLB_OBJECT_CUBE:
    BLB_DestroyCube3D(object);
    break;
  case BLB_OBJECT_SPHERE:
    BLB_DestroySphere3D(object);
    break;
  case BLB_OBJECT_TORUS:
    BLB_DestroyTorus3D(object);
    break;
  default:
    BLB_DestroyModelOBJ(object);
    break;
  }
}

Test_Settings Test_DefaultSettings(void) {
  return (Test_Settings){.repetitions = 1, .samples = 15, .iterations = 1000, .cycles = 100, .ticks = 10};
}

int Test_Run(const char *suite) {
  return Test_RunConfigured(suite, NULL);
}

int Test_RunConfigured(const char *suite, const Test_Settings *settings) {
  Test_Settings config = settings ? *settings : Test_DefaultSettings();

  if (!suite || !config.repetitions || config.repetitions > 10000)
    return EXIT_FAILURE;

  Test_Initialize();
  checks = failures = 0;
  size_t count, executed = 0;
  const Test_Suite *suites = Test_GetSuites(&count);

  for (unsigned repeat = 0; repeat < config.repetitions; ++repeat) {
    for (size_t i = 0; i < count; ++i) {
      if (strcmp(suite, "all") && strcmp(suite, suites[i].name))
        continue;

      for (size_t j = 0; j < suites[i].count; ++j) {
        const size_t before = failures;
        const double started = BLB_Platform_TimeSeconds();

        suites[i].cases[j].function();
        const double milliseconds = (BLB_Platform_TimeSeconds() - started) * 1000.0;

        printf("[%s] %-12s / %-36s %9.3f ms\n", before == failures ? "PASS" : "FAIL", suites[i].name, suites[i].cases[j].name, milliseconds);
        ++executed;
      }
    }
  }

  if (!executed) {
    fprintf(stderr, "Unknown or empty suite: %s\n", suite);
    return EXIT_FAILURE;
  }

  printf("\n%zu cases, %zu checks, %zu failures\n", executed, checks, failures);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

const char *Test_Font(void) {
  const char *custom = getenv("BLB_DEMO_FONT");

  if (custom && *custom)
    return custom;

  const char *paths[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                         "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
                         "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                         "/usr/share/fonts/google-noto-vf/NotoSans[wght].ttf",
                         "/usr/share/fonts/google-droid-sans-fonts/DroidSans.ttf",
                         "C:/Windows/Fonts/arial.ttf",
                         "/System/Library/Fonts/Supplemental/Arial.ttf"};

  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
    FILE *file = fopen(paths[i], "rb");
    if (file) {
      fclose(file);
      return paths[i];
    }
  }
  return NULL;
}
