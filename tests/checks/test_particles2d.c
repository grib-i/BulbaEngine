#include "test.h"

static void emission_motion_and_lifetime(void) {
  BLB_ParticleSystem2D *system = BLB_CreateParticleSystem2D(BLB_V2(1, 1), BLB_V2(0, 0), 0, 16, 1, 0, NULL);

  TEST_REQUIRE(system);
  system->color = BLB_ColorRGBA(255, 80, 20, 255);
  system->color_end = BLB_ColorRGBA(0, 120, 220, 0);
  system->velocity = BLB_V2(2, 0);
  system->gravity = BLB_V2(0, 0);
  system->velocity_variation = BLB_V2(0, 0);
  system->looping = false;
  system->emission_rate = 0;
  TEST_CHECK(BLB_ParticleSystem2D_Emit(system, 4) == 4);
  TEST_CHECK(system->count == 4);
  TEST_CHECK(system->particles[0].color.r == 255 && system->particles[0].color.g == 80 && system->particles[0].color.b == 20 &&
              system->particles[0].color.a == 255);
  BLB_ParticleSystem2D_Update(system, 0.25f);
  TEST_CHECK(system->particles[0].color.r == 191 && system->particles[0].color.g == 90 && system->particles[0].color.b == 70 &&
              system->particles[0].color.a == 191);
  TEST_NEAR(system->particles[0].position.x, 0.5, 1e-5);
  TEST_NEAR(system->particles[0].lifetime, 0.25, 1e-5);

  BLB_ParticleSystem2D_Stop(system);
  BLB_ParticleSystem2D_Update(system, 0.25f);
  TEST_NEAR(system->particles[0].lifetime, 0.25, 1e-5);
  BLB_ParticleSystem2D_Play(system);

  for (int i = 0; i < 10; ++i)
    BLB_ParticleSystem2D_Update(system, 0.1f);
  /* Expired particles stay allocated for reuse, but are no longer visible. */
  TEST_CHECK(system->count == 4);

  for (size_t i = 0; i < system->count; ++i)
    TEST_CHECK(!system->particles[i].visible);

  TEST_CHECK(BLB_ParticleSystem2D_SetCount(system, 8));
  TEST_CHECK(system->count == 8 && system->capacity >= system->count);
  BLB_ParticleSystem2D_SetCameraAttached(system, true);
  TEST_CHECK(BLB_ParticleSystem2D_IsCameraAttached(system));
  BLB_ParticleSystem2D_Restart(system);
  TEST_CHECK(system->playing);
  BLB_ParticleSystem2D_Clear(system);
  TEST_CHECK(system->count == 0);
  BLB_DestroyParticleSystem2D(system);
}

static const Test_Case cases[] = {TEST_CASE(emission_motion_and_lifetime)};
const Test_Suite test_particles2d = {"particles2d", cases, sizeof(cases) / sizeof(cases[0])};
