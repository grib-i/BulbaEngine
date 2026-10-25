#include "bulba/core/scene.h"
#include "bulba/core/utils/delta_time.h"
#include <math.h>

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/optimization/object_aggregation.h"
#include "bulba/core/render/optimization/object_optimization2d.h"
#include "bulba/core/render/optimization/render_async.h"
#include "bulba/core/render/optimization/visibility_cache.h"

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

  scene->physics_world2d = BLB_Physics2D_WorldCreate();
  scene->physics_world3d = BLB_Physics3D_WorldCreate();
  scene->visibility_cache = BLB_RenderVisibilityCache_Create();

  if (!scene->name || !scene->physics_world2d || !scene->physics_world3d || !scene->visibility_cache) {

    BLB_Physics2D_WorldDestroy(scene->physics_world2d);
    BLB_Physics3D_WorldDestroy(scene->physics_world3d);
    BLB_RenderVisibilityCache_Destroy(scene->visibility_cache);

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

  scene->delta_time = BLB_DeltaTime_Get();
  scene->time = 0.0;
  scene->frame_index = 0;

  scene->next_entity_id = 1;
  scene->optimization_rebuild_pending = false;

  scene->camera = NULL;
  scene->skybox = NULL;

  scene->objects3d = NULL;
  scene->objects2d = NULL;

  scene->text3d = NULL;
  scene->text2d = NULL;

  scene->lights3d = NULL;
  scene->lights2d = NULL;

  scene->particles3d = NULL;
  scene->particles2d = NULL;

  scene->particle_systems3d = NULL;
  scene->particle_systems2d = NULL;

  scene->audio3d = NULL;
  scene->audio2d = NULL;

  scene->object3d_count = 0;
  scene->object2d_count = 0;

  scene->text3d_count = 0;
  scene->text2d_count = 0;

  scene->light3d_count = 0;
  scene->light2d_count = 0;

  scene->particle3d_count = 0;
  scene->particle2d_count = 0;

  scene->particle_system3d_count = 0;
  scene->particle_system2d_count = 0;

  scene->audio3d_count = 0;
  scene->audio2d_count = 0;

  scene->main_count = 0;

  scene->sort_signature3d = 0;
  scene->sort_signature2d = 0;
  scene->sort_signature_text2d = 0;
  scene->sort_signature_light3d = 0;
  scene->sort_signature_light2d = 0;
  scene->sort_signature_particle3d = 0;
  scene->sort_signature_particle2d = 0;
  scene->sort_signature_audio3d = 0;
  scene->sort_signature_audio2d = 0;

  scene->object_aggregation3d = NULL;
  scene->optimization_async_result = NULL;
  scene->optimization_async_generation = 0;
  scene->optimization_async_pending = false;
  scene->object_optimization2d = NULL;

  return scene;
}

void BLB_DestroyScene(BLB_Scene *scene) {
  if (!scene)
    return;

  BLB_RenderAsync_WaitAll();
  BLB_ObjectAggregation3D_Destroy(scene->optimization_async_result);
  scene->optimization_async_result = NULL;

  BLB_Physics2D_WorldDestroy(scene->physics_world2d);
  BLB_Physics3D_WorldDestroy(scene->physics_world3d);
  BLB_ObjectAggregation3D_Destroy(scene->object_aggregation3d);
  BLB_ObjectOptimization2D_Destroy(scene->object_optimization2d);
  BLB_RenderVisibilityCache_Destroy(scene->visibility_cache);
  BLB_Skybox_Destroy(scene->skybox);

  free(scene->objects3d);
  free(scene->objects2d);

  free(scene->text3d);
  free(scene->text2d);

  free(scene->lights3d);
  free(scene->lights2d);
  free(scene->particles3d);
  free(scene->particles2d);
  free(scene->particle_systems3d);
  free(scene->particle_systems2d);
  free(scene->audio3d);
  free(scene->audio2d);

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

  if (!isfinite(delta_time))
    return;

  if (delta_time < 0.0f)
    delta_time = 0.0f;

  if (delta_time > 0.1f)
    delta_time = 0.1f;

  scene->delta_time = delta_time;
  scene->delta_time_manual = true;
  scene->time += delta_time;
  scene->frame_index++;
}

void BLB_SetSceneSkybox(BLB_Scene *scene, BLB_Skybox *skybox) {
  if (!scene || scene->skybox == skybox)
    return;

  BLB_Skybox_Destroy(scene->skybox);
  scene->skybox = skybox;
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

  object->delta_time = scene->delta_time;

  ADD_ITEM(scene->objects3d, scene->object3d_count, BLB_Object3D, object);
}

int BLB_AddObject2D(BLB_Scene *scene, BLB_Object2D *object) {
  if (!scene || !object)
    return -1;

  if (object->entity_id == BLB_INVALID_ENTITY_ID)
    object->entity_id = scene->next_entity_id++;

  object->delta_time = scene->delta_time;

  ADD_ITEM(scene->objects2d, scene->object2d_count, BLB_Object2D, object);
}

int BLB_AddText2D(BLB_Scene *scene, BLB_Text2D *text) {
  if (!scene || !text)
    return -1;

  if (text->entity_id == BLB_INVALID_ENTITY_ID)
    text->entity_id = scene->next_entity_id++;

  text->delta_time = scene->delta_time;
  ADD_ITEM(scene->text2d, scene->text2d_count, BLB_Text2D, text);
}

int BLB_AddLight3D(BLB_Scene *scene, BLB_Light3D *light) {
  if (!scene || !light || !light->object)
    return -1;

  if (light->object->entity_id == BLB_INVALID_ENTITY_ID)
    light->object->entity_id = scene->next_entity_id++;

  light->object->delta_time = scene->delta_time;
  ADD_ITEM(scene->lights3d, scene->light3d_count, BLB_Light3D, light);
}

int BLB_AddLight2D(BLB_Scene *scene, BLB_Light2D *light) {
  if (!scene || !light || !light->object)
    return -1;

  if (light->object->entity_id == BLB_INVALID_ENTITY_ID)
    light->object->entity_id = scene->next_entity_id++;

  light->object->delta_time = scene->delta_time;
  ADD_ITEM(scene->lights2d, scene->light2d_count, BLB_Light2D, light);
}

int BLB_AddParticle3D(BLB_Scene *scene, BLB_Particle3D *particle) {
  if (!scene || !particle)
    return -1;

  particle->delta_time = scene->delta_time;
  ADD_ITEM(scene->particles3d, scene->particle3d_count, BLB_Particle3D, particle);
}

int BLB_AddParticle2D(BLB_Scene *scene, BLB_Particle2D *particle) {
  if (!scene || !particle)
    return -1;

  particle->delta_time = scene->delta_time;
  ADD_ITEM(scene->particles2d, scene->particle2d_count, BLB_Particle2D, particle);
}

int BLB_AddParticleSystem3D(BLB_Scene *scene, BLB_ParticleSystem3D *system) {
  if (!scene || !system)
    return -1;

  system->delta_time = scene->delta_time;
  ADD_ITEM(scene->particle_systems3d, scene->particle_system3d_count, BLB_ParticleSystem3D, system);
}

int BLB_AddParticleSystem2D(BLB_Scene *scene, BLB_ParticleSystem2D *system) {
  if (!scene || !system)
    return -1;

  system->delta_time = scene->delta_time;
  ADD_ITEM(scene->particle_systems2d, scene->particle_system2d_count, BLB_ParticleSystem2D, system);
}

int BLB_AddAudio3D(BLB_Scene *scene, BLB_Audio3D *audio) {
  if (!scene || !audio)
    return -1;

  ADD_ITEM(scene->audio3d, scene->audio3d_count, BLB_Audio3D, audio);
}

int BLB_AddAudio2D(BLB_Scene *scene, BLB_Audio2D *audio) {
  if (!scene || !audio)
    return -1;

  ADD_ITEM(scene->audio2d, scene->audio2d_count, BLB_Audio2D, audio);
}

/*
 * Functions for removing objects from the scene
 * Must be used when deleting the object.
 */

static int remove_item(void *array, int *count, void *item) {
  if (!array || !count || !item)
    return -1;

  void **items = *(void ***)array;

  if (!items)
    return -1;

  for (int i = 0; i < *count; ++i) {
    if (items[i] == item) {
      memmove(&items[i], &items[i + 1], (size_t)(*count - i - 1) * sizeof(void *));

      (*count)--;

      if (*count == 0) {
        free(items);
        *(void ***)array = NULL;
      } else {
        void **new_array = realloc(items, (size_t)*count * sizeof(void *));

        if (new_array)
          *(void ***)array = new_array;
      }

      return 0;
    }
  }

  return -1;
}

int BLB_RemoveObject3D(BLB_Scene *scene, BLB_Object3D *object) {
  if (!scene || !object)
    return -1;

  for (int i = 0; i < scene->light3d_count;) {
    BLB_Light3D *light = scene->lights3d[i];

    if (light && light->object == object) {
      remove_item((void **)&scene->lights3d, &scene->light3d_count, light);
      continue;
    }

    i++;
  }

  int result = remove_item((void **)&scene->objects3d, &scene->object3d_count, object);

  if (result == 0) {
    BLB_RenderAsync_WaitAll();
    BLB_ObjectAggregation3D_Destroy(scene->optimization_async_result);
    scene->optimization_async_result = NULL;
    BLB_ObjectAggregation3D_Destroy(scene->object_aggregation3d);
    scene->object_aggregation3d = NULL;
    scene->optimization_rebuild_pending = true;
  }

  return result;
}

int BLB_RemoveObject2D(BLB_Scene *scene, BLB_Object2D *object) {
  if (!scene || !object)
    return -1;

  for (int i = 0; i < scene->light2d_count;) {
    BLB_Light2D *light = scene->lights2d[i];

    if (light && light->object == object) {
      remove_item((void **)&scene->lights2d, &scene->light2d_count, light);
      continue;
    }

    i++;
  }

  int result = remove_item((void **)&scene->objects2d, &scene->object2d_count, object);

  if (result == 0) {
    BLB_ObjectOptimization2D_Destroy(scene->object_optimization2d);
    scene->object_optimization2d = NULL;
  }

  return result;
}

int BLB_RemoveText2D(BLB_Scene *scene, BLB_Text2D *text) {
  if (!scene || !text)
    return -1;

  return remove_item((void **)&scene->text2d, &scene->text2d_count, text);
}

int BLB_RemoveLight3D(BLB_Scene *scene, BLB_Light3D *light) {
  if (!scene || !light)
    return -1;

  return remove_item((void **)&scene->lights3d, &scene->light3d_count, light);
}

int BLB_RemoveLight2D(BLB_Scene *scene, BLB_Light2D *light) {
  if (!scene || !light)
    return -1;

  return remove_item((void **)&scene->lights2d, &scene->light2d_count, light);
}

int BLB_RemoveParticle3D(BLB_Scene *scene, BLB_Particle3D *particle) {
  if (!scene || !particle)
    return -1;

  return remove_item((void **)&scene->particles3d, &scene->particle3d_count, particle);
}

int BLB_RemoveParticle2D(BLB_Scene *scene, BLB_Particle2D *particle) {
  if (!scene || !particle)
    return -1;

  return remove_item((void **)&scene->particles2d, &scene->particle2d_count, particle);
}

int BLB_RemoveParticleSystem3D(BLB_Scene *scene, BLB_ParticleSystem3D *system) {
  if (!scene || !system)
    return -1;

  return remove_item((void **)&scene->particle_systems3d, &scene->particle_system3d_count, system);
}

int BLB_RemoveParticleSystem2D(BLB_Scene *scene, BLB_ParticleSystem2D *system) {
  if (!scene || !system)
    return -1;

  return remove_item((void **)&scene->particle_systems2d, &scene->particle_system2d_count, system);
}

int BLB_RemoveAudio3D(BLB_Scene *scene, BLB_Audio3D *audio) {
  if (!scene || !audio)
    return -1;

  return remove_item((void **)&scene->audio3d, &scene->audio3d_count, audio);
}

int BLB_RemoveAudio2D(BLB_Scene *scene, BLB_Audio2D *audio) {
  if (!scene || !audio)
    return -1;

  return remove_item((void **)&scene->audio2d, &scene->audio2d_count, audio);
}
