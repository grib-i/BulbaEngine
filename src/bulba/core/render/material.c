#include "bulba/core/render/material.h"
#include "bulba/core/render/texture.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLB_MATERIAL_TEXTURE_SLOT_COUNT 17

typedef struct {
  BLB_Material *material;
  char *path;
} BLB_MaterialCacheEntry;

static BLB_MaterialCacheEntry *material_cache = NULL;
static size_t material_cache_count = 0;
static size_t material_cache_capacity = 0;

static char *material_strdup(const char *text) {
  if (!text) return NULL;
  size_t n = strlen(text) + 1u;
  char *copy = malloc(n);
  if (!copy) return NULL;
  memcpy(copy, text, n);
  return copy;
}

static BLB_Material *material_cache_find(const char *path) {
  if (!path) return NULL;
  for (size_t i = 0; i < material_cache_count; ++i) {
    if (material_cache[i].material && strcmp(material_cache[i].path, path) == 0)
      return material_cache[i].material;
  }
  return NULL;
}

static void material_cache_insert(BLB_Material *material, const char *path) {
  if (!material || !path) return;
  if (material_cache_count == material_cache_capacity) {
    size_t new_capacity = material_cache_capacity ? material_cache_capacity * 2u : 16u;
    BLB_MaterialCacheEntry *next = realloc(material_cache, new_capacity * sizeof(*next));
    if (!next) return;
    material_cache = next;
    material_cache_capacity = new_capacity;
  }
  char *copy = material_strdup(path);
  if (!copy) return;
  material_cache[material_cache_count++] = (BLB_MaterialCacheEntry){.material = material, .path = copy};
}

static void material_cache_remove(BLB_Material *material) {
  if (!material) return;
  for (size_t i = 0; i < material_cache_count; ++i) {
    if (material_cache[i].material != material) continue;
    free(material_cache[i].path);
    material_cache[i] = material_cache[material_cache_count - 1u];
    --material_cache_count;
    return;
  }
}

static BLB_RenderMode normalize_mode(BLB_RenderMode mode) {
  if ((int)mode < (int)BLB_RENDER_OPAQUE || (int)mode >= (int)BLB_RENDER_MODE_COUNT)
    return BLB_RENDER_OPAQUE;

  return mode;
}

static BLB_AlphaMode normalize_alpha_mode(BLB_AlphaMode mode) {
  if ((int)mode < (int)BLB_ALPHA_OPAQUE || (int)mode > (int)BLB_ALPHA_BLEND)
    return BLB_ALPHA_OPAQUE;

  return mode;
}

static float clamp01(float value) {
  if (value < 0.0f)
    return 0.0f;

  if (value > 1.0f)
    return 1.0f;

  return value;
}

static float non_negative(float value) { return value < 0.0f ? 0.0f : value; }

static uint64_t texture_state_revision = 1;

static void texture_state_touch(void) {
  texture_state_revision++;
  if (texture_state_revision == 0)
    texture_state_revision = 1;
}

static void material_touch(BLB_Material *material) {
  if (!material)
    return;

  material->revision++;
  if (material->revision == 0)
    material->revision = 1;
}

static void copy_string(char *dst, size_t size, const char *src) {
  if (!dst || size == 0)
    return;

  memset(dst, 0, size);

  if (src)
    strncpy(dst, src, size - 1);
}

static void release_material_texture(BLB_Material *material, BLB_MaterialTexture *texture) {
  if (!material || !texture || !texture->texture)
    return;

  if (texture->owned && material->texture_release)
    material->texture_release(texture->texture, material->texture_user_data);

  texture->texture = NULL;
  texture->owned = false;
}

static void init_texture(BLB_MaterialTexture *texture) {
  if (!texture)
    return;

  memset(texture, 0, sizeof(*texture));

  texture->scale[0] = 1.0f;
  texture->scale[1] = 1.0f;
  texture->wrap_u = BLB_TEXTURE_WRAP_REPEAT;
  texture->wrap_v = BLB_TEXTURE_WRAP_REPEAT;
  texture->min_filter = BLB_TEXTURE_FILTER_LINEAR;
  texture->mag_filter = BLB_TEXTURE_FILTER_LINEAR;
}

BLB_Material *BLB_Material_Create(BLB_MaterialDomain domain) {
  BLB_Material *material = calloc(1, sizeof(*material));

  if (!material)
    return NULL;

  material->ref_count = 1;
  material->domain = domain;
  material->render_mode = BLB_RENDER_OPAQUE;
  material->alpha_mode = BLB_ALPHA_OPAQUE;
  material->lighting_enabled = true;
  material->depth_enabled = domain == BLB_MATERIAL_3D;
  material->depth_write = domain == BLB_MATERIAL_3D;
  material->double_sided = domain == BLB_MATERIAL_2D;
  material->unlit = false;

  material->base_color[0] = 1.0f;
  material->base_color[1] = 1.0f;
  material->base_color[2] = 1.0f;
  material->base_color[3] = 1.0f;

  material->metallic = 0.0f;
  material->roughness = 0.65f;
  material->normal_scale = 1.0f;
  material->occlusion_strength = 1.0f;

  material->emission_color[0] = 1.0f;
  material->emission_color[1] = 1.0f;
  material->emission_color[2] = 1.0f;
  material->emission_color[3] = 1.0f;

  material->emission_strength = 0.0f;
  material->alpha_cutoff = 0.5f;

  material->specular_factor = 1.0f;
  material->specular_color[0] = 1.0f;
  material->specular_color[1] = 1.0f;
  material->specular_color[2] = 1.0f;

  material->ior = 1.5f;
  material->transmission = 0.0f;

  material->volume_thickness = 0.0f;
  material->attenuation_color[0] = 1.0f;
  material->attenuation_color[1] = 1.0f;
  material->attenuation_color[2] = 1.0f;
  material->attenuation_distance = INFINITY;

  material->clearcoat_factor = 0.0f;
  material->clearcoat_roughness = 0.0f;
  material->clearcoat_normal_scale = 1.0f;

  material->sheen_color[0] = 0.0f;
  material->sheen_color[1] = 0.0f;
  material->sheen_color[2] = 0.0f;
  material->sheen_roughness = 0.0f;

  material->iridescence_factor = 0.0f;
  material->iridescence_ior = 1.3f;
  material->iridescence_thickness_min = 100.0f;
  material->iridescence_thickness_max = 400.0f;

  material->anisotropy_strength = 0.0f;
  material->anisotropy_rotation = 0.0f;
  material->dispersion = 0.0f;

  material->glow_strength = 0.0f;
  material->glow_radius = 0.0f;
  material->glow_falloff = 2.0f;
  material->temperature = 6500.0f;
  material->revision = 1;

  init_texture(&material->base_color_texture);
  init_texture(&material->metallic_roughness_texture);
  init_texture(&material->normal_texture);
  init_texture(&material->occlusion_texture);
  init_texture(&material->emission_texture);
  init_texture(&material->specular_texture);
  init_texture(&material->specular_color_texture);
  init_texture(&material->clearcoat_texture);
  init_texture(&material->clearcoat_roughness_texture);
  init_texture(&material->clearcoat_normal_texture);
  init_texture(&material->transmission_texture);
  init_texture(&material->thickness_texture);
  init_texture(&material->sheen_color_texture);
  init_texture(&material->sheen_roughness_texture);
  init_texture(&material->iridescence_texture);
  init_texture(&material->iridescence_thickness_texture);
  init_texture(&material->anisotropy_texture);

  return material;
}

BLB_Material *BLB_Material_Create2D(void) { return BLB_Material_Create(BLB_MATERIAL_2D); }

BLB_Material *BLB_Material_Create3D(void) { return BLB_Material_Create(BLB_MATERIAL_3D); }

BLB_Material *BLB_Material_Build(BLB_MaterialDomain domain, BLB_MaterialConfigureFn configure) {
  BLB_Material *material = BLB_Material_Create(domain);
  if (!material)
    return NULL;

  if (configure)
    configure(material);

  return material;
}

void BLB_Material_Retain(BLB_Material *material) {
  if (material)
    material->ref_count++;
}

void BLB_Material_Release(BLB_Material *material) {
  if (!material)
    return;

  if (material->ref_count > 1) {
    material->ref_count--;
    return;
  }

  material_cache_remove(material);

  if (material->backend_destroy && material->backend_data)
    material->backend_destroy(material->backend_data);
  material->backend_destroy = NULL;
  material->backend_data = NULL;

  release_material_texture(material, &material->base_color_texture);
  release_material_texture(material, &material->metallic_roughness_texture);
  release_material_texture(material, &material->normal_texture);
  release_material_texture(material, &material->occlusion_texture);
  release_material_texture(material, &material->emission_texture);
  release_material_texture(material, &material->specular_texture);
  release_material_texture(material, &material->specular_color_texture);
  release_material_texture(material, &material->clearcoat_texture);
  release_material_texture(material, &material->clearcoat_roughness_texture);
  release_material_texture(material, &material->clearcoat_normal_texture);
  release_material_texture(material, &material->transmission_texture);
  release_material_texture(material, &material->thickness_texture);
  release_material_texture(material, &material->sheen_color_texture);
  release_material_texture(material, &material->sheen_roughness_texture);
  release_material_texture(material, &material->iridescence_texture);
  release_material_texture(material, &material->iridescence_thickness_texture);
  release_material_texture(material, &material->anisotropy_texture);

  if (material->shader_program) {
    BLB_Shader_Release(material->shader_program);
    material->shader_program = NULL;
  }

  free(material);
}

BLB_Material *BLB_Material_Clone(const BLB_Material *material) {
  if (!material)
    return NULL;

  BLB_Material *copy = malloc(sizeof(*copy));

  if (!copy)
    return NULL;

  *copy = *material;
  copy->ref_count = 1;
  copy->user_data = NULL;
  copy->revision = 1;
  copy->backend_data = NULL;
  copy->backend_destroy = NULL;

  if (copy->shader_program)
    BLB_Shader_Retain(copy->shader_program);

  BLB_MaterialTexture *textures[] = {&copy->base_color_texture,
                                     &copy->metallic_roughness_texture,
                                     &copy->normal_texture,
                                     &copy->occlusion_texture,
                                     &copy->emission_texture,
                                     &copy->specular_texture,
                                     &copy->specular_color_texture,
                                     &copy->clearcoat_texture,
                                     &copy->clearcoat_roughness_texture,
                                     &copy->clearcoat_normal_texture,
                                     &copy->transmission_texture,
                                     &copy->thickness_texture,
                                     &copy->sheen_color_texture,
                                     &copy->sheen_roughness_texture,
                                     &copy->iridescence_texture,
                                     &copy->iridescence_thickness_texture,
                                     &copy->anisotropy_texture};

  for (size_t i = 0; i < sizeof(textures) / sizeof(textures[0]); i++) {
    if (!textures[i]->texture || !textures[i]->owned)
      continue;

    if (copy->texture_retain)
      copy->texture_retain(textures[i]->texture, copy->texture_user_data);
    else
      textures[i]->owned = false;
  }

  return copy;
}

void BLB_Material_SetName(BLB_Material *material, const char *name) {
  if (!material)
    return;

  copy_string(material->name, sizeof(material->name), name);
  material_touch(material);
}

void BLB_Material_SetDomain(BLB_Material *material, BLB_MaterialDomain domain) {
  if (material)
    material->domain = domain;
  material_touch(material);
}

void BLB_Material_SetBaseColor(BLB_Material *material, float r, float g, float b, float a) {
  if (!material)
    return;

  material->base_color[0] = r;
  material->base_color[1] = g;
  material->base_color[2] = b;
  material->base_color[3] = a;
  material_touch(material);
}

void BLB_Material_SetEmission(BLB_Material *material, float r, float g, float b, float a, float strength) {
  if (!material)
    return;

  material->emission_color[0] = r;
  material->emission_color[1] = g;
  material->emission_color[2] = b;
  material->emission_color[3] = a;
  material->emission_strength = non_negative(strength);
  material_touch(material);
}

void BLB_Material_SetAlphaCutoff(BLB_Material *material, float cutoff) {
  if (material) {
    material->alpha_cutoff = clamp01(cutoff);
    material_touch(material);
  }
}

void BLB_Material_SetPBR(BLB_Material *material, float metallic, float roughness) {
  if (!material)
    return;

  material->metallic = clamp01(metallic);
  material->roughness = clamp01(roughness);
  material_touch(material);
}

void BLB_Material_SetNormal(BLB_Material *material, float scale) {
  if (material) {
    material->normal_scale = non_negative(scale);
    material_touch(material);
  }
}

void BLB_Material_SetOcclusion(BLB_Material *material, float strength) {
  if (material) {
    material->occlusion_strength = clamp01(strength);
    material_touch(material);
  }
}

void BLB_Material_SetSpecular(BLB_Material *material, float factor, float r, float g, float b) {
  if (!material)
    return;

  material->specular_factor = clamp01(factor);
  material->specular_color[0] = r;
  material->specular_color[1] = g;
  material->specular_color[2] = b;
  material_touch(material);
}

void BLB_Material_SetIOR(BLB_Material *material, float ior) {
  if (material) {
    material->ior = ior < 1.0f ? 1.0f : ior;
    material_touch(material);
  }
}

void BLB_Material_SetTransmission(BLB_Material *material, float transmission) {
  if (material) {
    material->transmission = clamp01(transmission);
    material_touch(material);
  }
}

void BLB_Material_SetVolume(BLB_Material *material, float thickness, float attenuation_distance, float r, float g, float b) {
  if (!material)
    return;

  material->volume_thickness = non_negative(thickness);
  material->attenuation_distance = attenuation_distance > 0.0f ? attenuation_distance : INFINITY;
  material->attenuation_color[0] = r;
  material->attenuation_color[1] = g;
  material->attenuation_color[2] = b;
  material_touch(material);
}

void BLB_Material_SetClearcoat(BLB_Material *material, float factor, float roughness, float normal_scale) {
  if (!material)
    return;

  material->clearcoat_factor = clamp01(factor);
  material->clearcoat_roughness = clamp01(roughness);
  material->clearcoat_normal_scale = non_negative(normal_scale);
  material_touch(material);
}

void BLB_Material_SetSheen(BLB_Material *material, float r, float g, float b, float roughness) {
  if (!material)
    return;

  material->sheen_color[0] = r;
  material->sheen_color[1] = g;
  material->sheen_color[2] = b;
  material->sheen_roughness = clamp01(roughness);
  material_touch(material);
}

void BLB_Material_SetIridescence(BLB_Material *material, float factor, float ior, float thickness_min, float thickness_max) {
  if (!material)
    return;

  material->iridescence_factor = clamp01(factor);
  material->iridescence_ior = ior < 1.0f ? 1.0f : ior;
  material->iridescence_thickness_min = non_negative(thickness_min);
  material->iridescence_thickness_max = thickness_max < material->iridescence_thickness_min ? material->iridescence_thickness_min : thickness_max;
  material_touch(material);
}

void BLB_Material_SetAnisotropy(BLB_Material *material, float strength, float rotation) {
  if (!material)
    return;

  material->anisotropy_strength = clamp01(strength);
  material->anisotropy_rotation = rotation;
  material_touch(material);
}

void BLB_Material_SetDispersion(BLB_Material *material, float dispersion) {
  if (material) {
    material->dispersion = non_negative(dispersion);
    material_touch(material);
  }
}

void BLB_Material_SetGlow(BLB_Material *material, float strength, float radius, float falloff) {
  if (!material)
    return;

  material->glow_strength = non_negative(strength);
  material->glow_radius = non_negative(radius);
  material->glow_falloff = falloff <= 0.0f ? 1.0f : falloff;
  material_touch(material);
}

void BLB_Material_SetTemperature(BLB_Material *material, float kelvin) {
  if (!material)
    return;

  material->temperature = kelvin > 0.0f ? kelvin : 0.0f;
  material_touch(material);
}

void BLB_Material_SetRenderMode(BLB_Material *material, BLB_RenderMode mode) {
  if (material) {
    material->render_mode = normalize_mode(mode);
    material_touch(material);
  }
}

void BLB_Material_SetAlphaMode(BLB_Material *material, BLB_AlphaMode mode) {
  if (material) {
    material->alpha_mode = normalize_alpha_mode(mode);
    material_touch(material);
  }
}

void BLB_Material_SetLighting(BLB_Material *material, bool enabled) {
  if (material) {
    material->lighting_enabled = enabled;
    material_touch(material);
  }
}

void BLB_Material_SetDepth(BLB_Material *material, bool enabled, bool write) {
  if (!material)
    return;

  material->depth_enabled = enabled;
  material->depth_write = write;
  material_touch(material);
}

void BLB_Material_SetDoubleSided(BLB_Material *material, bool enabled) {
  if (material) {
    material->double_sided = enabled;
    material_touch(material);
  }
}

void BLB_Material_SetUnlit(BLB_Material *material, bool enabled) {
  if (material) {
    material->unlit = enabled;
    material_touch(material);
  }
}

void BLB_Material_SetShader(BLB_Material *material, BLB_ShaderProgram *shader) {
  if (!material)
    return;

  if (material->shader_program == shader)
    return;

  if (shader)
    BLB_Shader_Retain(shader);

  if (material->shader_program)
    BLB_Shader_Release(material->shader_program);

  material->shader_program = shader;
  material_touch(material);
}

void BLB_MaterialTexture_Init(BLB_MaterialTexture *texture) { init_texture(texture); }

void BLB_MaterialTexture_SetTexture(BLB_Material *material, BLB_MaterialTexture *slot, BLB_Texture *texture) {
  if (!material || !slot)
    return;

  release_material_texture(material, slot);

  slot->texture = texture;
  slot->owned = false;
  material_touch(material);
}

void BLB_MaterialTexture_AdoptTexture(BLB_MaterialTexture *slot, BLB_Texture *texture) {
  if (!slot)
    return;

  slot->texture = texture;
  slot->owned = texture != NULL;
}

void BLB_MaterialTexture_SetTransform(BLB_MaterialTexture *texture, float offset_x, float offset_y, float scale_x, float scale_y, float rotation) {
  if (!texture)
    return;

  texture->offset[0] = offset_x;
  texture->offset[1] = offset_y;
  texture->scale[0] = scale_x;
  texture->scale[1] = scale_y;
  texture->rotation = rotation;
  texture_state_touch();
}

void BLB_MaterialTexture_SetSampler(BLB_MaterialTexture *texture, BLB_TextureWrap wrap_u, BLB_TextureWrap wrap_v, BLB_TextureFilter min_filter,
                                    BLB_TextureFilter mag_filter) {
  if (!texture)
    return;

  texture->wrap_u = wrap_u;
  texture->wrap_v = wrap_v;
  texture->min_filter = min_filter;
  texture->mag_filter = mag_filter;
  texture_state_touch();
}

bool BLB_MaterialTexture_Valid(const BLB_MaterialTexture *texture) { return texture && texture->texture; }

uint64_t BLB_Material_TextureStateRevision(void) { return texture_state_revision; }


typedef enum {
  BLB_MAT_SEC_NONE = 0,
  BLB_MAT_SEC_RENDER,
  BLB_MAT_SEC_BASE,
  BLB_MAT_SEC_EMISSION,
  BLB_MAT_SEC_SPECULAR,
  BLB_MAT_SEC_TRANSMISSION,
  BLB_MAT_SEC_VOLUME,
  BLB_MAT_SEC_CLEARCOAT,
  BLB_MAT_SEC_SHEEN,
  BLB_MAT_SEC_IRIDESCENCE,
  BLB_MAT_SEC_ANISOTROPY,
  BLB_MAT_SEC_GLOW,
  BLB_MAT_SEC_TEXTURES,
  BLB_MAT_SEC_SHADER
} BLB_MaterialParseSection;

static char *trim_ws(char *text) {
  if (!text)
    return text;
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
    text++;
  char *end = text + strlen(text);
  while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
    --end;
  *end = '\0';
  return text;
}

static bool parse_bool_value(const char *text, bool fallback) {
  if (!text)
    return fallback;
  if (strcmp(text, "true") == 0 || strcmp(text, "1") == 0)
    return true;
  if (strcmp(text, "false") == 0 || strcmp(text, "0") == 0)
    return false;
  return fallback;
}

static float parse_float_value(const char *text, float fallback) {
  if (!text)
    return fallback;
  if (strcmp(text, "inf") == 0 || strcmp(text, "infinity") == 0)
    return INFINITY;
  char *end = NULL;
  float value = strtof(text, &end);
  if (end == text)
    return fallback;
  return value;
}

static int parse_float_list(const char *text, float *values, size_t count) {
  if (!text || !values || count == 0)
    return 0;

  const char *p = strchr(text, '[');
  if (!p)
    p = text;
  else
    ++p;

  for (size_t i = 0; i < count; ++i) {
    while (*p == ' ' || *p == '\t' || *p == ',')
      ++p;
    char *end = NULL;
    float value = strtof(p, &end);
    if (end == p)
      return 0;
    values[i] = value;
    p = end;
  }
  return 1;
}

static void parse_string_value(const char *text, char *out, size_t out_size) {
  if (!out || out_size == 0)
    return;
  out[0] = '\0';
  if (!text)
    return;

  const char *start = strchr(text, '"');
  char quote = '"';
  if (!start) {
    start = strchr(text, '\'');
    quote = '\'';
  }

  if (start) {
    ++start;
    const char *end = strchr(start, quote);
    if (!end)
      return;
    size_t n = (size_t)(end - start);
    if (n >= out_size)
      n = out_size - 1;
    memcpy(out, start, n);
    out[n] = '\0';
    return;
  }

  strncpy(out, text, out_size - 1);
  out[out_size - 1] = '\0';
}


static BLB_RenderMode parse_render_mode(const char *text) {
  if (!text)
    return BLB_RENDER_OPAQUE;
  if (strcmp(text, "transparent") == 0)
    return BLB_RENDER_TRANSPARENT;
  if (strcmp(text, "additive") == 0)
    return BLB_RENDER_ADDITIVE;
  return BLB_RENDER_OPAQUE;
}

static BLB_AlphaMode parse_alpha_mode(const char *text) {
  if (!text)
    return BLB_ALPHA_OPAQUE;
  if (strcmp(text, "mask") == 0)
    return BLB_ALPHA_MASK;
  if (strcmp(text, "blend") == 0)
    return BLB_ALPHA_BLEND;
  return BLB_ALPHA_OPAQUE;
}

static int texture_slot_index(const char *name) {
  static const char *names[BLB_MATERIAL_TEXTURE_SLOT_COUNT] = {
      "base_color", "metallic_roughness", "normal", "occlusion", "emission", "specular", "specular_color", "clearcoat",
      "clearcoat_roughness", "clearcoat_normal", "transmission", "thickness", "sheen_color", "sheen_roughness", "iridescence",
      "iridescence_thickness", "anisotropy"};

  for (int i = 0; i < BLB_MATERIAL_TEXTURE_SLOT_COUNT; ++i)
    if (strcmp(name, names[i]) == 0)
      return i;
  return -1;
}

static BLB_MaterialTexture *material_texture_slot(BLB_Material *material, int index) {
  if (!material || index < 0 || index >= BLB_MATERIAL_TEXTURE_SLOT_COUNT)
    return NULL;
  BLB_MaterialTexture *slots[] = {
      &material->base_color_texture, &material->metallic_roughness_texture, &material->normal_texture, &material->occlusion_texture,
      &material->emission_texture, &material->specular_texture, &material->specular_color_texture, &material->clearcoat_texture,
      &material->clearcoat_roughness_texture, &material->clearcoat_normal_texture, &material->transmission_texture, &material->thickness_texture,
      &material->sheen_color_texture, &material->sheen_roughness_texture, &material->iridescence_texture, &material->iridescence_thickness_texture,
      &material->anisotropy_texture};
  return slots[index];
}

static char *material_dir(const char *path) {
  if (!path)
    return NULL;
  const char *slash = strrchr(path, '/');
  if (!slash) {
    char *dir = malloc(2);
    if (dir) {
      dir[0] = '.';
      dir[1] = '\0';
    }
    return dir;
  }
  size_t length = (size_t)(slash - path);
  char *dir = malloc(length + 1);
  if (!dir)
    return NULL;
  memcpy(dir, path, length);
  dir[length] = '\0';
  return dir;
}

static int file_exists(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return 0;
  fclose(file);
  return 1;
}

static void resolve_asset_path(const char *material_path, const char *value, char *out, size_t out_size) {
  if (!out || out_size == 0)
    return;
  out[0] = '\0';
  if (!value || !value[0])
    return;

  if (file_exists(value)) {
    strncpy(out, value, out_size - 1);
    out[out_size - 1] = '\0';
    return;
  }

  char *dir = material_dir(material_path);
  if (dir) {
    snprintf(out, out_size, "%s/%s", dir, value);
    if (file_exists(out)) {
      free(dir);
      return;
    }
    out[0] = '\0';
    free(dir);
  }

  strncpy(out, value, out_size - 1);
  out[out_size - 1] = '\0';
}

BLB_Material *BLB_Material_Load(const char *path, BLB_MaterialTextureLoadFn texture_load, BLB_MaterialTextureRetainFn texture_retain,
                                BLB_MaterialTextureReleaseFn texture_release, void *user_data) {
  if (!path)
    return NULL;

  const bool use_cache = !texture_load && !texture_retain && !texture_release && !user_data;
  if (use_cache) {
    BLB_Material *cached = material_cache_find(path);
    if (cached) {
      BLB_Material_Retain(cached);
      return cached;
    }
  }

  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  BLB_Material *material = BLB_Material_Create3D();
  if (!material) {
    fclose(file);
    return NULL;
  }

  material->texture_retain = texture_retain ? texture_retain : (BLB_MaterialTextureRetainFn)BLB_Texture_Retain;
  material->texture_release = texture_release ? texture_release : (BLB_MaterialTextureReleaseFn)BLB_Texture_Release;
  material->texture_user_data = user_data;

  BLB_MaterialParseSection section = BLB_MAT_SEC_NONE;
  int texture_slot = -1;
  char line[1024];
  char shader_vertex_path[BLB_SHADER_PATH_MAX] = {0};
  char shader_fragment_path[BLB_SHADER_PATH_MAX] = {0};
  bool have_shader_vertex = false;
  bool have_shader_fragment = false;
  char pending_block[64] = {0};

  while (fgets(line, sizeof(line), file)) {
    char *comment = strstr(line, "//");
    if (comment)
      *comment = '\0';
    comment = strchr(line, '#');
    if (comment && (comment == line || comment[-1] == ' ' || comment[-1] == '\t'))
      *comment = '\0';

    char *text = trim_ws(line);
    if (!*text)
      continue;

    if (strncmp(text, "material ", 9) == 0) {
      char name[BLB_MATERIAL_NAME_MAX];
      parse_string_value(text + 9, name, sizeof(name));
      BLB_Material_SetName(material, name);
      continue;
    }

    if (text[0] == '}') {
      if (texture_slot >= 0) {
        texture_slot = -1;
      } else {
        section = BLB_MAT_SEC_NONE;
      }
      continue;
    }

    if (strcmp(text, "{") == 0 && pending_block[0] != '\0') {
      char name[64];
      strncpy(name, pending_block, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
      pending_block[0] = '\0';

      if (section == BLB_MAT_SEC_TEXTURES) {
        texture_slot = texture_slot_index(name);
        continue;
      }
      if (strcmp(name, "render") == 0) section = BLB_MAT_SEC_RENDER;
      else if (strcmp(name, "base") == 0) section = BLB_MAT_SEC_BASE;
      else if (strcmp(name, "emission") == 0) section = BLB_MAT_SEC_EMISSION;
      else if (strcmp(name, "specular") == 0) section = BLB_MAT_SEC_SPECULAR;
      else if (strcmp(name, "transmission") == 0) section = BLB_MAT_SEC_TRANSMISSION;
      else if (strcmp(name, "volume") == 0) section = BLB_MAT_SEC_VOLUME;
      else if (strcmp(name, "clearcoat") == 0) section = BLB_MAT_SEC_CLEARCOAT;
      else if (strcmp(name, "sheen") == 0) section = BLB_MAT_SEC_SHEEN;
      else if (strcmp(name, "iridescence") == 0) section = BLB_MAT_SEC_IRIDESCENCE;
      else if (strcmp(name, "anisotropy") == 0) section = BLB_MAT_SEC_ANISOTROPY;
      else if (strcmp(name, "glow") == 0) section = BLB_MAT_SEC_GLOW;
      else if (strcmp(name, "textures") == 0) { section = BLB_MAT_SEC_TEXTURES; texture_slot = -1; }
      else if (strcmp(name, "shader") == 0) section = BLB_MAT_SEC_SHADER;
      continue;
    }

    if (strchr(text, '{') && !strchr(text, '=')) {
      char name[64] = {0};
      sscanf(text, "%63s", name);
      if (section == BLB_MAT_SEC_TEXTURES) {
        texture_slot = texture_slot_index(name);
        continue;
      }
      if (strcmp(name, "render") == 0) section = BLB_MAT_SEC_RENDER;
      else if (strcmp(name, "base") == 0) section = BLB_MAT_SEC_BASE;
      else if (strcmp(name, "emission") == 0) section = BLB_MAT_SEC_EMISSION;
      else if (strcmp(name, "specular") == 0) section = BLB_MAT_SEC_SPECULAR;
      else if (strcmp(name, "transmission") == 0) section = BLB_MAT_SEC_TRANSMISSION;
      else if (strcmp(name, "volume") == 0) section = BLB_MAT_SEC_VOLUME;
      else if (strcmp(name, "clearcoat") == 0) section = BLB_MAT_SEC_CLEARCOAT;
      else if (strcmp(name, "sheen") == 0) section = BLB_MAT_SEC_SHEEN;
      else if (strcmp(name, "iridescence") == 0) section = BLB_MAT_SEC_IRIDESCENCE;
      else if (strcmp(name, "anisotropy") == 0) section = BLB_MAT_SEC_ANISOTROPY;
      else if (strcmp(name, "glow") == 0) section = BLB_MAT_SEC_GLOW;
      else if (strcmp(name, "textures") == 0) { section = BLB_MAT_SEC_TEXTURES; texture_slot = -1; }
      else if (strcmp(name, "shader") == 0) section = BLB_MAT_SEC_SHADER;
      continue;
    }

    if (!strchr(text, '=') && text[0] != '}') {
      strncpy(pending_block, text, sizeof(pending_block) - 1);
      pending_block[sizeof(pending_block) - 1] = '\0';
      continue;
    }

    char key[64] = {0};
    char value[896] = {0};
    const char *equals = strchr(text, '=');
    if (!equals)
      continue;
    size_t key_len = (size_t)(equals - text);
    if (key_len >= sizeof(key))
      key_len = sizeof(key) - 1;
    memcpy(key, text, key_len);
    key[key_len] = '\0';
    char *clean_key = trim_ws(key);
    strncpy(value, trim_ws((char *)equals + 1), sizeof(value) - 1);

    if (texture_slot >= 0) {
      BLB_MaterialTexture *slot = material_texture_slot(material, texture_slot);
      if (!slot)
        continue;
      if (strcmp(clean_key, "path") == 0) {
        char asset_path[BLB_SHADER_PATH_MAX] = {0};
        parse_string_value(value, asset_path, sizeof(asset_path));
        if (asset_path[0]) {
          char resolved[BLB_SHADER_PATH_MAX] = {0};
          resolve_asset_path(path, asset_path, resolved, sizeof(resolved));
          BLB_Texture *texture = texture_load ? texture_load(resolved, user_data) : BLB_Texture_Load2D(resolved);
          if (texture)
            BLB_MaterialTexture_AdoptTexture(slot, texture);
        }
      } else if (strcmp(clean_key, "uv_set") == 0) {
        slot->uv_set = (int)parse_float_value(value, 0.0f);
      } else if (strcmp(clean_key, "offset") == 0) {
        parse_float_list(value, slot->offset, 2);
      } else if (strcmp(clean_key, "scale") == 0) {
        parse_float_list(value, slot->scale, 2);
      } else if (strcmp(clean_key, "rotation") == 0) {
        slot->rotation = parse_float_value(value, 0.0f);
      } else if (strcmp(clean_key, "wrap_u") == 0) {
        char token[64]; sscanf(value, "%63s", token);
        slot->wrap_u = strcmp(token, "clamp") == 0 || strcmp(token, "clamp_to_edge") == 0 ? BLB_TEXTURE_WRAP_CLAMP_TO_EDGE
                      : strcmp(token, "mirrored_repeat") == 0 ? BLB_TEXTURE_WRAP_MIRRORED_REPEAT : BLB_TEXTURE_WRAP_REPEAT;
      } else if (strcmp(clean_key, "wrap_v") == 0) {
        char token[64]; sscanf(value, "%63s", token);
        slot->wrap_v = strcmp(token, "clamp") == 0 || strcmp(token, "clamp_to_edge") == 0 ? BLB_TEXTURE_WRAP_CLAMP_TO_EDGE
                      : strcmp(token, "mirrored_repeat") == 0 ? BLB_TEXTURE_WRAP_MIRRORED_REPEAT : BLB_TEXTURE_WRAP_REPEAT;
      } else if (strcmp(clean_key, "min_filter") == 0) {
        slot->min_filter = strstr(value, "nearest") ? BLB_TEXTURE_FILTER_NEAREST : BLB_TEXTURE_FILTER_LINEAR;
      } else if (strcmp(clean_key, "mag_filter") == 0) {
        slot->mag_filter = strstr(value, "nearest") ? BLB_TEXTURE_FILTER_NEAREST : BLB_TEXTURE_FILTER_LINEAR;
      }
      material_touch(material);
      continue;
    }

    if (section == BLB_MAT_SEC_RENDER) {
      if (strcmp(clean_key, "mode") == 0) material->render_mode = parse_render_mode(value);
      else if (strcmp(clean_key, "alpha_mode") == 0) material->alpha_mode = parse_alpha_mode(value);
      else if (strcmp(clean_key, "alpha_cutoff") == 0) material->alpha_cutoff = clamp01(parse_float_value(value, material->alpha_cutoff));
      else if (strcmp(clean_key, "lighting") == 0) material->lighting_enabled = parse_bool_value(value, material->lighting_enabled);
      else if (strcmp(clean_key, "depth_test") == 0) material->depth_enabled = parse_bool_value(value, material->depth_enabled);
      else if (strcmp(clean_key, "depth_write") == 0) material->depth_write = parse_bool_value(value, material->depth_write);
      else if (strcmp(clean_key, "double_sided") == 0) material->double_sided = parse_bool_value(value, material->double_sided);
      else if (strcmp(clean_key, "unlit") == 0) material->unlit = parse_bool_value(value, material->unlit);
      material_touch(material);
      continue;
    }

    if (section == BLB_MAT_SEC_BASE) {
      if (strcmp(clean_key, "color") == 0) parse_float_list(value, material->base_color, 4);
      else if (strcmp(clean_key, "metallic") == 0) material->metallic = clamp01(parse_float_value(value, material->metallic));
      else if (strcmp(clean_key, "roughness") == 0) material->roughness = clamp01(parse_float_value(value, material->roughness));
      else if (strcmp(clean_key, "normal_scale") == 0) material->normal_scale = non_negative(parse_float_value(value, material->normal_scale));
      else if (strcmp(clean_key, "occlusion_strength") == 0) material->occlusion_strength = clamp01(parse_float_value(value, material->occlusion_strength));
      material_touch(material);
      continue;
    }

    if (section == BLB_MAT_SEC_EMISSION) {
      if (strcmp(clean_key, "color") == 0) parse_float_list(value, material->emission_color, 4);
      else if (strcmp(clean_key, "strength") == 0) material->emission_strength = non_negative(parse_float_value(value, material->emission_strength));
      material_touch(material);
      continue;
    }

    if (section == BLB_MAT_SEC_SPECULAR) {
      if (strcmp(clean_key, "factor") == 0) material->specular_factor = clamp01(parse_float_value(value, material->specular_factor));
      else if (strcmp(clean_key, "color") == 0) parse_float_list(value, material->specular_color, 3);
      material_touch(material);
      continue;
    }

    if (section == BLB_MAT_SEC_TRANSMISSION) {
      if (strcmp(clean_key, "factor") == 0) material->transmission = clamp01(parse_float_value(value, material->transmission));
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_VOLUME) {
      if (strcmp(clean_key, "thickness") == 0) material->volume_thickness = non_negative(parse_float_value(value, material->volume_thickness));
      else if (strcmp(clean_key, "attenuation_distance") == 0) material->attenuation_distance = parse_float_value(value, material->attenuation_distance);
      else if (strcmp(clean_key, "attenuation_color") == 0) parse_float_list(value, material->attenuation_color, 3);
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_CLEARCOAT) {
      if (strcmp(clean_key, "factor") == 0) material->clearcoat_factor = clamp01(parse_float_value(value, material->clearcoat_factor));
      else if (strcmp(clean_key, "roughness") == 0) material->clearcoat_roughness = clamp01(parse_float_value(value, material->clearcoat_roughness));
      else if (strcmp(clean_key, "normal_scale") == 0) material->clearcoat_normal_scale = non_negative(parse_float_value(value, material->clearcoat_normal_scale));
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_SHEEN) {
      if (strcmp(clean_key, "color") == 0) parse_float_list(value, material->sheen_color, 3);
      else if (strcmp(clean_key, "roughness") == 0) material->sheen_roughness = clamp01(parse_float_value(value, material->sheen_roughness));
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_IRIDESCENCE) {
      if (strcmp(clean_key, "factor") == 0) material->iridescence_factor = clamp01(parse_float_value(value, material->iridescence_factor));
      else if (strcmp(clean_key, "ior") == 0) material->iridescence_ior = fmaxf(1.0f, parse_float_value(value, material->iridescence_ior));
      else if (strcmp(clean_key, "thickness_min") == 0) material->iridescence_thickness_min = non_negative(parse_float_value(value, material->iridescence_thickness_min));
      else if (strcmp(clean_key, "thickness_max") == 0) material->iridescence_thickness_max = fmaxf(material->iridescence_thickness_min, parse_float_value(value, material->iridescence_thickness_max));
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_ANISOTROPY) {
      if (strcmp(clean_key, "strength") == 0) material->anisotropy_strength = clamp01(parse_float_value(value, material->anisotropy_strength));
      else if (strcmp(clean_key, "rotation") == 0) material->anisotropy_rotation = parse_float_value(value, material->anisotropy_rotation);
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_GLOW) {
      if (strcmp(clean_key, "strength") == 0) material->glow_strength = non_negative(parse_float_value(value, material->glow_strength));
      else if (strcmp(clean_key, "radius") == 0) material->glow_radius = non_negative(parse_float_value(value, material->glow_radius));
      else if (strcmp(clean_key, "falloff") == 0) material->glow_falloff = fmaxf(0.2f, parse_float_value(value, material->glow_falloff));
      material_touch(material); continue;
    }

    if (section == BLB_MAT_SEC_SHADER) {
      if (strcmp(clean_key, "vertex") == 0) {
        char raw[BLB_SHADER_PATH_MAX];
        parse_string_value(value, raw, sizeof(raw));
        resolve_asset_path(path, raw, shader_vertex_path, sizeof(shader_vertex_path));
        have_shader_vertex = shader_vertex_path[0] != '\0';
      } else if (strcmp(clean_key, "fragment") == 0) {
        char raw[BLB_SHADER_PATH_MAX];
        parse_string_value(value, raw, sizeof(raw));
        resolve_asset_path(path, raw, shader_fragment_path, sizeof(shader_fragment_path));
        have_shader_fragment = shader_fragment_path[0] != '\0';
      }
      /* Shader loading is deferred until the whole material is parsed so section order
       * cannot accidentally force the shader into the wrong 2D/3D pipeline contract. */
      continue;
    }

    if (strcmp(clean_key, "domain") == 0) {
      material->domain = strstr(value, "2d") ? BLB_MATERIAL_2D : BLB_MATERIAL_3D;
      material->depth_enabled = material->domain == BLB_MATERIAL_3D;
      material->depth_write = material->domain == BLB_MATERIAL_3D;
      material_touch(material);
    } else if (strcmp(clean_key, "ior") == 0) {
      material->ior = fmaxf(1.0f, parse_float_value(value, material->ior));
      material_touch(material);
    } else if (strcmp(clean_key, "dispersion") == 0) {
      material->dispersion = clamp01(parse_float_value(value, material->dispersion));
      material_touch(material);
    }
  }

  fclose(file);

  if (have_shader_vertex && have_shader_fragment) {
    BLB_ShaderProgram *shader = BLB_Shader_LoadSPIRV(shader_vertex_path, shader_fragment_path, material->domain == BLB_MATERIAL_2D);
    if (shader) {
      BLB_Material_SetShader(material, shader);
      BLB_Shader_Release(shader);
    }
  }

  if (material->domain == BLB_MATERIAL_2D && material->depth_enabled == true && material->unlit)
    material->depth_enabled = false;

  if (use_cache)
    material_cache_insert(material, path);

  return material;
}

bool BLB_Material_IsGlowing(const BLB_Material *material) {
  return material && (material->emission_strength > 0.0f || material->glow_strength > 0.0f);
}
