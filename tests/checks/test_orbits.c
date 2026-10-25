#include "../demos/orbital_physics.h"
#include "test.h"

static double length(Solar_Vector v) {
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static void ten_year_conservation(void) {
  Solar_State state;

  Solar_Initialize(&state);
  double energy = Solar_Energy(&state);
  Solar_Vector angular = Solar_AngularMomentum(&state);
  double largest_drift = 0;

  for (unsigned step = 0; step < 29220; ++step) {
    Solar_Step(&state, .125);
    if (step % 20 == 0)
      largest_drift = fmax(largest_drift, fabs((Solar_Energy(&state) - energy) / energy));
  }

  TEST_CHECK(largest_drift < 2e-6);
  Solar_Vector after = Solar_AngularMomentum(&state);

  TEST_NEAR(after.x, angular.x, 1e-15);
  TEST_NEAR(after.y, angular.y, 1e-15);
  TEST_NEAR(after.z, angular.z, 1e-15);
  Solar_Vector center = {0};

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    double mass = Solar_Planets[i].mass_ratio;

    center.x += mass * state.position[i].x;
    center.y += mass * state.position[i].y;
    center.z += mass * state.position[i].z;
    TEST_CHECK(isfinite(length(state.position[i])));
  }

  TEST_CHECK(length(center) < 1e-12);
}

static void ellipse_geometry_and_periods(void) {
  for (size_t i = 1; i < SOLAR_BODY_COUNT; ++i) {
    const Solar_Elements *p = &Solar_Planets[i];

    TEST_NEAR(length(Solar_OrbitPoint(i, 0)), p->a * (1 - p->e), 1e-10);
    TEST_NEAR(length(Solar_OrbitPoint(i, 3.141592653589793)), p->a * (1 + p->e), 1e-10);
  }

  Solar_State state;

  Solar_Initialize(&state);
  Solar_Vector earth = state.position[3];

  for (unsigned i = 0; i < 2922; ++i)
    Solar_Step(&state, .125); // 365.25 days, Earth should complete approximately one orbit.

  Solar_Vector difference = {state.position[3].x - earth.x, state.position[3].y - earth.y, state.position[3].z - earth.z};

  TEST_CHECK(length(difference) < .006);
}

static void fixed_step_is_frame_rate_independent(void) {
  Solar_State a, b;

  Solar_Initialize(&a);
  Solar_Initialize(&b);
  for (unsigned i = 0; i < 1000; ++i)
    Solar_Advance(&a, .1, .125);

  for (unsigned i = 0; i < 100; ++i)
    Solar_Advance(&b, 1, .125);

  TEST_NEAR(a.days, 100, 1e-8);
  TEST_NEAR(b.days, 100, 1e-8);
  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    TEST_NEAR(a.position[i].x, b.position[i].x, 1e-12);
    TEST_NEAR(a.position[i].y, b.position[i].y, 1e-12);
    TEST_NEAR(a.position[i].z, b.position[i].z, 1e-12);
  }

  Solar_Advance(&a, NAN, .125);
  Solar_Advance(&a, -1, .125);
  Solar_Advance(&a, 1, 0);
  TEST_NEAR(a.days, 100, 1e-8);
}

static const Test_Case cases[] = {TEST_CASE(ten_year_conservation), TEST_CASE(ellipse_geometry_and_periods),
                                  TEST_CASE(fixed_step_is_frame_rate_independent)};
const Test_Suite test_orbits = {"orbits", cases, sizeof(cases) / sizeof(cases[0])};
