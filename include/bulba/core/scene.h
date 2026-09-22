#ifndef BULBA_CORE_SCENE_H
#define BULBA_CORE_SCENE_H

#include "bulba/core/camera.h"
#include "bulba/core/entity.h"
#include "bulba/core/math3v/lights.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/text2d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BLB_Scene {
  char *name;

  unsigned short layer;

  bool enabled;
  bool visible;

  bool clear_enabled;
  unsigned char clear_color[4];

  float delta_time;
  double time;
  uint64_t frame_index;

  BLB_EntityId next_entity_id;

  BLB_PhysicsWorld *physics_world;
  BLB_Camera *camera;

  BLB_Object3D **objects3d;
  BLB_Object2D **objects2d;

  BLB_Text2D **text3d;
  BLB_Text2D **text2d;

  BLB_Light3D **lights3d;
  BLB_Light2D **lights2d;

  int object3d_count;
  int object2d_count;

  int text3d_count;
  int text2d_count;

  int light3d_count;
  int light2d_count;

  int main_count;

  uint64_t sort_signature3d;
  uint64_t sort_signature2d;
  uint64_t sort_signature_text2d;
  uint64_t sort_signature_light3d;
  uint64_t sort_signature_light2d;

} BLB_Scene;

BLB_Scene *BLB_CreateScene(const char *name);
void BLB_DestroyScene(BLB_Scene *scene);

void BLB_SetSceneEnabled(BLB_Scene *scene, bool enabled);
void BLB_SetSceneVisible(BLB_Scene *scene, bool visible);
void BLB_SetSceneLayer(BLB_Scene *scene, unsigned short layer);
void BLB_SetSceneDeltaTime(BLB_Scene *scene, float delta_time);

void BLB_SetSceneClear(BLB_Scene *scene, bool enabled, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

int BLB_AddObject3D(BLB_Scene *scene, BLB_Object3D *object);
int BLB_AddObject2D(BLB_Scene *scene, BLB_Object2D *object);

int BLB_AddText2D(BLB_Scene *scene, BLB_Text2D *text);

int BLB_AddLight3D(BLB_Scene *scene, BLB_Light3D *light);
int BLB_AddLight2D(BLB_Scene *scene, BLB_Light2D *light);

#endif
