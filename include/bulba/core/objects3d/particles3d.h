#ifndef PARTICLES3D_H
#define PARTICLES3D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/utils/particle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  BLB_ParticleID id;

  BLB_Vec3 position;
  BLB_Vec3 velocity;
  BLB_Vec3 scale;
  BLB_Vec3 rotation;
  BLB_Vec3 rotation_speed;
  BLB_Vec4 color;

  float lifetime;
  float lifetime_max;
  float delta_time;

  int render_mode;
  BLB_ComponentMask component_mask;
  bool visible;
  bool camera_attached;

  BLB_Texture *texture;
  BLB_Material *material;

  BLB_Polygon3D *polygon;
  Mesh *mesh;
} BLB_Particle3D;

typedef struct {
  BLB_Particle3D *particles;
  BLB_ParticleSystemID id;

  size_t count;
  size_t capacity;

  float emission_rate;
  float emission_accumulator;
  float lifetime;
  float lifetime_variation;
  float delta_time;

  int render_mode;
  BLB_ComponentMask component_mask;
  bool visible;
  bool camera_attached;

  BLB_Vec3 position;
  BLB_Vec3 velocity;
  BLB_Vec3 velocity_variation;
  BLB_Vec3 gravity;

  BLB_Vec3 particle_system_scale;
  BLB_Vec3 particle_scale;
  BLB_Vec3 particle_scale_variation;

  BLB_Vec4 color;
  BLB_Vec4 color_end;

  BLB_Vec3 rotation;
  BLB_Vec3 rotation_speed;

  bool looping;
  bool playing;

  BLB_Texture *texture;
  BLB_Material *material;
} BLB_ParticleSystem3D;

BLB_Particle3D *BLB_CreateParticle3D(BLB_ParticleID id, BLB_Vec3 position, BLB_Vec3 velocity, BLB_Vec3 scale, BLB_Vec3 rotation, BLB_Vec4 color,
                                     float lifetime, float lifetime_max, BLB_Texture *texture, BLB_Material *material);

void BLB_DestroyParticle3D(BLB_Particle3D *p);

void BLB_Particle3D_Move(BLB_Particle3D *p, BLB_Vec3 velocity);
void BLB_Particle3D_SetPosition(BLB_Particle3D *p, BLB_Vec3 position);
void BLB_Particle3D_Rotate(BLB_Particle3D *p, BLB_Vec3 angular_velocity);
void BLB_Particle3D_SetRotation(BLB_Particle3D *p, BLB_Vec3 rotation);
void BLB_Particle3D_Scale(BLB_Particle3D *p, BLB_Vec3 scale_velocity);
void BLB_Particle3D_SetScale(BLB_Particle3D *p, BLB_Vec3 scale);
void BLB_Particle3D_Transform(BLB_Particle3D *p, BLB_Vec3 position, BLB_Vec3 rotation, BLB_Vec3 scale);
void BLB_Particle3D_SetTexture(BLB_Particle3D *p, BLB_Texture *texture);
void BLB_Particle3D_SetMaterial(BLB_Particle3D *p, BLB_Material *material);
void BLB_Particle3D_FlipX(BLB_Particle3D *p);
void BLB_Particle3D_FlipY(BLB_Particle3D *p);
void BLB_Particle3D_FlipZ(BLB_Particle3D *p);

BLB_ParticleSystem3D *BLB_CreateParticleSystem3D(BLB_Vec3 particle_system_scale, BLB_Vec3 position, size_t count, size_t capacity, float lifetime,
                                                 float lifetime_variation, BLB_Texture *texture);

void BLB_DestroyParticleSystem3D(BLB_ParticleSystem3D *ps);

size_t BLB_ParticleSystem3D_Emit(BLB_ParticleSystem3D *ps, size_t count);
bool BLB_ParticleSystem3D_SetCount(BLB_ParticleSystem3D *ps, size_t count);
void BLB_ParticleSystem3D_Clear(BLB_ParticleSystem3D *ps);

void BLB_ParticleSystem3D_Update(BLB_ParticleSystem3D *ps, float delta_time);

void BLB_ParticleSystem3D_Play(BLB_ParticleSystem3D *ps);
void BLB_ParticleSystem3D_Stop(BLB_ParticleSystem3D *ps);
void BLB_ParticleSystem3D_Restart(BLB_ParticleSystem3D *ps);

void BLB_ParticleSystem3D_Move(BLB_ParticleSystem3D *ps, BLB_Vec3 velocity);
void BLB_ParticleSystem3D_SetPosition(BLB_ParticleSystem3D *ps, BLB_Vec3 position);
void BLB_ParticleSystem3D_Rotate(BLB_ParticleSystem3D *ps, BLB_Vec3 angular_velocity);
void BLB_ParticleSystem3D_SetRotation(BLB_ParticleSystem3D *ps, BLB_Vec3 rotation);
void BLB_ParticleSystem3D_Scale(BLB_ParticleSystem3D *ps, BLB_Vec3 scale_velocity);
void BLB_ParticleSystem3D_SetScale(BLB_ParticleSystem3D *ps, BLB_Vec3 scale);
void BLB_ParticleSystem3D_Transform(BLB_ParticleSystem3D *ps, BLB_Vec3 position, BLB_Vec3 rotation, BLB_Vec3 scale);
void BLB_ParticleSystem3D_SetTexture(BLB_ParticleSystem3D *ps, BLB_Texture *texture);
void BLB_ParticleSystem3D_SetMaterial(BLB_ParticleSystem3D *ps, BLB_Material *material);
void BLB_ParticleSystem3D_FlipX(BLB_ParticleSystem3D *ps);
void BLB_ParticleSystem3D_FlipY(BLB_ParticleSystem3D *ps);
void BLB_ParticleSystem3D_FlipZ(BLB_ParticleSystem3D *ps);

void BLB_Particle3D_SetCameraAttached(BLB_Particle3D *p, bool attached);
bool BLB_Particle3D_IsCameraAttached(const BLB_Particle3D *p);

void BLB_ParticleSystem3D_SetCameraAttached(BLB_ParticleSystem3D *ps, bool attached);
bool BLB_ParticleSystem3D_IsCameraAttached(const BLB_ParticleSystem3D *ps);

#endif
