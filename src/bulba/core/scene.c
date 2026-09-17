#include "bulba/core/scene.h"

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
  scene->physics_world = BLB_PhysicsWorld_Create();

  if (!scene->name || !scene->physics_world) {
    BLB_PhysicsWorld_Destroy(scene->physics_world);
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

  return scene;
}

void BLB_DestroyScene(BLB_Scene *scene) {
  if (!scene)
    return;

  for (int i = 0; i < scene->super_light3d_count; i++)
    BLB_DestroySuperLightObject3D(scene->super_lights3d[i]);

  free(scene->objects3d);
  free(scene->objects2d);
  free(scene->text3d);
  free(scene->text2d);
  free(scene->lights3d);
  free(scene->lights2d);
  free(scene->super_lights3d);
  BLB_PhysicsWorld_Destroy(scene->physics_world);
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

#define ADD_ITEM(array, count, type, value) \
  do { \
    type **new_array = realloc((array), sizeof(type *) * ((count) + 1)); \
    if (!new_array) \
      return -1; \
    (array) = new_array; \
    (array)[(count)++] = (value); \
    return 0; \
  } while (0)

int BLB_AddObject3D(BLB_Scene *scene, BLB_Object3D *object) {
  if (!scene || !object)
    return -1;
  if (object->entity_id == BLB_INVALID_ENTITY_ID)
    object->entity_id = scene->next_entity_id++;
  ADD_ITEM(scene->objects3d, scene->object3d_count, BLB_Object3D, object);
}

int BLB_AddObject2D(BLB_Scene *scene, BLB_Object2D *object) {
  if (!scene || !object)
    return -1;
  if (object->entity_id == BLB_INVALID_ENTITY_ID)
    object->entity_id = scene->next_entity_id++;
  ADD_ITEM(scene->objects2d, scene->object2d_count, BLB_Object2D, object);
}

int BLB_AddText2D(BLB_Scene *scene, BLB_Text2D *text) {
  if (!scene || !text)
    return -1;
  if (text->entity_id == BLB_INVALID_ENTITY_ID)
    text->entity_id = scene->next_entity_id++;
  ADD_ITEM(scene->text2d, scene->text2d_count, BLB_Text2D, text);
}

int BLB_AddLight3D(BLB_Scene *scene, BLB_Light3D *light) {
  if (!scene || !light)
    return -1;
  ADD_ITEM(scene->lights3d, scene->light3d_count, BLB_Light3D, light);
}

int BLB_AddSuperLightObject3D(BLB_Scene *scene, BLB_SuperObject3D *object) {
  if (!scene || !object)
    return -1;

  BLB_Object3D **objects = realloc(scene->objects3d, sizeof(*objects) * (size_t)(scene->object3d_count + 1));
  if (!objects)
    return -1;

  scene->objects3d = objects;
  if (object->object.entity_id == BLB_INVALID_ENTITY_ID)
    object->object.entity_id = scene->next_entity_id++;
  scene->objects3d[scene->object3d_count++] = &object->object;

  BLB_Light3D **lights = realloc(scene->lights3d, sizeof(*lights) * (size_t)(scene->light3d_count + 1));
  if (!lights) {
    scene->object3d_count--;
    return -1;
  }

  scene->lights3d = lights;
  scene->lights3d[scene->light3d_count++] = &object->light;

  BLB_SuperObject3D **supers = realloc(scene->super_lights3d, sizeof(*supers) * (size_t)(scene->super_light3d_count + 1));
  if (!supers) {
    scene->object3d_count--;
    scene->light3d_count--;
    return -1;
  }

  scene->super_lights3d = supers;
  scene->super_lights3d[scene->super_light3d_count++] = object;
  return 0;
}

int BLB_AddLight2D(BLB_Scene *scene, BLB_Light2D *light) {
  if (!scene || !light)
    return -1;
  ADD_ITEM(scene->lights2d, scene->light2d_count, BLB_Light2D, light);
}
