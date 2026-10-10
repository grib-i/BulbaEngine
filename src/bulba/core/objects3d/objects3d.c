#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/object_aggregation.h"
#include "bulba/core/render/texture.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void mark_transform_changed(BLB_Object3D *object) {
  if (!object)
    return;

  ++object->transform_revision;
  if (object->transform_revision == 0)
    object->transform_revision = 1;

  object->optimization_dynamic = true;
  object->optimization_transform_dirty = true;
  object->optimization_stable_frames = 0;
  object->optimization_static_recovered = false;

  if (object->optimization_cluster)
    object->optimization_cluster->dirty = true;
}

void BLB_Object3D_Move(BLB_Object3D *object, HMM_Vec3 velocity) {
  if (!object)
    return;

  if (object->delta_time)
    velocity = HMM_MulV3F(velocity, *object->delta_time);

  object->position = HMM_AddV3(object->position, velocity);
  mark_transform_changed(object);
}

void BLB_Object3D_SetPosition(BLB_Object3D *object, HMM_Vec3 position) {
  if (!object)
    return;

  object->position = position;
  mark_transform_changed(object);
}

void BLB_Object3D_Rotate(BLB_Object3D *object, HMM_Vec3 angular_velocity) {
  if (!object)
    return;

  if (object->delta_time)
    angular_velocity = HMM_MulV3F(angular_velocity, *object->delta_time);

  object->rotation = HMM_AddV3(object->rotation, angular_velocity);
  mark_transform_changed(object);
}

void BLB_Object3D_SetRotation(BLB_Object3D *object, HMM_Vec3 rotation) {
  if (!object)
    return;

  object->rotation = rotation;
  mark_transform_changed(object);
}

void BLB_Object3D_Scale(BLB_Object3D *object, HMM_Vec3 scale_velocity) {
  if (!object)
    return;

  if (object->delta_time)
    scale_velocity = HMM_MulV3F(scale_velocity, *object->delta_time);

  object->scale = HMM_AddV3(object->scale, scale_velocity);
  mark_transform_changed(object);
}

void BLB_Object3D_SetScale(BLB_Object3D *object, HMM_Vec3 scale) {
  if (!object)
    return;

  object->scale = scale;
  mark_transform_changed(object);
}

void BLB_Object3D_Transform(BLB_Object3D *object, HMM_Vec3 position, HMM_Vec3 rotation, HMM_Vec3 scale) {
  if (!object)
    return;

  object->position = position;
  object->rotation = rotation;
  object->scale = scale;
  mark_transform_changed(object);
}

void BLB_Object3D_SetCameraAttached(BLB_Object3D *object, bool attached) {
  if (!object || object->camera_attached == attached)
    return;
  object->camera_attached = attached;
  mark_transform_changed(object);
}

bool BLB_Object3D_IsCameraAttached(const BLB_Object3D *object) { return object && object->camera_attached; }

void BLB_Object3D_SetMaterial(BLB_Object3D *object, BLB_Material *material) {
  if (!object || object->material == material)
    return;

  if (material)
    BLB_Material_Retain(material);

  if (object->material)
    BLB_Material_Release(object->material);

  object->material = material;
}

typedef struct {
  BLB_Material *material;
  char texture_path[1024];
} BLB_MTLMaterial;

static char *mtl_trim(char *value) {
  if (!value)
    return value;
  while (*value && isspace((unsigned char)*value))
    ++value;
  size_t length = strlen(value);
  while (length && isspace((unsigned char)value[length - 1]))
    value[--length] = '\0';
  return value;
}

static BLB_Texture *mtl_load_texture(const char *mtl_path, const char *texture_path) {
  if (!mtl_path || !texture_path || !texture_path[0])
    return NULL;

  char resolved[2048] = {0};
  const char *slash = strrchr(mtl_path, '/');
  const char *backslash = strrchr(mtl_path, '\\');

  if (backslash && (!slash || backslash > slash))
    slash = backslash;

  bool absolute = texture_path[0] == '/' || texture_path[0] == '\\' || (strlen(texture_path) > 1 && texture_path[1] == ':');
  if (!absolute && slash) {
    size_t prefix = (size_t)(slash - mtl_path + 1);

    if (prefix + strlen(texture_path) >= sizeof(resolved))
      return NULL;

    memcpy(resolved, mtl_path, prefix);
    strcpy(resolved + prefix, texture_path);
  } else {

    if (strlen(texture_path) >= sizeof(resolved))
      return NULL;

    strcpy(resolved, texture_path);
  }

  return BLB_Texture_Load2D(resolved);
}

static void mtl_apply_texture(BLB_Material *material, const char *mtl_path, const char *texture_path) {
  if (!material || !texture_path || !texture_path[0])
    return;

  BLB_Texture *texture = mtl_load_texture(mtl_path, texture_path);
  if (!texture)
    return;

  BLB_MaterialTexture_SetTexture(material, &material->base_color_texture, texture);
  BLB_Texture_Release(texture);
}

static int material_load_mtl(BLB_Object3D *object, const char *path) {
  FILE *file = fopen(path, "rb");

  if (!file)
    return -1;

  BLB_MTLMaterial *materials = NULL;
  size_t material_count = 0;
  size_t material_capacity = 0;
  BLB_Material *current = NULL;
  char line[4096];

  while (fgets(line, sizeof(line), file)) {
    char *value = mtl_trim(line);
    if (!value[0] || value[0] == '#')
      continue;

    char *separator = value;
    while (*separator && !isspace((unsigned char)*separator))
      ++separator;

    char *argument = separator;
    if (*separator)
      *separator++ = '\0';

    argument = mtl_trim(separator);
    if (strcmp(value, "newmtl") == 0) {
      char *name_comment = strchr(argument, '#');
      if (name_comment)
        *name_comment = '\0';

      argument = mtl_trim(argument);
      if (!argument[0]) {
        current = NULL;
        continue;
      }

      if (material_count == material_capacity) {
        size_t next_capacity = material_capacity ? material_capacity * 2u : 4u;
        BLB_MTLMaterial *next = realloc(materials, next_capacity * sizeof(*next));

        if (!next) {
          fclose(file);
          for (size_t i = 0; i < material_count; ++i)
            BLB_Material_Release(materials[i].material);
          free(materials);
          return -1;
        }

        materials = next;
        material_capacity = next_capacity;
      }

      current = BLB_Material_Create3D();

      if (!current) {
        fclose(file);
        for (size_t i = 0; i < material_count; ++i)
          BLB_Material_Release(materials[i].material);

        free(materials);
        return -1;
      }

      memset(&materials[material_count], 0, sizeof(materials[material_count]));
      materials[material_count].material = current;
      BLB_Material_SetName(current, argument);
      ++material_count;
      continue;
    }

    if (!current)
      continue;

    if (strcmp(value, "Kd") == 0 || strcmp(value, "Ka") == 0) {
      float r, g, b;

      if (sscanf(argument, "%f %f %f", &r, &g, &b) == 3 && strcmp(value, "Kd") == 0)
        BLB_Material_SetBaseColor(current, r, g, b, current->base_color[3]);
    } else if (strcmp(value, "Ks") == 0) {
      float r, g, b;

      if (sscanf(argument, "%f %f %f", &r, &g, &b) == 3)
        BLB_Material_SetSpecular(current, 1.0f, r, g, b);
    } else if (strcmp(value, "Ns") == 0) {
      float shininess;

      if (sscanf(argument, "%f", &shininess) == 1 && isfinite(shininess))
        BLB_Material_SetPBR(current, current->metallic, fminf(1.0f, fmaxf(0.04f, sqrtf(2.0f / (fmaxf(0.0f, shininess) + 2.0f)))));
    } else if (strcmp(value, "Ni") == 0) {
      float ior;

      if (sscanf(argument, "%f", &ior) == 1 && isfinite(ior) && ior > 0.0f)
        BLB_Material_SetIOR(current, ior);
    } else if (strcmp(value, "d") == 0) {
      float alpha;

      if (sscanf(argument, "%f", &alpha) == 1)
        BLB_Material_SetBaseColor(current, current->base_color[0], current->base_color[1], current->base_color[2], fminf(1.0f, fmaxf(0.0f, alpha)));
    } else if (strcmp(value, "Tr") == 0) {
      float transparency;
      if (sscanf(argument, "%f", &transparency) == 1)
        BLB_Material_SetBaseColor(current, current->base_color[0], current->base_color[1], current->base_color[2],
                                  1.0f - fminf(1.0f, fmaxf(0.0f, transparency)));
    } else if (strcmp(value, "illum") == 0) {
      int illumination;

      if (sscanf(argument, "%d", &illumination) == 1 && illumination <= 1)
        BLB_Material_SetSpecular(current, 0.0f, current->specular_color[0], current->specular_color[1], current->specular_color[2]);
    } else if (strcmp(value, "map_Kd") == 0) {
      char *texture_name = argument;

      if (texture_name[0] == '"' || texture_name[0] == '\'') {
        char quote = *texture_name++;
        char *last = strrchr(texture_name, quote);
        if (last)
          *last = '\0';
      } else {
        char *comment = strchr(texture_name, '#');
        if (comment)
          *comment = '\0';

        texture_name = mtl_trim(texture_name);
      }

      if (texture_name[0] == '-') {
        char *last_space = strrchr(texture_name, ' ');
        if (last_space)
          texture_name = mtl_trim(last_space + 1);
      }

      BLB_MTLMaterial *record = &materials[material_count - 1u];
      strncpy(record->texture_path, texture_name, sizeof(record->texture_path) - 1u);
      record->texture_path[sizeof(record->texture_path) - 1u] = '\0';
    }
  }

  fclose(file);
  if (material_count == 0) {
    free(materials);
    return -1;
  }

  for (size_t i = 0; i < material_count; ++i) {
    mtl_apply_texture(materials[i].material, path, materials[i].texture_path);
    if (materials[i].material->base_color[3] < 0.999f)
      BLB_Material_SetAlphaMode(materials[i].material, BLB_ALPHA_BLEND);
  }

  bool matched_submesh = false;
  if (object->submeshes && object->submesh_count) {
    for (size_t i = 0; i < object->submesh_count; ++i) {
      BLB_Material *existing = object->submeshes[i].material;

      if (!existing || !existing->name[0])
        continue;

      for (size_t j = 0; j < material_count; ++j) {
        if (strcmp(existing->name, materials[j].material->name) != 0)
          continue;

        BLB_Material_Retain(materials[j].material);
        object->submeshes[i].material = materials[j].material;
        BLB_Material_Release(existing);
        matched_submesh = true;
        break;
      }
    }

    if (!matched_submesh) {
      for (size_t i = 0; i < object->submesh_count; ++i) {
        BLB_Material *existing = object->submeshes[i].material;
        BLB_Material *replacement = materials[i < material_count ? i : material_count - 1u].material;
        if (existing == replacement)
          continue;

        BLB_Material_Retain(replacement);
        object->submeshes[i].material = replacement;
        BLB_Material_Release(existing);
      }
    }

    BLB_Object3D_SetMaterial(object, object->submeshes[0].material);
  } else {
    BLB_Object3D_SetMaterial(object, materials[0].material);
  }

  for (size_t i = 0; i < material_count; ++i)
    BLB_Material_Release(materials[i].material);

  free(materials);
  return 0;
}

int BLB_Object3D_SetMaterialFromFile(BLB_Object3D *object, const char *path) {
  if (!object || !path || !path[0])
    return -1;
  const char *ext = strrchr(path, '.');
  if (ext && strlen(ext) == 4u && tolower((unsigned char)ext[1]) == 'm' && tolower((unsigned char)ext[2]) == 't' &&
      tolower((unsigned char)ext[3]) == 'l')
    return material_load_mtl(object, path);
  BLB_Material *material = BLB_Material_Load(path, NULL, NULL, NULL, NULL);
  if (!material || material->domain != BLB_MATERIAL_3D) {
    if (material)
      BLB_Material_Release(material);
    return -1;
  }
  BLB_Object3D_SetMaterial(object, material);
  BLB_Material_Release(material);
  return 0;
}

int BLB_Object3D_SetTexture(BLB_Object3D *object, BLB_Texture *texture) {
  if (!object)
    return -1;

  if (texture && texture->type == BLB_TEXTURE_CUBE)
    return -1;

  if (object->texture == texture)
    return 0;

  if (texture)
    BLB_Texture_Retain(texture);

  if (object->texture)
    BLB_Texture_Release(object->texture);

  object->texture = texture;
  return 0;
}

void BLB_Object3D_FlipX(BLB_Object3D *object) {
  if (!object)
    return;

  object->scale.x *= -1.0f;
  mark_transform_changed(object);
}

void BLB_Object3D_FlipY(BLB_Object3D *object) {
  if (!object)
    return;

  object->scale.y *= -1.0f;
  mark_transform_changed(object);
}

void BLB_Object3D_FlipZ(BLB_Object3D *object) {
  if (!object)
    return;

  object->scale.z *= -1.0f;
  mark_transform_changed(object);
}

void BLB_Object3D_SetHandler(BLB_Object3D *object, const BLB_ObjectHandler *handler) {
  if (!object)
    return;

  if (!handler) {
    BLB_Object3D_ClearHandler(object);
    return;
  }

  if (!object->handler) {
    object->handler = malloc(sizeof(*object->handler));
    if (!object->handler)
      return;
  }

  *object->handler = *handler;
  object->handler->captured_buttons = 0;
  object->handler->hovered = false;
  object->handler->enabled = true;
}

void BLB_Object3D_ClearHandler(BLB_Object3D *object) {
  if (!object)
    return;
  free(object->handler);
  object->handler = NULL;
}
