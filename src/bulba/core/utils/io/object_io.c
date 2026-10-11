#include "bulba/core/utils/io/object_io.h"

#include "bulba/core/math3v/physics3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/scene.h"
#include "bulba/core/utils/config.h"
#include "bulba/core/utils/object.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLB_OBJECT_MAX_ARRAY_COUNT 100000000ULL

static void BLB_ObjectFile_Log(const char *operation, const char *stage, int result) {
  if (!BLB_DEBUG)
    return;

  fprintf(stderr, "[BLB:Object] %-5s %-14s %s\n", operation, stage, result == 0 ? "OK" : "FAILED");
}

static void BLB_ObjectFile_Error(const char *message, const char *path) {
  fprintf(stderr, "[BLB:Object] ERROR: %s%s%s\n", message, path ? ": " : "", path ? path : "");
}

static int BLB_ObjectFile_Write(FILE *file, const void *data, size_t size, size_t count) {
  if (!count)
    return 0;

  if (!data)
    return -1;

  return fwrite(data, size, count, file) == count ? 0 : -1;
}

static int BLB_ObjectFile_Read(FILE *file, void *data, size_t size, size_t count) {
  if (!count)
    return 0;

  if (!data)
    return -1;

  return fread(data, size, count, file) == count ? 0 : -1;
}

static int BLB_ObjectFile_WriteBool(FILE *file, bool value) {
  uint8_t data = value ? 1u : 0u;
  return BLB_ObjectFile_Write(file, &data, sizeof(data), 1);
}

static int BLB_ObjectFile_ReadBool(FILE *file, bool *value) {
  uint8_t data;

  if (BLB_ObjectFile_Read(file, &data, sizeof(data), 1) != 0)
    return -1;

  if (data > 1)
    return -1;

  *value = data != 0;
  return 0;
}

static int BLB_ObjectFile_WriteU64(FILE *file, uint64_t value) { return BLB_ObjectFile_Write(file, &value, sizeof(value), 1); }

static int BLB_ObjectFile_ReadU64(FILE *file, uint64_t *value) { return BLB_ObjectFile_Read(file, value, sizeof(*value), 1); }

static int BLB_ObjectFile_WriteVec2(FILE *file, HMM_Vec2 value) {
  return BLB_ObjectFile_Write(file, &value.x, sizeof(value.x), 1) || BLB_ObjectFile_Write(file, &value.y, sizeof(value.y), 1);
}

static int BLB_ObjectFile_ReadVec2(FILE *file, HMM_Vec2 *value) {
  return BLB_ObjectFile_Read(file, &value->x, sizeof(value->x), 1) || BLB_ObjectFile_Read(file, &value->y, sizeof(value->y), 1);
}

static int BLB_ObjectFile_WriteVec3(FILE *file, HMM_Vec3 value) {
  return BLB_ObjectFile_Write(file, &value.x, sizeof(value.x), 1) || BLB_ObjectFile_Write(file, &value.y, sizeof(value.y), 1) ||
         BLB_ObjectFile_Write(file, &value.z, sizeof(value.z), 1);
}

static int BLB_ObjectFile_ReadVec3(FILE *file, HMM_Vec3 *value) {
  return BLB_ObjectFile_Read(file, &value->x, sizeof(value->x), 1) || BLB_ObjectFile_Read(file, &value->y, sizeof(value->y), 1) ||
         BLB_ObjectFile_Read(file, &value->z, sizeof(value->z), 1);
}

static int BLB_ObjectFile_WriteQuat(FILE *file, HMM_Quat value) {
  return BLB_ObjectFile_Write(file, &value.x, sizeof(value.x), 1) || BLB_ObjectFile_Write(file, &value.y, sizeof(value.y), 1) ||
         BLB_ObjectFile_Write(file, &value.z, sizeof(value.z), 1) || BLB_ObjectFile_Write(file, &value.w, sizeof(value.w), 1);
}

static int BLB_ObjectFile_ReadQuat(FILE *file, HMM_Quat *value) {
  return BLB_ObjectFile_Read(file, &value->x, sizeof(value->x), 1) || BLB_ObjectFile_Read(file, &value->y, sizeof(value->y), 1) ||
         BLB_ObjectFile_Read(file, &value->z, sizeof(value->z), 1) || BLB_ObjectFile_Read(file, &value->w, sizeof(value->w), 1);
}

static int BLB_ObjectFile_CheckExtension(const char *path, const char *extension) {
  if (!path || !extension)
    return -1;

  const char *dot = strrchr(path, '.');

  if (!dot)
    return -1;

  return strcmp(dot, extension) == 0 ? 0 : -1;
}

static int BLB_ObjectFile_WriteHeader(FILE *file, BLB_ObjectFileType type) {
  BLB_ObjectFileHeader header = {.magic = {'B', (char)('0' + type), 'O', '\0'}, .version = BLB_OBJECT_FILE_VERSION, .type = (uint32_t)type};

  return BLB_ObjectFile_Write(file, &header, sizeof(header), 1);
}

static int BLB_ObjectFile_ReadHeader(FILE *file, BLB_ObjectFileType expected_type, uint32_t *version) {
  BLB_ObjectFileHeader header;
  char expected_magic[4] = {'B', (char)('0' + expected_type), 'O', '\0'};

  if (BLB_ObjectFile_Read(file, &header, sizeof(header), 1) != 0)
    return -1;

  if (memcmp(header.magic, expected_magic, sizeof(expected_magic)) != 0)
    return -2;

  if (header.version == 0 || header.version > BLB_OBJECT_FILE_VERSION)
    return -3;

  if (header.type != (uint32_t)expected_type)
    return -4;

  if (version)
    *version = header.version;
  return 0;
}

static int BLB_ObjectFile_WriteArray(FILE *file, const void *data, size_t count, size_t element_size) {
  if ((uint64_t)count > BLB_OBJECT_MAX_ARRAY_COUNT)
    return -1;

  if (BLB_ObjectFile_WriteU64(file, (uint64_t)count) != 0)
    return -1;

  if (!count)
    return 0;

  if (!data)
    return -1;

  return BLB_ObjectFile_Write(file, data, element_size, count);
}

static int BLB_ObjectFile_ReadArray(FILE *file, void **data, size_t *count, size_t element_size) {
  uint64_t serialized_count;

  *data = NULL;
  *count = 0;

  if (BLB_ObjectFile_ReadU64(file, &serialized_count) != 0)
    return -1;

  if (serialized_count > BLB_OBJECT_MAX_ARRAY_COUNT || serialized_count > SIZE_MAX)
    return -1;

  *count = (size_t)serialized_count;

  if (!*count)
    return 0;

  if (*count > SIZE_MAX / element_size)
    return -1;

  *data = malloc(*count * element_size);

  if (!*data)
    return -1;

  if (BLB_ObjectFile_Read(file, *data, element_size, *count) != 0) {
    free(*data);
    *data = NULL;
    *count = 0;
    return -1;
  }

  return 0;
}

static int BLB_ObjectFile_WriteOptionalArray(FILE *file, const void *data, size_t count, size_t element_size) {
  if (BLB_ObjectFile_WriteBool(file, data != NULL) != 0)
    return -1;

  if (!data)
    return 0;

  return BLB_ObjectFile_WriteArray(file, data, count, element_size);
}

static int BLB_ObjectFile_ReadOptionalArray(FILE *file, void **data, size_t *count, size_t element_size) {
  bool present;

  *data = NULL;
  *count = 0;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  return BLB_ObjectFile_ReadArray(file, data, count, element_size);
}

static int BLB_ObjectFile_WriteObjectID(FILE *file, const BLB_ObjectID *id) {
  if (BLB_ObjectFile_WriteBool(file, id != NULL) != 0)
    return -1;

  if (!id)
    return 0;

  if (BLB_ObjectFile_Write(file, &id->id, sizeof(id->id), 1) != 0)
    return -1;

  return BLB_ObjectFile_Write(file, id->id_type, sizeof(id->id_type), 1);
}

static int BLB_ObjectFile_ReadObjectID(FILE *file, BLB_ObjectID **out_id) {
  bool present;
  BLB_ObjectID value;

  *out_id = NULL;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  if (BLB_ObjectFile_Read(file, &value.id, sizeof(value.id), 1) != 0)
    return -1;

  if (BLB_ObjectFile_Read(file, value.id_type, sizeof(value.id_type), 1) != 0)
    return -1;

  if (!memchr(value.id_type, '\0', sizeof(value.id_type)))
    return -1;

  for (size_t i = 0; i < BLB_OBJECTS_ID_COUNT; ++i) {
    if (BLB_OBJECTS_ID[i].id == value.id && strcmp(BLB_OBJECTS_ID[i].id_type, value.id_type) == 0) {
      *out_id = &BLB_OBJECTS_ID[i];
      return 0;
    }
  }

  *out_id = malloc(sizeof(**out_id));

  if (!*out_id)
    return -1;

  **out_id = value;
  return 0;
}

static bool BLB_ObjectFile_IsGlobalObjectID(const BLB_ObjectID *id) {
  if (!id || !BLB_OBJECTS_ID)
    return false;

  for (size_t i = 0; i < BLB_OBJECTS_ID_COUNT; ++i) {
    if (id == &BLB_OBJECTS_ID[i])
      return true;
  }

  return false;
}

static int BLB_ObjectFile_WriteTexture(FILE *file, const BLB_Texture *texture) {
  if (BLB_ObjectFile_WriteBool(file, texture != NULL) != 0)
    return -1;

  if (!texture)
    return 0;

  if (texture->pixel_size && !texture->pixels)
    return -1;

  uint32_t type = (uint32_t)texture->type;
  uint32_t format = (uint32_t)texture->format;

  if (BLB_ObjectFile_Write(file, &type, sizeof(type), 1) != 0 || BLB_ObjectFile_Write(file, &format, sizeof(format), 1) != 0 ||
      BLB_ObjectFile_Write(file, &texture->width, sizeof(texture->width), 1) != 0 ||
      BLB_ObjectFile_Write(file, &texture->height, sizeof(texture->height), 1) != 0 ||
      BLB_ObjectFile_Write(file, &texture->depth, sizeof(texture->depth), 1) != 0 || BLB_ObjectFile_WriteBool(file, texture->clamp_to_edge) != 0 ||
      BLB_ObjectFile_WriteU64(file, (uint64_t)texture->pixel_size) != 0)
    return -1;

  if (BLB_ObjectFile_Write(file, texture->pixels, 1, texture->pixel_size) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_ReadTexture(FILE *file, BLB_Texture **out_texture) {
  bool present;
  uint32_t type;
  uint32_t format;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
  bool clamp_to_edge;
  uint64_t pixel_size64;
  unsigned char *pixels = NULL;

  *out_texture = NULL;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  if (BLB_ObjectFile_Read(file, &type, sizeof(type), 1) != 0 || BLB_ObjectFile_Read(file, &format, sizeof(format), 1) != 0 ||
      BLB_ObjectFile_Read(file, &width, sizeof(width), 1) != 0 || BLB_ObjectFile_Read(file, &height, sizeof(height), 1) != 0 ||
      BLB_ObjectFile_Read(file, &depth, sizeof(depth), 1) != 0 || BLB_ObjectFile_ReadBool(file, &clamp_to_edge) != 0 ||
      BLB_ObjectFile_ReadU64(file, &pixel_size64) != 0)
    return -1;

  if (type > BLB_TEXTURE_CUBE || format > BLB_TEXTURE_FORMAT_RGBA8)
    return -1;

  if (pixel_size64 > SIZE_MAX)
    return -1;

  size_t pixel_size = (size_t)pixel_size64;

  if (type == BLB_TEXTURE_CUBE)
    return -1;

  if (width == 0 || height == 0 || (type == BLB_TEXTURE_3D && depth == 0) || (type == BLB_TEXTURE_2D && depth != 1))
    return -1;

  if ((size_t)width > SIZE_MAX / (size_t)height)
    return -1;
  size_t texels = (size_t)width * (size_t)height;
  if (type == BLB_TEXTURE_3D) {
    if ((size_t)depth > SIZE_MAX / texels)
      return -1;
    texels *= (size_t)depth;
  }
  if (texels > SIZE_MAX / 4u || pixel_size != texels * 4u)
    return -1;

  if (pixel_size) {
    pixels = malloc(pixel_size);

    if (!pixels)
      return -1;

    if (BLB_ObjectFile_Read(file, pixels, 1, pixel_size) != 0) {
      free(pixels);
      return -1;
    }
  }

  BLB_Texture *texture = type == BLB_TEXTURE_3D ? BLB_Texture_Create3D(width, height, depth, pixels, pixel_size)
                                                : BLB_Texture_Create2D(width, height, pixels, pixel_size);

  free(pixels);

  if (!texture)
    return -1;

  texture->format = (BLB_TextureFormat)format;
  texture->depth = depth;
  texture->clamp_to_edge = clamp_to_edge;

  *out_texture = texture;
  return 0;
}

static int BLB_ObjectFile_WriteMaterialTexture(FILE *file, const BLB_MaterialTexture *slot) {
  uint32_t wrap_u = (uint32_t)slot->wrap_u;
  uint32_t wrap_v = (uint32_t)slot->wrap_v;
  uint32_t min_filter = (uint32_t)slot->min_filter;
  uint32_t mag_filter = (uint32_t)slot->mag_filter;

  if (BLB_ObjectFile_WriteTexture(file, slot->texture) != 0 || BLB_ObjectFile_Write(file, &slot->uv_set, sizeof(slot->uv_set), 1) != 0 ||
      BLB_ObjectFile_Write(file, slot->offset, sizeof(slot->offset), 1) != 0 ||
      BLB_ObjectFile_Write(file, slot->scale, sizeof(slot->scale), 1) != 0 ||
      BLB_ObjectFile_Write(file, &slot->rotation, sizeof(slot->rotation), 1) != 0 || BLB_ObjectFile_Write(file, &wrap_u, sizeof(wrap_u), 1) != 0 ||
      BLB_ObjectFile_Write(file, &wrap_v, sizeof(wrap_v), 1) != 0 || BLB_ObjectFile_Write(file, &min_filter, sizeof(min_filter), 1) != 0 ||
      BLB_ObjectFile_Write(file, &mag_filter, sizeof(mag_filter), 1) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_ReadMaterialTexture(FILE *file, BLB_MaterialTexture *slot) {
  BLB_Texture *texture = NULL;
  uint32_t wrap_u;
  uint32_t wrap_v;
  uint32_t min_filter;
  uint32_t mag_filter;

  if (BLB_ObjectFile_ReadTexture(file, &texture) != 0)
    return -1;

  if (BLB_ObjectFile_Read(file, &slot->uv_set, sizeof(slot->uv_set), 1) != 0 ||
      BLB_ObjectFile_Read(file, slot->offset, sizeof(slot->offset), 1) != 0 || BLB_ObjectFile_Read(file, slot->scale, sizeof(slot->scale), 1) != 0 ||
      BLB_ObjectFile_Read(file, &slot->rotation, sizeof(slot->rotation), 1) != 0 || BLB_ObjectFile_Read(file, &wrap_u, sizeof(wrap_u), 1) != 0 ||
      BLB_ObjectFile_Read(file, &wrap_v, sizeof(wrap_v), 1) != 0 || BLB_ObjectFile_Read(file, &min_filter, sizeof(min_filter), 1) != 0 ||
      BLB_ObjectFile_Read(file, &mag_filter, sizeof(mag_filter), 1) != 0) {
    if (texture)
      BLB_Texture_Release(texture);

    return -1;
  }

  slot->uv_set = slot->uv_set;
  slot->rotation = slot->rotation;
  slot->wrap_u = (BLB_TextureWrap)wrap_u;
  slot->wrap_v = (BLB_TextureWrap)wrap_v;
  slot->min_filter = (BLB_TextureFilter)min_filter;
  slot->mag_filter = (BLB_TextureFilter)mag_filter;

  if (texture) {
    BLB_MaterialTexture_AdoptTexture(slot, texture);
  } else {
    slot->texture = NULL;
    slot->owned = false;
  }

  return 0;
}

static int BLB_ObjectFile_WriteMaterial(FILE *file, const BLB_Material *material) {
  if (BLB_ObjectFile_WriteBool(file, material != NULL) != 0)
    return -1;

  if (!material)
    return 0;

  uint32_t domain = (uint32_t)material->domain;
  uint32_t render_mode = (uint32_t)material->render_mode;
  uint32_t alpha_mode = (uint32_t)material->alpha_mode;

  if (BLB_ObjectFile_Write(file, &domain, sizeof(domain), 1) != 0 || BLB_ObjectFile_Write(file, &render_mode, sizeof(render_mode), 1) != 0 ||
      BLB_ObjectFile_Write(file, &alpha_mode, sizeof(alpha_mode), 1) != 0 || BLB_ObjectFile_WriteBool(file, material->lighting_enabled) != 0 ||
      BLB_ObjectFile_WriteBool(file, material->depth_enabled) != 0 || BLB_ObjectFile_WriteBool(file, material->depth_write) != 0 ||
      BLB_ObjectFile_WriteBool(file, material->double_sided) != 0 || BLB_ObjectFile_WriteBool(file, material->unlit) != 0 ||
      BLB_ObjectFile_Write(file, material->base_color, sizeof(material->base_color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->metallic, sizeof(material->metallic), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->roughness, sizeof(material->roughness), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->normal_scale, sizeof(material->normal_scale), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->occlusion_strength, sizeof(material->occlusion_strength), 1) != 0 ||
      BLB_ObjectFile_Write(file, material->emission_color, sizeof(material->emission_color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->emission_strength, sizeof(material->emission_strength), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->alpha_cutoff, sizeof(material->alpha_cutoff), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->specular_factor, sizeof(material->specular_factor), 1) != 0 ||
      BLB_ObjectFile_Write(file, material->specular_color, sizeof(material->specular_color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->ior, sizeof(material->ior), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->transmission, sizeof(material->transmission), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->volume_thickness, sizeof(material->volume_thickness), 1) != 0 ||
      BLB_ObjectFile_Write(file, material->attenuation_color, sizeof(material->attenuation_color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->attenuation_distance, sizeof(material->attenuation_distance), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->clearcoat_factor, sizeof(material->clearcoat_factor), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->clearcoat_roughness, sizeof(material->clearcoat_roughness), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->clearcoat_normal_scale, sizeof(material->clearcoat_normal_scale), 1) != 0 ||
      BLB_ObjectFile_Write(file, material->sheen_color, sizeof(material->sheen_color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->sheen_roughness, sizeof(material->sheen_roughness), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->iridescence_factor, sizeof(material->iridescence_factor), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->iridescence_ior, sizeof(material->iridescence_ior), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->iridescence_thickness_min, sizeof(material->iridescence_thickness_min), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->iridescence_thickness_max, sizeof(material->iridescence_thickness_max), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->anisotropy_strength, sizeof(material->anisotropy_strength), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->anisotropy_rotation, sizeof(material->anisotropy_rotation), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->dispersion, sizeof(material->dispersion), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->glow_strength, sizeof(material->glow_strength), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->glow_radius, sizeof(material->glow_radius), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->glow_falloff, sizeof(material->glow_falloff), 1) != 0 ||
      BLB_ObjectFile_Write(file, &material->temperature, sizeof(material->temperature), 1) != 0 ||
      BLB_ObjectFile_Write(file, material->name, sizeof(material->name), 1) != 0)
    return -1;

  const BLB_MaterialTexture *slots[] = {&material->base_color_texture,
                                        &material->metallic_roughness_texture,
                                        &material->normal_texture,
                                        &material->occlusion_texture,
                                        &material->emission_texture,
                                        &material->specular_texture,
                                        &material->specular_color_texture,
                                        &material->clearcoat_texture,
                                        &material->clearcoat_roughness_texture,
                                        &material->clearcoat_normal_texture,
                                        &material->transmission_texture,
                                        &material->thickness_texture,
                                        &material->sheen_color_texture,
                                        &material->sheen_roughness_texture,
                                        &material->iridescence_texture,
                                        &material->iridescence_thickness_texture,
                                        &material->anisotropy_texture};

  for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
    if (BLB_ObjectFile_WriteMaterialTexture(file, slots[i]) != 0)
      return -1;
  }

  return 0;
}

static int BLB_ObjectFile_ReadMaterial(FILE *file, BLB_Material **out_material) {
  bool present;
  uint32_t domain;
  uint32_t render_mode;
  uint32_t alpha_mode;

  *out_material = NULL;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  if (BLB_ObjectFile_Read(file, &domain, sizeof(domain), 1) != 0 || BLB_ObjectFile_Read(file, &render_mode, sizeof(render_mode), 1) != 0 ||
      BLB_ObjectFile_Read(file, &alpha_mode, sizeof(alpha_mode), 1) != 0)
    return -1;

  if (domain > BLB_MATERIAL_3D)
    return -1;

  BLB_Material *material = BLB_Material_Create((BLB_MaterialDomain)domain);

  if (!material)
    return -1;

  material->render_mode = (BLB_RenderMode)render_mode;
  material->alpha_mode = (BLB_AlphaMode)alpha_mode;

  if (BLB_ObjectFile_ReadBool(file, &material->lighting_enabled) != 0 || BLB_ObjectFile_ReadBool(file, &material->depth_enabled) != 0 ||
      BLB_ObjectFile_ReadBool(file, &material->depth_write) != 0 || BLB_ObjectFile_ReadBool(file, &material->double_sided) != 0 ||
      BLB_ObjectFile_ReadBool(file, &material->unlit) != 0 || BLB_ObjectFile_Read(file, material->base_color, sizeof(material->base_color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->metallic, sizeof(material->metallic), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->roughness, sizeof(material->roughness), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->normal_scale, sizeof(material->normal_scale), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->occlusion_strength, sizeof(material->occlusion_strength), 1) != 0 ||
      BLB_ObjectFile_Read(file, material->emission_color, sizeof(material->emission_color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->emission_strength, sizeof(material->emission_strength), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->alpha_cutoff, sizeof(material->alpha_cutoff), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->specular_factor, sizeof(material->specular_factor), 1) != 0 ||
      BLB_ObjectFile_Read(file, material->specular_color, sizeof(material->specular_color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->ior, sizeof(material->ior), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->transmission, sizeof(material->transmission), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->volume_thickness, sizeof(material->volume_thickness), 1) != 0 ||
      BLB_ObjectFile_Read(file, material->attenuation_color, sizeof(material->attenuation_color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->attenuation_distance, sizeof(material->attenuation_distance), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->clearcoat_factor, sizeof(material->clearcoat_factor), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->clearcoat_roughness, sizeof(material->clearcoat_roughness), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->clearcoat_normal_scale, sizeof(material->clearcoat_normal_scale), 1) != 0 ||
      BLB_ObjectFile_Read(file, material->sheen_color, sizeof(material->sheen_color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->sheen_roughness, sizeof(material->sheen_roughness), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->iridescence_factor, sizeof(material->iridescence_factor), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->iridescence_ior, sizeof(material->iridescence_ior), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->iridescence_thickness_min, sizeof(material->iridescence_thickness_min), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->iridescence_thickness_max, sizeof(material->iridescence_thickness_max), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->anisotropy_strength, sizeof(material->anisotropy_strength), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->anisotropy_rotation, sizeof(material->anisotropy_rotation), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->dispersion, sizeof(material->dispersion), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->glow_strength, sizeof(material->glow_strength), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->glow_radius, sizeof(material->glow_radius), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->glow_falloff, sizeof(material->glow_falloff), 1) != 0 ||
      BLB_ObjectFile_Read(file, &material->temperature, sizeof(material->temperature), 1) != 0 ||
      BLB_ObjectFile_Read(file, material->name, sizeof(material->name), 1) != 0)
    goto error;

  BLB_MaterialTexture *slots[] = {&material->base_color_texture,
                                  &material->metallic_roughness_texture,
                                  &material->normal_texture,
                                  &material->occlusion_texture,
                                  &material->emission_texture,
                                  &material->specular_texture,
                                  &material->specular_color_texture,
                                  &material->clearcoat_texture,
                                  &material->clearcoat_roughness_texture,
                                  &material->clearcoat_normal_texture,
                                  &material->transmission_texture,
                                  &material->thickness_texture,
                                  &material->sheen_color_texture,
                                  &material->sheen_roughness_texture,
                                  &material->iridescence_texture,
                                  &material->iridescence_thickness_texture,
                                  &material->anisotropy_texture};

  for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
    if (BLB_ObjectFile_ReadMaterialTexture(file, slots[i]) != 0)
      goto error;
  }

  *out_material = material;
  return 0;
error:
  BLB_Material_Release(material);
  return -1;
}

static int BLB_ObjectFile_WriteMesh(FILE *file, const Mesh *mesh) {
  if (BLB_ObjectFile_WriteOptionalArray(file, mesh->vertices, mesh->vertex_count, sizeof(*mesh->vertices)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, mesh->normals, mesh->vertex_count, sizeof(*mesh->normals)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, mesh->uvs, mesh->vertex_count, sizeof(*mesh->uvs)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, mesh->indices, mesh->index_count, sizeof(*mesh->indices)) != 0)
    return -1;

  return 0;
}

static void BLB_ObjectFile_DestroyMesh(Mesh *mesh) {
  if (!mesh)
    return;

  free(mesh->vertices);
  free(mesh->normals);
  free(mesh->uvs);
  free(mesh->indices);

  memset(mesh, 0, sizeof(*mesh));
}

static int BLB_ObjectFile_ReadMesh(FILE *file, Mesh *mesh) {
  size_t vertices_count;
  size_t normals_count;
  size_t uvs_count;
  size_t indices_count;

  memset(mesh, 0, sizeof(*mesh));

  if (BLB_ObjectFile_ReadOptionalArray(file, (void **)&mesh->vertices, &vertices_count, sizeof(*mesh->vertices)) != 0)
    goto error;

  if (BLB_ObjectFile_ReadOptionalArray(file, (void **)&mesh->normals, &normals_count, sizeof(*mesh->normals)) != 0)
    goto error;

  if (BLB_ObjectFile_ReadOptionalArray(file, (void **)&mesh->uvs, &uvs_count, sizeof(*mesh->uvs)) != 0)
    goto error;

  if (BLB_ObjectFile_ReadOptionalArray(file, (void **)&mesh->indices, &indices_count, sizeof(*mesh->indices)) != 0)
    goto error;

  mesh->vertex_count = vertices_count;
  mesh->index_count = indices_count;

  if ((mesh->normals && normals_count != mesh->vertex_count) || (mesh->uvs && uvs_count != mesh->vertex_count))
    goto error;

  return 0;
error:
  BLB_ObjectFile_DestroyMesh(mesh);
  return -1;
}

static int BLB_ObjectFile_WritePolygon3D(FILE *file, const BLB_Polygon3D *polygon) {
  if (BLB_ObjectFile_WriteBool(file, polygon != NULL) != 0)
    return -1;

  if (!polygon)
    return 0;

  if (BLB_ObjectFile_WriteOptionalArray(file, polygon->vertices, polygon->vertex_count, sizeof(*polygon->vertices)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->base_vertices, polygon->vertex_count, sizeof(*polygon->base_vertices)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->normals, polygon->vertex_count, sizeof(*polygon->normals)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->uvs, polygon->vertex_count, sizeof(*polygon->uvs)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->indices, polygon->index_count, sizeof(*polygon->indices)) != 0 ||
      BLB_ObjectFile_WriteVec3(file, polygon->normal) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_ReadPolygon3D(FILE *file, BLB_Polygon3D **out_polygon) {
  bool present;
  BLB_Polygon3D *polygon = NULL;
  size_t vertices_count;
  size_t base_count;
  size_t normals_count;
  size_t uvs_count;
  size_t indices_count;

  *out_polygon = NULL;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return -1;

  if (BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->vertices, &vertices_count, sizeof(*polygon->vertices)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->base_vertices, &base_count, sizeof(*polygon->base_vertices)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->normals, &normals_count, sizeof(*polygon->normals)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->uvs, &uvs_count, sizeof(*polygon->uvs)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->indices, &indices_count, sizeof(*polygon->indices)) != 0 ||
      BLB_ObjectFile_ReadVec3(file, &polygon->normal) != 0)
    goto error;

  polygon->vertex_count = vertices_count;
  polygon->index_count = indices_count;

  if ((polygon->base_vertices && base_count != polygon->vertex_count) || (polygon->normals && normals_count != polygon->vertex_count) ||
      (polygon->uvs && uvs_count != polygon->vertex_count))
    goto error;

  *out_polygon = polygon;
  return 0;
error:
  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->normals);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
  return -1;
}

static int BLB_ObjectFile_WritePolygon2D(FILE *file, const BLB_Polygon2D *polygon) {
  if (BLB_ObjectFile_WriteBool(file, polygon != NULL) != 0)
    return -1;

  if (!polygon)
    return 0;

  if (BLB_ObjectFile_WriteOptionalArray(file, polygon->vertices, polygon->vertex_count, sizeof(*polygon->vertices)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->base_vertices, polygon->vertex_count, sizeof(*polygon->base_vertices)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->uvs, polygon->vertex_count, sizeof(*polygon->uvs)) != 0 ||
      BLB_ObjectFile_WriteOptionalArray(file, polygon->indices, polygon->index_count, sizeof(*polygon->indices)) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_ReadPolygon2D(FILE *file, BLB_Polygon2D **out_polygon) {
  bool present;
  BLB_Polygon2D *polygon = NULL;
  size_t vertices_count;
  size_t base_count;
  size_t uvs_count;
  size_t indices_count;

  *out_polygon = NULL;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  polygon = calloc(1, sizeof(*polygon));

  if (!polygon)
    return -1;

  if (BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->vertices, &vertices_count, sizeof(*polygon->vertices)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->base_vertices, &base_count, sizeof(*polygon->base_vertices)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->uvs, &uvs_count, sizeof(*polygon->uvs)) != 0 ||
      BLB_ObjectFile_ReadOptionalArray(file, (void **)&polygon->indices, &indices_count, sizeof(*polygon->indices)) != 0)
    goto error;

  polygon->vertex_count = vertices_count;
  polygon->index_count = indices_count;

  if ((polygon->base_vertices && base_count != polygon->vertex_count) || (polygon->uvs && uvs_count != polygon->vertex_count))
    goto error;

  *out_polygon = polygon;
  return 0;
error:
  free(polygon->vertices);
  free(polygon->base_vertices);
  free(polygon->uvs);
  free(polygon->indices);
  free(polygon);
  return -1;
}

static int BLB_ObjectFile_WriteAnimation(FILE *file, const BLB_Animation *animation) {
  if (BLB_ObjectFile_WriteBool(file, animation != NULL) != 0)
    return -1;

  if (!animation)
    return 0;

  if ((uint64_t)animation->textures_count > BLB_OBJECT_MAX_ARRAY_COUNT)
    return -1;

  if (BLB_ObjectFile_Write(file, &animation->texture_counter, sizeof(animation->texture_counter), 1) != 0 ||
      BLB_ObjectFile_WriteU64(file, (uint64_t)animation->textures_count) != 0 ||
      BLB_ObjectFile_Write(file, &animation->frame_time, sizeof(animation->frame_time), 1) != 0 ||
      BLB_ObjectFile_WriteBool(file, animation->enable) != 0 ||
      BLB_ObjectFile_Write(file, &animation->frame_count, sizeof(animation->frame_count), 1) != 0)
    return -1;

  for (size_t i = 0; i < animation->textures_count; ++i) {
    if (BLB_ObjectFile_WriteTexture(file, animation->textures[i]) != 0)
      return -1;
  }

  return 0;
}

static int BLB_ObjectFile_ReadAnimation(FILE *file, BLB_Animation **out_animation) {
  bool present;
  uint64_t textures_count;
  BLB_Animation *animation = NULL;

  *out_animation = NULL;

  if (BLB_ObjectFile_ReadBool(file, &present) != 0)
    return -1;

  if (!present)
    return 0;

  animation = calloc(1, sizeof(*animation));

  if (!animation)
    return -1;

  if (BLB_ObjectFile_Read(file, &animation->texture_counter, sizeof(animation->texture_counter), 1) != 0 ||
      BLB_ObjectFile_ReadU64(file, &textures_count) != 0 ||
      BLB_ObjectFile_Read(file, &animation->frame_time, sizeof(animation->frame_time), 1) != 0 ||
      BLB_ObjectFile_ReadBool(file, &animation->enable) != 0 ||
      BLB_ObjectFile_Read(file, &animation->frame_count, sizeof(animation->frame_count), 1) != 0)
    goto error;

  if (textures_count > BLB_OBJECT_MAX_ARRAY_COUNT || textures_count > SIZE_MAX)
    goto error;

  animation->textures_count = (size_t)textures_count;

  if (animation->textures_count) {
    animation->textures = calloc(animation->textures_count, sizeof(BLB_Texture));

    if (!animation->textures)
      goto error;
  }

  for (size_t i = 0; i < animation->textures_count; ++i) {
    if (BLB_ObjectFile_ReadTexture(file, &animation->textures[i]) != 0)
      goto error;

    if (!animation->textures[i])
      goto error;
  }

  *out_animation = animation;
  return 0;
error:
  if (animation) {
    for (size_t i = 0; i < animation->textures_count; ++i) {
      if (animation->textures && animation->textures[i])
        BLB_Texture_Release(animation->textures[i]);
    }

    free(animation->textures);
    free(animation);
  }

  return -1;
}

static void BLB_ObjectFile_DestroyMesh(Mesh *mesh);

static int BLB_ObjectFile_WritePhysicsShape3D(FILE *file, const BLB_Physics3DShape *shape) {
  uint32_t type;

  if (!shape)
    return -1;

  type = (uint32_t)shape->type;

  if (BLB_ObjectFile_Write(file, &type, sizeof(type), 1) != 0)
    return -1;

  switch (shape->type) {
  case BLB_PHYSICS3D_SHAPE_BOX:
    return BLB_ObjectFile_WriteVec3(file, shape->data.box.half_extents);

  case BLB_PHYSICS3D_SHAPE_SPHERE:
    return BLB_ObjectFile_Write(file, &shape->data.sphere.radius, sizeof(float), 1);

  case BLB_PHYSICS3D_SHAPE_CAPSULE:
    return BLB_ObjectFile_Write(file, &shape->data.capsule.radius, sizeof(float), 1) ||
           BLB_ObjectFile_Write(file, &shape->data.capsule.half_height, sizeof(float), 1);

  case BLB_PHYSICS3D_SHAPE_PLANE:
    return BLB_ObjectFile_WriteVec3(file, shape->data.plane.normal) || BLB_ObjectFile_Write(file, &shape->data.plane.distance, sizeof(float), 1);

  case BLB_PHYSICS3D_SHAPE_CONVEX_HULL:
    return BLB_ObjectFile_WriteArray(file, shape->data.convex_hull.vertices, shape->data.convex_hull.vertex_count,
                                     sizeof(*shape->data.convex_hull.vertices));

  case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH:
    if (BLB_ObjectFile_WriteArray(file, shape->data.triangle_mesh.vertices, shape->data.triangle_mesh.vertex_count,
                                  sizeof(*shape->data.triangle_mesh.vertices)) != 0)
      return -1;

    return BLB_ObjectFile_WriteArray(file, shape->data.triangle_mesh.indices, shape->data.triangle_mesh.index_count,
                                     sizeof(*shape->data.triangle_mesh.indices));

  case BLB_PHYSICS3D_SHAPE_COMPOUND:
    BLB_ObjectFile_Error("compound 3D physics shape is not supported", NULL);
    return -1;

  default:
    return -1;
  }
}

static int BLB_ObjectFile_ReadPhysicsShape3D(FILE *file, BLB_Physics3DShape *shape) {
  uint32_t type;

  memset(shape, 0, sizeof(*shape));

  if (BLB_ObjectFile_Read(file, &type, sizeof(type), 1) != 0)
    return -1;

  if (type > BLB_PHYSICS3D_SHAPE_COMPOUND)
    return -1;

  shape->type = (BLB_Physics3DShapeType)type;

  switch (shape->type) {
  case BLB_PHYSICS3D_SHAPE_BOX:
    return BLB_ObjectFile_ReadVec3(file, &shape->data.box.half_extents);

  case BLB_PHYSICS3D_SHAPE_SPHERE:
    return BLB_ObjectFile_Read(file, &shape->data.sphere.radius, sizeof(float), 1);

  case BLB_PHYSICS3D_SHAPE_CAPSULE:
    return BLB_ObjectFile_Read(file, &shape->data.capsule.radius, sizeof(float), 1) ||
           BLB_ObjectFile_Read(file, &shape->data.capsule.half_height, sizeof(float), 1);

  case BLB_PHYSICS3D_SHAPE_PLANE:
    return BLB_ObjectFile_ReadVec3(file, &shape->data.plane.normal) || BLB_ObjectFile_Read(file, &shape->data.plane.distance, sizeof(float), 1);

  case BLB_PHYSICS3D_SHAPE_CONVEX_HULL:
    return BLB_ObjectFile_ReadArray(file, (void **)&shape->data.convex_hull.vertices, &shape->data.convex_hull.vertex_count,
                                    sizeof(*shape->data.convex_hull.vertices));

  case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH:
    if (BLB_ObjectFile_ReadArray(file, (void **)&shape->data.triangle_mesh.vertices, &shape->data.triangle_mesh.vertex_count,
                                 sizeof(*shape->data.triangle_mesh.vertices)) != 0)
      return -1;

    return BLB_ObjectFile_ReadArray(file, (void **)&shape->data.triangle_mesh.indices, &shape->data.triangle_mesh.index_count,
                                    sizeof(*shape->data.triangle_mesh.indices));

  case BLB_PHYSICS3D_SHAPE_COMPOUND:
    return -1;

  default:
    return -1;
  }
}

static void BLB_ObjectFile_DestroyPhysicsShape3D(BLB_Physics3DShape *shape) {
  if (!shape)
    return;

  switch (shape->type) {
  case BLB_PHYSICS3D_SHAPE_CONVEX_HULL:
    free(shape->data.convex_hull.vertices);
    break;

  case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH:
    free(shape->data.triangle_mesh.vertices);
    free(shape->data.triangle_mesh.indices);
    break;

  default:
    break;
  }

  memset(shape, 0, sizeof(*shape));
}

static int BLB_ObjectFile_WritePhysics3D(FILE *file, const BLB_Object3D *object) {
  bool body_present = object->rigid_body != NULL;
  bool collider_present = object->collider != NULL;

  if (BLB_ObjectFile_WriteBool(file, body_present) != 0)
    return -1;

  if (!body_present)
    return 0;

  if (BLB_ObjectFile_WriteBool(file, collider_present) != 0)
    return -1;

  BLB_RigidBody3D *body = (BLB_RigidBody3D *)object->rigid_body;

  uint32_t body_type = (uint32_t)BLB_Physics3D_BodyGetType(body);
  bool body_active = BLB_Physics3D_BodyIsActive(body);
  HMM_Vec3 body_position = BLB_Physics3D_BodyGetPosition(body);
  HMM_Quat body_rotation = BLB_Physics3D_BodyGetRotation(body);
  HMM_Vec3 body_velocity = BLB_Physics3D_BodyGetVelocity(body);
  HMM_Vec3 body_angular_velocity = BLB_Physics3D_BodyGetAngularVelocity(body);

  if (BLB_ObjectFile_Write(file, &body_type, sizeof(body_type), 1) != 0 || BLB_ObjectFile_WriteBool(file, body_active) != 0 ||
      BLB_ObjectFile_WriteVec3(file, body_position) != 0 || BLB_ObjectFile_WriteQuat(file, body_rotation) != 0 ||
      BLB_ObjectFile_WriteVec3(file, body_velocity) != 0 || BLB_ObjectFile_WriteVec3(file, body_angular_velocity) != 0)
    return -1;

  if (!collider_present)
    return 0;

  BLB_Collider3D *collider = (BLB_Collider3D *)object->collider;
  const BLB_Physics3DShape *shape = BLB_Physics3D_ColliderGetShape(collider);

  if (!shape)
    return -1;

  BLB_PhysicsFilter filter = BLB_Physics3D_ColliderGetFilter(collider);
  float density = BLB_Physics3D_ColliderGetDensity(collider);
  float friction = BLB_Physics3D_ColliderGetFriction(collider);
  float restitution = BLB_Physics3D_ColliderGetRestitution(collider);
  bool trigger = BLB_Physics3D_ColliderIsTrigger(collider);
  bool active = BLB_Physics3D_ColliderIsActive(collider);

  if (BLB_ObjectFile_WritePhysicsShape3D(file, shape) != 0 || BLB_ObjectFile_Write(file, &filter, sizeof(filter), 1) != 0 ||
      BLB_ObjectFile_Write(file, &density, sizeof(density), 1) != 0 || BLB_ObjectFile_Write(file, &friction, sizeof(friction), 1) != 0 ||
      BLB_ObjectFile_Write(file, &restitution, sizeof(restitution), 1) != 0 || BLB_ObjectFile_WriteBool(file, trigger) != 0 ||
      BLB_ObjectFile_WriteBool(file, active) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_ReadPhysics3D(FILE *file, BLB_Object3D *object, BLB_Scene *scene) {
  bool body_present;
  bool collider_present;
  uint32_t body_type;
  bool body_active;
  HMM_Vec3 body_position;
  HMM_Quat body_rotation;
  HMM_Vec3 body_velocity;
  HMM_Vec3 body_angular_velocity;
  BLB_Physics3DShape shape;
  BLB_PhysicsFilter filter;
  float density;
  float friction;
  float restitution;
  bool trigger;
  bool collider_active;

  memset(&shape, 0, sizeof(shape));

  if (BLB_ObjectFile_ReadBool(file, &body_present) != 0)
    return -1;

  if (!body_present)
    return 0;

  if (BLB_ObjectFile_ReadBool(file, &collider_present) != 0 || BLB_ObjectFile_Read(file, &body_type, sizeof(body_type), 1) != 0 ||
      BLB_ObjectFile_ReadBool(file, &body_active) != 0 || BLB_ObjectFile_ReadVec3(file, &body_position) != 0 ||
      BLB_ObjectFile_ReadQuat(file, &body_rotation) != 0 || BLB_ObjectFile_ReadVec3(file, &body_velocity) != 0 ||
      BLB_ObjectFile_ReadVec3(file, &body_angular_velocity) != 0)
    return -1;

  if (body_type > BLB_PHYSICS3D_KINEMATIC)
    return -1;

  if (collider_present) {
    if (BLB_ObjectFile_ReadPhysicsShape3D(file, &shape) != 0)
      return -1;

    if (BLB_ObjectFile_Read(file, &filter, sizeof(filter), 1) != 0 || BLB_ObjectFile_Read(file, &density, sizeof(density), 1) != 0 ||
        BLB_ObjectFile_Read(file, &friction, sizeof(friction), 1) != 0 || BLB_ObjectFile_Read(file, &restitution, sizeof(restitution), 1) != 0 ||
        BLB_ObjectFile_ReadBool(file, &trigger) != 0 || BLB_ObjectFile_ReadBool(file, &collider_active) != 0) {
      BLB_ObjectFile_DestroyPhysicsShape3D(&shape);
      return -1;
    }
  }

  if (!scene) {
    BLB_ObjectFile_DestroyPhysicsShape3D(&shape);
    BLB_ObjectFile_Log("LOAD", "physics", 0);
    return 0;
  }

  if (!scene->physics_world3d) {
    BLB_ObjectFile_DestroyPhysicsShape3D(&shape);
    BLB_ObjectFile_Error("scene has no 3D physics world", NULL);
    return -1;
  }

  BLB_RigidBody3D *body = BLB_Physics3D_BodyCreate(scene->physics_world3d, (BLB_Physics3DBodyType)body_type);

  if (!body) {
    BLB_ObjectFile_DestroyPhysicsShape3D(&shape);
    return -1;
  }

  BLB_Physics3D_BodySetPosition(body, body_position);
  BLB_Physics3D_BodySetRotation(body, body_rotation);
  BLB_Physics3D_BodySetVelocity(body, body_velocity);
  BLB_Physics3D_BodySetAngularVelocity(body, body_angular_velocity);
  BLB_Physics3D_BodySetActive(body, body_active);

  object->rigid_body = (BLB_RigidBody *)body;

  if (!collider_present) {
    BLB_ObjectFile_DestroyPhysicsShape3D(&shape);
    return 0;
  }

  BLB_Collider3D *collider = BLB_Physics3D_ColliderCreate(body, &shape);

  BLB_ObjectFile_DestroyPhysicsShape3D(&shape);

  if (!collider) {
    BLB_Physics3D_BodyDestroy(scene->physics_world3d, body);
    object->rigid_body = NULL;
    return -1;
  }

  BLB_Physics3D_ColliderSetFilter(collider, filter);
  BLB_Physics3D_ColliderSetDensity(collider, density);
  BLB_Physics3D_ColliderSetFriction(collider, friction);
  BLB_Physics3D_ColliderSetRestitution(collider, restitution);
  BLB_Physics3D_ColliderSetTrigger(collider, trigger);
  BLB_Physics3D_ColliderSetActive(collider, collider_active);

  object->collider = (BLB_Collider *)collider;

  return 0;
}

static int BLB_ObjectFile_Write3DData(FILE *file, const BLB_Object3D *object) {
  uint64_t entity_id = (uint64_t)object->entity_id;
  uint64_t component_mask = (uint64_t)object->component_mask;
  uint32_t type = (uint32_t)object->type;
  uint32_t render_mode = (uint32_t)object->render_mode;

  if (BLB_ObjectFile_WriteU64(file, entity_id) != 0 || BLB_ObjectFile_WriteU64(file, component_mask) != 0 ||
      BLB_ObjectFile_Write(file, &type, sizeof(type), 1) != 0 || BLB_ObjectFile_WriteU64(file, object->geometry_id) != 0 ||
      BLB_ObjectFile_WriteVec3(file, object->position) != 0 || BLB_ObjectFile_WriteVec3(file, object->rotation) != 0 ||
      BLB_ObjectFile_WriteVec3(file, object->scale) != 0 || BLB_ObjectFile_Write(file, &render_mode, sizeof(render_mode), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->color, sizeof(object->color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->glow, sizeof(object->glow), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->emission, sizeof(object->emission), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->roundness, sizeof(object->roundness), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->layer, sizeof(object->layer), 1) != 0 || BLB_ObjectFile_WriteBool(file, object->visible) != 0 ||
      BLB_ObjectFile_WriteBool(file, object->camera_attached) != 0 || BLB_ObjectFile_WriteBool(file, object->delta_time != NULL) != 0)
    return -1;

  if (object->delta_time && BLB_ObjectFile_Write(file, object->delta_time, sizeof(*object->delta_time), 1) != 0)
    return -1;

  if (BLB_ObjectFile_WriteObjectID(file, object->id) != 0)
    return -1;

  if (BLB_ObjectFile_WriteMaterial(file, object->material) != 0)
    return -1;

  if (BLB_ObjectFile_WriteTexture(file, object->texture) != 0)
    return -1;

  if (BLB_ObjectFile_WritePolygon3D(file, object->polygon) != 0)
    return -1;

  if (BLB_ObjectFile_WriteMesh(file, &object->mesh) != 0)
    return -1;

  if (BLB_ObjectFile_WritePhysics3D(file, object) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_Read3DData(FILE *file, BLB_Object3D *object, BLB_Scene *scene, uint32_t version) {
  uint64_t entity_id;
  uint64_t component_mask;
  uint32_t type;
  uint32_t render_mode;
  bool delta_time_present;

  if (BLB_ObjectFile_ReadU64(file, &entity_id) != 0 || BLB_ObjectFile_ReadU64(file, &component_mask) != 0 ||
      BLB_ObjectFile_Read(file, &type, sizeof(type), 1) != 0 || BLB_ObjectFile_ReadU64(file, &object->geometry_id) != 0 ||
      BLB_ObjectFile_ReadVec3(file, &object->position) != 0 || BLB_ObjectFile_ReadVec3(file, &object->rotation) != 0 ||
      BLB_ObjectFile_ReadVec3(file, &object->scale) != 0 || BLB_ObjectFile_Read(file, &render_mode, sizeof(render_mode), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->color, sizeof(object->color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->glow, sizeof(object->glow), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->emission, sizeof(object->emission), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->roundness, sizeof(object->roundness), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->layer, sizeof(object->layer), 1) != 0 || BLB_ObjectFile_ReadBool(file, &object->visible) != 0)
    return -1;

  if (version >= 2 && BLB_ObjectFile_ReadBool(file, &object->camera_attached) != 0)
    return -1;
  if (BLB_ObjectFile_ReadBool(file, &delta_time_present) != 0)
    return -1;

  if (type > BLB_OBJECT_POLYGON3D)
    return -1;

  object->entity_id = (BLB_EntityId)entity_id;
  object->component_mask = (BLB_ComponentMask)component_mask;
  object->type = (BLB_ObjectType)type;
  object->render_mode = (BLB_RenderMode)render_mode;

  if (delta_time_present) {
    object->delta_time = malloc(sizeof(*object->delta_time));

    if (!object->delta_time)
      return -1;

    if (BLB_ObjectFile_Read(file, object->delta_time, sizeof(*object->delta_time), 1) != 0)
      return -1;
  }

  if (BLB_ObjectFile_ReadObjectID(file, &object->id) != 0)
    return -1;

  if (BLB_ObjectFile_ReadMaterial(file, &object->material) != 0)
    return -1;

  if (BLB_ObjectFile_ReadTexture(file, &object->texture) != 0)
    return -1;

  if (BLB_ObjectFile_ReadPolygon3D(file, &object->polygon) != 0)
    return -1;

  if (BLB_ObjectFile_ReadMesh(file, &object->mesh) != 0)
    return -1;

  if (BLB_ObjectFile_ReadPhysics3D(file, object, scene) != 0)
    return -1;

  return 0;
}

int BLB_Object3D_Save(const BLB_Object3D *object, const char *path) {
  if (!object || !path) {
    BLB_ObjectFile_Error("invalid arguments to BLB_Object3D_Save", path);
    return -1;
  }

  if (BLB_ObjectFile_CheckExtension(path, ".b3o") != 0) {
    BLB_ObjectFile_Error("invalid 3D object extension", path);
    return -1;
  }

  size_t temp_length = strlen(path) + 5;
  char *temp_path = malloc(temp_length);

  if (!temp_path)
    return -1;

  snprintf(temp_path, temp_length, "%s.tmp", path);

  BLB_ObjectFile_Log("SAVE", "open", 0);

  FILE *file = fopen(temp_path, "wb");

  if (!file) {
    BLB_ObjectFile_Error(strerror(errno), temp_path);
    free(temp_path);
    return -1;
  }

  int result = BLB_ObjectFile_WriteHeader(file, BLB_OBJECT_FILE_3D);
  BLB_ObjectFile_Log("SAVE", "header", result);

  if (result == 0) {
    result = BLB_ObjectFile_WriteU64(file, (uint64_t)object->entity_id);
    result |= BLB_ObjectFile_WriteU64(file, (uint64_t)object->component_mask);
    uint32_t type = (uint32_t)object->type;
    uint32_t render_mode = (uint32_t)object->render_mode;

    result |= BLB_ObjectFile_Write(file, &type, sizeof(type), 1);
    result |= BLB_ObjectFile_WriteU64(file, object->geometry_id);
    result |= BLB_ObjectFile_WriteVec3(file, object->position);
    result |= BLB_ObjectFile_WriteVec3(file, object->rotation);
    result |= BLB_ObjectFile_WriteVec3(file, object->scale);
    result |= BLB_ObjectFile_Write(file, &render_mode, sizeof(render_mode), 1);
    result |= BLB_ObjectFile_Write(file, &object->color, sizeof(object->color), 1);
    result |= BLB_ObjectFile_Write(file, &object->glow, sizeof(object->glow), 1);
    result |= BLB_ObjectFile_Write(file, &object->emission, sizeof(object->emission), 1);
    result |= BLB_ObjectFile_Write(file, &object->roundness, sizeof(object->roundness), 1);
    result |= BLB_ObjectFile_Write(file, &object->layer, sizeof(object->layer), 1);
    result |= BLB_ObjectFile_WriteBool(file, object->visible);
    result |= BLB_ObjectFile_WriteBool(file, object->camera_attached);
    result |= BLB_ObjectFile_WriteBool(file, object->delta_time != NULL);

    BLB_ObjectFile_Log("SAVE", "core", result);
  }

  if (result == 0 && object->delta_time) {
    result = BLB_ObjectFile_Write(file, object->delta_time, sizeof(*object->delta_time), 1);
    BLB_ObjectFile_Log("SAVE", "delta_time", result);
  }

  if (result == 0) {
    result = BLB_ObjectFile_WriteObjectID(file, object->id);
    BLB_ObjectFile_Log("SAVE", "object_id", result);
  }

  if (result == 0) {
    result = BLB_ObjectFile_WriteMaterial(file, object->material);
    BLB_ObjectFile_Log("SAVE", "material", result);
  }

  if (result == 0) {
    result = BLB_ObjectFile_WriteTexture(file, object->texture);
    BLB_ObjectFile_Log("SAVE", "texture", result);
  }

  if (result == 0) {
    result = BLB_ObjectFile_WritePolygon3D(file, object->polygon);
    BLB_ObjectFile_Log("SAVE", "polygon", result);
  }

  if (result == 0) {
    result = BLB_ObjectFile_WriteMesh(file, &object->mesh);
    BLB_ObjectFile_Log("SAVE", "mesh", result);
  }

  if (result == 0) {
    result = BLB_ObjectFile_WritePhysics3D(file, object);
    BLB_ObjectFile_Log("SAVE", "physics", result);
  }

  if (fclose(file) != 0)
    result = -1;

  if (result != 0) {
    remove(temp_path);
    BLB_ObjectFile_Error("failed to save 3D object", path);
    free(temp_path);
    return -1;
  }

  if (rename(temp_path, path) != 0) {
    BLB_ObjectFile_Error(strerror(errno), path);
    remove(temp_path);
    free(temp_path);
    return -1;
  }

  free(temp_path);

  BLB_ObjectFile_Log("SAVE", "complete", 0);
  return 0;
}

BLB_Object3D *BLB_Object3D_Load(const char *path, BLB_Scene *scene) {
  if (!path)
    return NULL;

  if (BLB_ObjectFile_CheckExtension(path, ".b3o") != 0) {
    BLB_ObjectFile_Error("invalid 3D object extension", path);
    return NULL;
  }

  FILE *file = fopen(path, "rb");

  if (!file) {
    BLB_ObjectFile_Error(strerror(errno), path);
    return NULL;
  }

  uint32_t object_file_version = 0;
  int result = BLB_ObjectFile_ReadHeader(file, BLB_OBJECT_FILE_3D, &object_file_version);
  BLB_ObjectFile_Log("LOAD", "header", result);

  if (result != 0) {
    if (result == -2)
      BLB_ObjectFile_Error("invalid 3D object magic", path);
    else if (result == -3)
      BLB_ObjectFile_Error("unsupported 3D object version", path);
    else if (result == -4)
      BLB_ObjectFile_Error("3D object type mismatch", path);
    else
      BLB_ObjectFile_Error("failed to read 3D object header", path);

    fclose(file);
    return NULL;
  }

  BLB_Object3D *object = calloc(1, sizeof(*object));

  if (!object) {
    fclose(file);
    return NULL;
  }

  result = BLB_ObjectFile_Read3DData(file, object, scene, object_file_version);
  BLB_ObjectFile_Log("LOAD", "data", result);

  if (result == 0 && fgetc(file) != EOF) {
    BLB_ObjectFile_Error("unexpected trailing data", path);
    result = -1;
  }

  fclose(file);

  if (result != 0) {
    if (object->rigid_body && scene && scene->physics_world3d)
      BLB_Physics3D_BodyDestroy(scene->physics_world3d, (BLB_RigidBody3D *)object->rigid_body);

    if (object->material)
      BLB_Material_Release(object->material);

    if (object->texture)
      BLB_Texture_Release(object->texture);

    if (object->polygon)
      BLB_DestroyPolygon3D(object->polygon);

    BLB_ObjectFile_DestroyMesh(&object->mesh);

    if (object->id && !BLB_ObjectFile_IsGlobalObjectID(object->id))
      free(object->id);

    free(object->delta_time);
    free(object);

    BLB_ObjectFile_Error("failed to load 3D object", path);
    return NULL;
  }

  BLB_ObjectFile_Log("LOAD", "complete", 0);
  return object;
}

static int BLB_ObjectFile_Write2DData(FILE *file, const BLB_Object2D *object) {
  uint64_t entity_id = (uint64_t)object->entity_id;
  uint64_t component_mask = (uint64_t)object->component_mask;
  uint32_t type = (uint32_t)object->type;
  uint32_t render_mode = (uint32_t)object->render_mode;

  if (BLB_ObjectFile_WriteU64(file, entity_id) != 0 || BLB_ObjectFile_WriteU64(file, component_mask) != 0 ||
      BLB_ObjectFile_Write(file, &type, sizeof(type), 1) != 0 || BLB_ObjectFile_WriteVec2(file, object->position) != 0 ||
      BLB_ObjectFile_Write(file, &object->rotation, sizeof(object->rotation), 1) != 0 || BLB_ObjectFile_WriteVec2(file, object->scale) != 0 ||
      BLB_ObjectFile_Write(file, &render_mode, sizeof(render_mode), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->color, sizeof(object->color), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->glow, sizeof(object->glow), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->emission, sizeof(object->emission), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->roundness, sizeof(object->roundness), 1) != 0 ||
      BLB_ObjectFile_Write(file, &object->layer, sizeof(object->layer), 1) != 0 || BLB_ObjectFile_WriteBool(file, object->visible) != 0 ||
      BLB_ObjectFile_WriteBool(file, object->camera_attached) != 0 || BLB_ObjectFile_WriteBool(file, object->delta_time != NULL) != 0)
    return -1;

  if (object->delta_time && BLB_ObjectFile_Write(file, object->delta_time, sizeof(*object->delta_time), 1) != 0)
    return -1;

  if (BLB_ObjectFile_WriteObjectID(file, object->id) != 0 || BLB_ObjectFile_WriteMaterial(file, object->material) != 0 ||
      BLB_ObjectFile_WriteTexture(file, object->texture) != 0 || BLB_ObjectFile_WriteAnimation(file, object->animation) != 0 ||
      BLB_ObjectFile_WritePolygon2D(file, object->polygon) != 0)
    return -1;

  return 0;
}

static int BLB_ObjectFile_Read2DData(FILE *file, BLB_Object2D *object, uint32_t version) {
  uint64_t entity_id;
  uint64_t component_mask;
  uint32_t type;
  uint32_t render_mode;
  bool delta_time_present;

  if (BLB_ObjectFile_ReadU64(file, &entity_id) != 0 || BLB_ObjectFile_ReadU64(file, &component_mask) != 0 ||
      BLB_ObjectFile_Read(file, &type, sizeof(type), 1) != 0 || BLB_ObjectFile_ReadVec2(file, &object->position) != 0 ||
      BLB_ObjectFile_Read(file, &object->rotation, sizeof(object->rotation), 1) != 0 || BLB_ObjectFile_ReadVec2(file, &object->scale) != 0 ||
      BLB_ObjectFile_Read(file, &render_mode, sizeof(render_mode), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->color, sizeof(object->color), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->glow, sizeof(object->glow), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->emission, sizeof(object->emission), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->roundness, sizeof(object->roundness), 1) != 0 ||
      BLB_ObjectFile_Read(file, &object->layer, sizeof(object->layer), 1) != 0 || BLB_ObjectFile_ReadBool(file, &object->visible) != 0)
    return -1;

  if (version >= 3) {
    if (BLB_ObjectFile_ReadBool(file, &object->camera_attached) != 0)
      return -1;
  } else {
    bool legacy_screen_space = false;
    if (BLB_ObjectFile_ReadBool(file, &legacy_screen_space) != 0)
      return -1;
    if (version >= 2 && BLB_ObjectFile_ReadBool(file, &object->camera_attached) != 0)
      return -1;
    object->camera_attached = object->camera_attached || legacy_screen_space;
  }

  if (BLB_ObjectFile_ReadBool(file, &delta_time_present) != 0)
    return -1;

  if (type < BLB_OBJECT_SQUARE || type > BLB_OBJECT_TEXT)
    return -1;

  object->entity_id = (BLB_EntityId)entity_id;
  object->component_mask = (BLB_ComponentMask)component_mask;
  object->type = (BLB_ObjectType)type;
  object->render_mode = (BLB_RenderMode)render_mode;

  if (delta_time_present) {
    object->delta_time = malloc(sizeof(*object->delta_time));

    if (!object->delta_time)
      return -1;

    if (BLB_ObjectFile_Read(file, object->delta_time, sizeof(*object->delta_time), 1) != 0)
      return -1;
  }

  if (BLB_ObjectFile_ReadObjectID(file, &object->id) != 0 || BLB_ObjectFile_ReadMaterial(file, &object->material) != 0 ||
      BLB_ObjectFile_ReadTexture(file, &object->texture) != 0 || BLB_ObjectFile_ReadAnimation(file, &object->animation) != 0 ||
      BLB_ObjectFile_ReadPolygon2D(file, &object->polygon) != 0)
    return -1;

  return 0;
}

int BLB_Object2D_Save(const BLB_Object2D *object, const char *path) {
  if (!object || !path) {
    BLB_ObjectFile_Error("invalid arguments to BLB_Object2D_Save", path);
    return -1;
  }

  if (BLB_ObjectFile_CheckExtension(path, ".b2o") != 0) {
    BLB_ObjectFile_Error("invalid 2D object extension", path);
    return -1;
  }

  size_t temp_length = strlen(path) + 5;
  char *temp_path = malloc(temp_length);

  if (!temp_path)
    return -1;

  snprintf(temp_path, temp_length, "%s.tmp", path);

  FILE *file = fopen(temp_path, "wb");

  if (!file) {
    BLB_ObjectFile_Error(strerror(errno), temp_path);
    free(temp_path);
    return -1;
  }

  int result = BLB_ObjectFile_WriteHeader(file, BLB_OBJECT_FILE_2D);
  BLB_ObjectFile_Log("SAVE", "header", result);

  if (result == 0) {
    result = BLB_ObjectFile_Write2DData(file, object);
    BLB_ObjectFile_Log("SAVE", "2d_data", result);
  }

  if (fclose(file) != 0)
    result = -1;

  if (result != 0) {
    remove(temp_path);
    free(temp_path);
    BLB_ObjectFile_Error("failed to save 2D object", path);
    return -1;
  }

  if (rename(temp_path, path) != 0) {
    BLB_ObjectFile_Error(strerror(errno), path);
    remove(temp_path);
    free(temp_path);
    return -1;
  }

  free(temp_path);

  BLB_ObjectFile_Log("SAVE", "complete", 0);
  return 0;
}

BLB_Object2D *BLB_Object2D_Load(const char *path, BLB_Scene *scene) {
  (void)scene;

  if (!path)
    return NULL;

  if (BLB_ObjectFile_CheckExtension(path, ".b2o") != 0) {
    BLB_ObjectFile_Error("invalid 2D object extension", path);
    return NULL;
  }

  FILE *file = fopen(path, "rb");

  if (!file) {
    BLB_ObjectFile_Error(strerror(errno), path);
    return NULL;
  }

  uint32_t object_file_version = 0;
  int result = BLB_ObjectFile_ReadHeader(file, BLB_OBJECT_FILE_2D, &object_file_version);
  BLB_ObjectFile_Log("LOAD", "header", result);

  if (result != 0) {
    if (result == -2)
      BLB_ObjectFile_Error("invalid 2D object magic", path);
    else if (result == -3)
      BLB_ObjectFile_Error("unsupported 2D object version", path);
    else if (result == -4)
      BLB_ObjectFile_Error("2D object type mismatch", path);
    else
      BLB_ObjectFile_Error("failed to read 2D object header", path);

    fclose(file);
    return NULL;
  }

  BLB_Object2D *object = calloc(1, sizeof(*object));

  if (!object) {
    fclose(file);
    return NULL;
  }

  result = BLB_ObjectFile_Read2DData(file, object, object_file_version);
  BLB_ObjectFile_Log("LOAD", "2d_data", result);

  if (result == 0 && fgetc(file) != EOF) {
    BLB_ObjectFile_Error("unexpected trailing data", path);
    result = -1;
  }

  fclose(file);

  if (result != 0) {
    if (object->material)
      BLB_Material_Release(object->material);

    if (object->texture)
      BLB_Texture_Release(object->texture);

    if (object->animation) {
      for (size_t i = 0; i < object->animation->textures_count; ++i) {
        if (object->animation->textures && object->animation->textures[i])
          BLB_Texture_Release(object->animation->textures[i]);
      }

      free(object->animation->textures);
      free(object->animation);
    }

    if (object->polygon)
      BLB_DestroyPolygon2D(object->polygon);

    if (object->id && !BLB_ObjectFile_IsGlobalObjectID(object->id))
      free(object->id);

    free(object->delta_time);
    free(object);

    BLB_ObjectFile_Error("failed to load 2D object", path);
    return NULL;
  }

  BLB_ObjectFile_Log("LOAD", "complete", 0);
  return object;
}
