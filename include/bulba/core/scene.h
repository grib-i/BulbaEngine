#ifndef BULBA_CORE_SCENE_H
#define BULBA_CORE_SCENE_H

#include "bulba/core/camera.h"
#include "bulba/core/entity.h"
#include "bulba/core/math3v/lights.h"
#include "bulba/core/objects2d/audio2d.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/particles2d.h"
#include "bulba/core/objects2d/text2d.h"
#include "bulba/core/objects3d/audio3d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/objects3d/particles3d.h"
#include "bulba/core/objects3d/skybox.h"
#include "bulba/core/physics/physics2d.h"
#include "bulba/core/physics/physics3d.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BLB_ObjectAggregation3D BLB_ObjectAggregation3D;
typedef struct BLB_ObjectOptimization2D BLB_ObjectOptimization2D;
typedef struct BLB_RenderVisibilityCache BLB_RenderVisibilityCache;

typedef struct BLB_Scene {
  char *name;

  unsigned short layer;

  bool enabled;
  bool visible;

  bool clear_enabled;
  unsigned char clear_color[4];

  float delta_time;
  bool delta_time_manual;
  uint64_t delta_time_revision;
  double time;
  uint64_t frame_index;

  BLB_EntityId next_entity_id;

  BLB_Physics2DWorld *physics_world2d;
  BLB_Physics3DWorld *physics_world3d;

  BLB_Camera *camera;
  BLB_Skybox *skybox;

  BLB_Object3D **objects3d;
  BLB_Object2D **objects2d;

  BLB_Text2D **text3d;
  BLB_Text2D **text2d;

  BLB_Light3D **lights3d;
  BLB_Light2D **lights2d;

  BLB_Particle3D *particle3d;
  BLB_Particle2D *particle2d;

  BLB_Particle3D **particles3d;
  BLB_Particle2D **particles2d;

  BLB_ParticleSystem3D **particle_systems3d;
  BLB_ParticleSystem2D **particle_systems2d;

  BLB_Audio3D **audio3d;
  BLB_Audio2D **audio2d;

  int object3d_count;
  int object2d_count;

  int text3d_count;
  int text2d_count;

  int light3d_count;
  int light2d_count;

  int particle3d_count;
  int particle2d_count;

  int particle_system3d_count;
  int particle_system2d_count;

  int audio3d_count;
  int audio2d_count;

  int main_count;

  uint64_t sort_signature3d;
  uint64_t sort_signature2d;
  uint64_t sort_signature_text2d;
  uint64_t sort_signature_light3d;
  uint64_t sort_signature_light2d;
  uint64_t sort_signature_particle3d;
  uint64_t sort_signature_particle2d;
  uint64_t sort_signature_audio3d;
  uint64_t sort_signature_audio2d;

  BLB_ObjectAggregation3D *object_aggregation3d;
  BLB_ObjectAggregation3D *optimization_async_result;
  uint64_t optimization_async_generation;
  bool optimization_async_pending;
  bool optimization_rebuild_pending;
  BLB_ObjectOptimization2D *object_optimization2d;
  BLB_RenderVisibilityCache *visibility_cache;
} BLB_Scene;

BLB_Scene *BLB_CreateScene(const char *name);
void BLB_DestroyScene(BLB_Scene *scene);

void BLB_SetSceneEnabled(BLB_Scene *scene, bool enabled);
void BLB_SetSceneVisible(BLB_Scene *scene, bool visible);
void BLB_SetSceneLayer(BLB_Scene *scene, unsigned short layer);
void BLB_SetSceneDeltaTime(BLB_Scene *scene, float delta_time);
void BLB_SetSceneSkybox(BLB_Scene *scene, BLB_Skybox *skybox);

void BLB_SetSceneClear(BLB_Scene *scene, bool enabled, unsigned char r, unsigned char g, unsigned char b, unsigned char a); // char in number

/*
 * Functions for added objects from the scene
 */
int BLB_AddObject3D(BLB_Scene *scene, BLB_Object3D *object);
int BLB_AddObject2D(BLB_Scene *scene, BLB_Object2D *object);

int BLB_AddText2D(BLB_Scene *scene, BLB_Text2D *text);
int BLB_AddParticle3D(BLB_Scene *scene, BLB_Particle3D *particle);
int BLB_AddParticle2D(BLB_Scene *scene, BLB_Particle2D *particle);

int BLB_AddLight3D(BLB_Scene *scene, BLB_Light3D *light);
int BLB_AddLight2D(BLB_Scene *scene, BLB_Light2D *light);

/* Particles and particle systems participate in scene update/render like other scene entries. */
int BLB_AddParticleSystem3D(BLB_Scene *scene, BLB_ParticleSystem3D *system);
int BLB_AddParticleSystem2D(BLB_Scene *scene, BLB_ParticleSystem2D *system);

int BLB_AddAudio3D(BLB_Scene *scene, BLB_Audio3D *audio);
int BLB_AddAudio2D(BLB_Scene *scene, BLB_Audio2D *audio);

/*
 * Functions for removing objects from the scene
 * Must be used when deleting the object.
 */
int BLB_RemoveObject3D(BLB_Scene *scene, BLB_Object3D *object);
int BLB_RemoveObject2D(BLB_Scene *scene, BLB_Object2D *object);
int BLB_RemoveText2D(BLB_Scene *scene, BLB_Text2D *text);
int BLB_RemoveParticle3D(BLB_Scene *scene, BLB_Particle3D *particle);
int BLB_RemoveParticle2D(BLB_Scene *scene, BLB_Particle2D *particle);

int BLB_RemoveLight3D(BLB_Scene *scene, BLB_Light3D *light);
int BLB_RemoveLight2D(BLB_Scene *scene, BLB_Light2D *light);
int BLB_RemoveParticleSystem3D(BLB_Scene *scene, BLB_ParticleSystem3D *system);
int BLB_RemoveParticleSystem2D(BLB_Scene *scene, BLB_ParticleSystem2D *system);

int BLB_RemoveAudio3D(BLB_Scene *scene, BLB_Audio3D *audio);
int BLB_RemoveAudio2D(BLB_Scene *scene, BLB_Audio2D *audio);

#endif
