#include "test.h"

extern const Test_Suite test_io;
extern const Test_Suite test_shaders;
extern const Test_Suite test_audio;
extern const Test_Suite test_core;
extern const Test_Suite test_resources;
extern const Test_Suite test_physics2d;
extern const Test_Suite test_physics3d;
extern const Test_Suite test_particles2d;
extern const Test_Suite test_particles3d;
extern const Test_Suite test_handlers;
extern const Test_Suite test_assets;
extern const Test_Suite test_spatial;
extern const Test_Suite test_renderer;
extern const Test_Suite test_orbits;

const Test_Suite *Test_GetSuites(size_t *count) {
  static const Test_Suite *const sources[] = {&test_io,        &test_shaders,   &test_audio,       &test_core,        &test_resources,
                                              &test_physics2d, &test_physics3d, &test_particles2d, &test_particles3d, &test_handlers,
                                              &test_assets,    &test_spatial,   &test_renderer,    &test_orbits};
  static Test_Suite suites[sizeof(sources) / sizeof(sources[0])];

  for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i)
    suites[i] = *sources[i];

  *count = sizeof(suites) / sizeof(suites[0]);
  return suites;
}
