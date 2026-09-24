#include "bulba/core/scene.h"

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects3d/objects3d.h"

#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *value) {
  if (!value)
    return NULL;

  size_t size = strlen(value) + 1;
  char *copy = malloc(size);

  if (copy)
    memcpy(copy, value, size);

  return copy;
}

BLB_Scene *BLB_CreateScene(const char *name) {
  BLB_Scene *scene = calloc(1, sizeof(*scene));

  if (!scene)
    return NULL;

  scene->name = duplicate_string(name ? name : "scene");

  scene->physics_world2d = BLB_Physics2DWorld_Create();
  scene->physics_world3d = BLB_Physics3DWorld_Create();

  if (!scene->name || !scene->physics_world2d || !scene->physics_world3d) {

    BLB_Physics2DWorld_Destroy(scene->physics_world2d);
    BLB_Physics3DWorld_Destroy(scene->physics_world3d);

    free(scene->name);
    free(scene);

    return NULL;
  }

  scene->layer = 0;

  scene->enabled = true;
  scene->visible = true;

  scene->clear_enabled = true;

  scene->clear_color[0] = 10;
  scene->clear_color[1] = 10;
  scene->clear_color[2] = 15;
  scene->clear_color[3] = 255;

  scene->delta_time = 0.0f;
  scene->time = 0.0;
  scene->frame_index = 0;

  scene->next_entity_id = 1;

  scene->camera = NULL;

  scene->objects3d = NULL;
  scene->objects2d = NULL;

  scene->text3d = NULL;
  scene->text2d = NULL;

  scene->lights3d = NULL;
  scene->lights2d = NULL;

  scene->object3d_count = 0;
  scene->object2d_count = 0;

  scene->text3d_count = 0;
  scene->text2d_count = 0;

  scene->light3d_count = 0;
  scene->light2d_count = 0;

  scene->main_count = 0;

  scene->sort_signature3d = 0;
  scene->sort_signature2d = 0;
  scene->sort_signature_text2d = 0;
  scene->sort_signature_light3d = 0;
  scene->sort_signature_light2d = 0;

  return scene;
}

void BLB_DestroyScene(BLB_Scene *scene) {
  if (!scene)
    return;

  BLB_Physics2DWorld_Destroy(scene->physics_world2d);
  BLB_Physics3DWorld_Destroy(scene->physics_world3d);

  free(scene->objects3d);
  free(scene->objects2d);

  free(scene->text3d);
  free(scene->text2d);

  free(scene->lights3d);
  free(scene->lights2d);

  free(scene->name);
  free(scene);
}

void BLB_SetSceneEnabled(BLB_Scene *scene, bool enabled) {
  if (!scene)
    return;

  scene->enabled = enabled;
}

void BLB_SetSceneVisible(BLB_Scene *scene, bool visible) {
  if (!scene)
    return;

  scene->visible = visible;
}

void BLB_SetSceneLayer(BLB_Scene *scene, unsigned short layer) {
  if (!scene)
    return;

  scene->layer = layer;
}

void BLB_SetSceneDeltaTime(BLB_Scene *scene, float delta_time) {
  if (!scene)
    return;

  if (delta_time < 0.0f)
    delta_time = 0.0f;

  if (delta_time > 0.1f)
    delta_time = 0.1f;

  scene->delta_time = delta_time;
  scene->time += delta_time;
  scene->frame_index++;
}

void BLB_SetSceneClear(BLB_Scene *scene, bool enabled, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  if (!scene)
    return;

  scene->clear_enabled = enabled;

  scene->clear_color[0] = r;
  scene->clear_color[1] = g;
  scene->clear_color[2] = b;
  scene->clear_color[3] = a;
}

#define ADD_ITEM(array, count, type, value)                                                                                                          \
  do {                                                                                                                                               \
    type **new_array = realloc((array), sizeof(type *) * ((size_t)(count) + 1));                                                                     \
                                                                                                                                                     \
    if (!new_array)                                                                                                                                  \
      return -1;                                                                                                                                     \
                                                                                                                                                     \
    (array) = new_array;                                                                                                                             \
    (array)[(count)++] = (value);                                                                                                                    \
                                                                                                                                                     \
    if ((count) > scene->main_count)                                                                                                                 \
      scene->main_count = (count);                                                                                                                   \
                                                                                                                                                     \
    return 0;                                                                                                                                        \
  } while (0)

int BLB_AddObject3D(BLB_Scene *scene, BLB_Object3D *object) {
  if (!scene || !object)
    return -1;

  if (object->entity_id == BLB_INVALID_ENTITY_ID)
    object->entity_id = scene->next_entity_id++;

  object->delta_time = &scene->delta_time;

  ADD_ITEM(scene->objects3d, scene->object3d_count, BLB_Object3D, object);
}

int BLB_AddObject2D(BLB_Scene *scene, BLB_Object2D *object) {
  if (!scene || !object)
    return -1;

  if (object->entity_id == BLB_INVALID_ENTITY_ID)
    object->entity_id = scene->next_entity_id++;

  object->delta_time = &scene->delta_time;

  ADD_ITEM(scene->objects2d, scene->object2d_count, BLB_Object2D, object);
}

int BLB_AddText2D(BLB_Scene *scene, BLB_Text2D *text) {
  if (!scene || !text)
    return -1;

  if (text->entity_id == BLB_INVALID_ENTITY_ID)
    text->entity_id = scene->next_entity_id++;

  text->delta_time = &scene->delta_time;

  ADD_ITEM(scene->text2d, scene->text2d_count, BLB_Text2D, text);
}

int BLB_AddLight3D(BLB_Scene *scene, BLB_Light3D *light) {
  if (!scene || !light || !light->object)
    return -1;

  if (light->object->entity_id == BLB_INVALID_ENTITY_ID)
    light->object->entity_id = scene->next_entity_id++;

  light->object->delta_time = &scene->delta_time;

  ADD_ITEM(scene->lights3d, scene->light3d_count, BLB_Light3D, light);
}

int BLB_AddLight2D(BLB_Scene *scene, BLB_Light2D *light) {
  if (!scene || !light || !light->object)
    return -1;

  if (light->object->entity_id == BLB_INVALID_ENTITY_ID)
    light->object->entity_id = scene->next_entity_id++;

  light->object->delta_time = &scene->delta_time;

  ADD_ITEM(scene->lights2d, scene->light2d_count, BLB_Light2D, light);
}
