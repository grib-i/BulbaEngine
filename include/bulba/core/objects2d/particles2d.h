#ifndef BULBA_CORE_OBJECTS2D_PARTICLES2D_H
#define BULBA_CORE_OBJECTS2D_PARTICLES2D_H

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

  BLB_Vec2 position;
  BLB_Vec2 velocity;
  BLB_Vec2 scale;

  float rotation;
  float rotation_speed;

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

  BLB_Polygon2D *polygon;
} BLB_Particle2D;

typedef struct {
  BLB_Particle2D *particles;
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

  BLB_Vec2 position;
  BLB_Vec2 velocity;
  BLB_Vec2 velocity_variation;
  BLB_Vec2 gravity;

  BLB_Vec2 particle_system_scale;
  BLB_Vec2 particle_scale;
  BLB_Vec2 particle_scale_variation;

  BLB_Vec4 color;
  BLB_Vec4 color_end;

  float rotation;
  float rotation_speed;

  bool looping;
  bool playing;

  BLB_Texture *texture;
  BLB_Material *material;
} BLB_ParticleSystem2D;

BLB_Particle2D *BLB_CreateParticle2D(BLB_ParticleID id, BLB_Vec2 position, BLB_Vec2 velocity, BLB_Vec2 scale, float rotation, float rotation_speed,
                                     BLB_Vec4 color, float lifetime, float lifetime_max, BLB_Texture *texture, BLB_Material *material);

void BLB_DestroyParticle2D(BLB_Particle2D *p);

void BLB_Particle2D_Move(BLB_Particle2D *p, BLB_Vec2 velocity);
void BLB_Particle2D_SetPosition(BLB_Particle2D *p, BLB_Vec2 position);
void BLB_Particle2D_Rotate(BLB_Particle2D *p, float angular_velocity);
void BLB_Particle2D_SetRotation(BLB_Particle2D *p, float rotation);
void BLB_Particle2D_Scale(BLB_Particle2D *p, BLB_Vec2 scale_velocity);
void BLB_Particle2D_SetScale(BLB_Particle2D *p, BLB_Vec2 scale);
void BLB_Particle2D_Transform(BLB_Particle2D *p, BLB_Vec2 position, float rotation, BLB_Vec2 scale);
int BLB_Particle2D_SetTexture(BLB_Particle2D *p, BLB_Texture *texture);
void BLB_Particle2D_SetMaterial(BLB_Particle2D *p, BLB_Material *material);
void BLB_Particle2D_FlipX(BLB_Particle2D *p);
void BLB_Particle2D_FlipY(BLB_Particle2D *p);

BLB_ParticleSystem2D *BLB_CreateParticleSystem2D(BLB_Vec2 particle_system_scale, BLB_Vec2 position, size_t count, size_t capacity, float lifetime,
                                                 float lifetime_variation, BLB_Texture *texture);

void BLB_DestroyParticleSystem2D(BLB_ParticleSystem2D *ps);

size_t BLB_ParticleSystem2D_Emit(BLB_ParticleSystem2D *ps, size_t count);
bool BLB_ParticleSystem2D_SetCount(BLB_ParticleSystem2D *ps, size_t count);
void BLB_ParticleSystem2D_Clear(BLB_ParticleSystem2D *ps);

void BLB_ParticleSystem2D_Update(BLB_ParticleSystem2D *ps, float delta_time);

void BLB_ParticleSystem2D_Play(BLB_ParticleSystem2D *ps);
void BLB_ParticleSystem2D_Stop(BLB_ParticleSystem2D *ps);
void BLB_ParticleSystem2D_Restart(BLB_ParticleSystem2D *ps);

void BLB_ParticleSystem2D_Move(BLB_ParticleSystem2D *ps, BLB_Vec2 velocity);
void BLB_ParticleSystem2D_SetPosition(BLB_ParticleSystem2D *ps, BLB_Vec2 position);
void BLB_ParticleSystem2D_Rotate(BLB_ParticleSystem2D *ps, float angular_velocity);
void BLB_ParticleSystem2D_SetRotation(BLB_ParticleSystem2D *ps, float rotation);
void BLB_ParticleSystem2D_Scale(BLB_ParticleSystem2D *ps, BLB_Vec2 scale_velocity);
void BLB_ParticleSystem2D_SetScale(BLB_ParticleSystem2D *ps, BLB_Vec2 scale);
void BLB_ParticleSystem2D_Transform(BLB_ParticleSystem2D *ps, BLB_Vec2 position, float rotation, BLB_Vec2 scale);
int BLB_ParticleSystem2D_SetTexture(BLB_ParticleSystem2D *ps, BLB_Texture *texture);
void BLB_ParticleSystem2D_SetMaterial(BLB_ParticleSystem2D *ps, BLB_Material *material);
void BLB_ParticleSystem2D_FlipX(BLB_ParticleSystem2D *ps);
void BLB_ParticleSystem2D_FlipY(BLB_ParticleSystem2D *ps);

void BLB_Particle2D_SetCameraAttached(BLB_Particle2D *p, bool attached);
bool BLB_Particle2D_IsCameraAttached(const BLB_Particle2D *p);

void BLB_ParticleSystem2D_SetCameraAttached(BLB_ParticleSystem2D *ps, bool attached);
bool BLB_ParticleSystem2D_IsCameraAttached(const BLB_ParticleSystem2D *ps);

#endif
