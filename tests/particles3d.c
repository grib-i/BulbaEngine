#include "test_common.h"
#include "tests.h"

int BLB_TestParticles3D(void) {
  BLB_TestContext app;
  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Particles 3D", HMM_V3(0.0f, 0.0f, 13.0f), 7, 9, 15) != 0)
    return -1;

  BLB_ParticleSystem3D *system = BLB_CreateParticleSystem3D(HMM_V3(1.0f, 1.0f, 1.0f), HMM_V3(0.0f, -1.0f, 0.0f), 0, 4096, 2.4f, 0.9f, NULL);
  if (!system) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  system->gravity = HMM_V3(0.0f, -3.8f, 0.0f);
  system->velocity = HMM_V3(0.0f, 4.8f, 0.0f);
  system->velocity_variation = HMM_V3(3.5f, 2.5f, 2.5f);
  system->particle_scale = HMM_V3(0.34f, 0.34f, 0.0f);
  system->particle_scale_variation = HMM_V3(0.10f, 0.10f, 0.0f);
  system->color = HMM_V4(90.0f, 205.0f, 255.0f, 225.0f);
  system->color_end = HMM_V4(190.0f, 70.0f, 255.0f, 0.0f);
  system->emission_rate = 300.0f;
  system->rotation_speed = HMM_V3(0.0f, 0.0f, 110.0f);
  system->looping = true;

  if (!BLB_ParticleSystem3D_SetCount(system, 320)) {
    BLB_DestroyParticleSystem3D(system);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  if (BLB_AddParticleSystem3D(app.scene, system) != 0) {
    BLB_DestroyParticleSystem3D(system);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Particle3D *single =
      BLB_CreateParticle3D((BLB_ParticleID){.id = BLB_INVALID_PARTICLE_ID}, HMM_V3(-2.5f, 0.0f, 0.0f), HMM_V3(0.8f, 1.7f, 0.0f),
                           HMM_V3(0.52f, 0.52f, 0.0f), HMM_V3(0.0f, 0.0f, 0.0f), HMM_V4(255.0f, 210.0f, 90.0f, 235.0f), 3.5f, 3.5f, NULL, NULL);

  if (single) {
    single->rotation_speed = HMM_V3(0.0f, 0.0f, 70.0f);
    if (BLB_AddParticle3D(app.scene, single) != 0) {
      BLB_DestroyParticle3D(single);
      single = NULL;
    }
  }

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &dt);
    if (frame < 0)
      break;
    if (frame > 0)
      continue;
    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  if (single) {
    BLB_RemoveParticle3D(app.scene, single);
    BLB_DestroyParticle3D(single);
  }

  BLB_RemoveParticleSystem3D(app.scene, system);
  BLB_DestroyParticleSystem3D(system);
  BLB_TestContext_Shutdown(&app);
  return 0;
}
