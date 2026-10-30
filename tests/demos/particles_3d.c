#include "particles_3d.h"

int Demo_Particles3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "3D particles: emission, gravity and color fade",
                 settings))
    return Demo_Finish(&demo, NULL);

  demo.is_3d = true;

  BLB_ParticleSystem3D *system = BLB_CreateParticleSystem3D(
      BLB_V3(1, 1, 1), BLB_V3(0, -1, 0), 0, demo.settings.particle_count, 300,
      1000.5f, NULL);

  if (!system || BLB_AddParticleSystem3D(demo.scene, system) != 0) {
    BLB_DestroyParticleSystem3D(system);
    demo.failed = true;
    return Demo_Finish(&demo, NULL);
  }
  system->velocity = BLB_V3(0, 2, 0);
  system->velocity_variation = BLB_V3(1.5f, 0.8f, 1.5f);
  system->gravity = BLB_V3(-1, -1, -1);
  system->particle_scale = BLB_V3(0.07f, 0.07f, 0.07f);
  system->emission_rate = demo.settings.emission_rate;
  system->color = BLB_ColorRGBA(255, 170, 40, 255);
  system->color_end = BLB_ColorRGBA(60, 100, 255, 0);
  system->looping = false;
  return Demo_Finish(&demo, NULL);
}
