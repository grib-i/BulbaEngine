#include "particles_2d.h"

int Demo_Particles2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "2D particles: emission, gravity and color fade", settings))
    return Demo_Finish(&demo, NULL);

  BLB_ParticleSystem2D *system = BLB_CreateParticleSystem2D(BLB_V2(1, 1), BLB_V2(640, 580), 0, demo.settings.particle_count, 3, 0.5f, NULL);

  if (!system || BLB_AddParticleSystem2D(demo.scene, system) != 0) {
    BLB_DestroyParticleSystem2D(system);
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }
  system->velocity = BLB_V2(0, -180);
  system->velocity_variation = BLB_V2(120, 90);
  system->gravity = BLB_V2(0, 100);
  system->particle_scale = BLB_V2(6, 6);
  BLB_ParticleSystem2D_SetCameraAttached(system, true);
  system->emission_rate = demo.settings.emission_rate;
  system->color = BLB_ColorRGBA(255, 170, 40, 255);
  system->color_end = BLB_ColorRGBA(60, 100, 255, 0);
  system->looping = false;
  return Demo_Finish(&demo, NULL);
}
