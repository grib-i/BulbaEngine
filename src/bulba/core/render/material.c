#include "bulba/core/render/material.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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
  material->lighting_enabled = domain == BLB_MATERIAL_3D;
  material->depth_enabled = domain == BLB_MATERIAL_3D;
  material->depth_write = domain == BLB_MATERIAL_3D;
  material->double_sided = domain == BLB_MATERIAL_2D;
  material->unlit = domain == BLB_MATERIAL_2D;

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
}

void BLB_Material_SetDomain(BLB_Material *material, BLB_MaterialDomain domain) {
  if (material)
    material->domain = domain;
}

void BLB_Material_SetBaseColor(BLB_Material *material, float r, float g, float b, float a) {
  if (!material)
    return;

  material->base_color[0] = r;
  material->base_color[1] = g;
  material->base_color[2] = b;
  material->base_color[3] = a;
}

void BLB_Material_SetEmission(BLB_Material *material, float r, float g, float b, float a, float strength) {
  if (!material)
    return;

  material->emission_color[0] = r;
  material->emission_color[1] = g;
  material->emission_color[2] = b;
  material->emission_color[3] = a;
  material->emission_strength = non_negative(strength);
}

void BLB_Material_SetPBR(BLB_Material *material, float metallic, float roughness) {
  if (!material)
    return;

  material->metallic = clamp01(metallic);
  material->roughness = clamp01(roughness);
}

void BLB_Material_SetNormal(BLB_Material *material, float scale) {
  if (material)
    material->normal_scale = non_negative(scale);
}

void BLB_Material_SetOcclusion(BLB_Material *material, float strength) {
  if (material)
    material->occlusion_strength = clamp01(strength);
}

void BLB_Material_SetSpecular(BLB_Material *material, float factor, float r, float g, float b) {
  if (!material)
    return;

  material->specular_factor = clamp01(factor);
  material->specular_color[0] = r;
  material->specular_color[1] = g;
  material->specular_color[2] = b;
}

void BLB_Material_SetIOR(BLB_Material *material, float ior) {
  if (material)
    material->ior = ior < 1.0f ? 1.0f : ior;
}

void BLB_Material_SetTransmission(BLB_Material *material, float transmission) {
  if (material)
    material->transmission = clamp01(transmission);
}

void BLB_Material_SetVolume(BLB_Material *material, float thickness, float attenuation_distance, float r, float g, float b) {
  if (!material)
    return;

  material->volume_thickness = non_negative(thickness);
  material->attenuation_distance = attenuation_distance > 0.0f ? attenuation_distance : INFINITY;
  material->attenuation_color[0] = r;
  material->attenuation_color[1] = g;
  material->attenuation_color[2] = b;
}

void BLB_Material_SetClearcoat(BLB_Material *material, float factor, float roughness, float normal_scale) {
  if (!material)
    return;

  material->clearcoat_factor = clamp01(factor);
  material->clearcoat_roughness = clamp01(roughness);
  material->clearcoat_normal_scale = non_negative(normal_scale);
}

void BLB_Material_SetSheen(BLB_Material *material, float r, float g, float b, float roughness) {
  if (!material)
    return;

  material->sheen_color[0] = r;
  material->sheen_color[1] = g;
  material->sheen_color[2] = b;
  material->sheen_roughness = clamp01(roughness);
}

void BLB_Material_SetIridescence(BLB_Material *material, float factor, float ior, float thickness_min, float thickness_max) {
  if (!material)
    return;

  material->iridescence_factor = clamp01(factor);
  material->iridescence_ior = ior < 1.0f ? 1.0f : ior;
  material->iridescence_thickness_min = non_negative(thickness_min);
  material->iridescence_thickness_max = thickness_max < material->iridescence_thickness_min ? material->iridescence_thickness_min : thickness_max;
}

void BLB_Material_SetAnisotropy(BLB_Material *material, float strength, float rotation) {
  if (!material)
    return;

  material->anisotropy_strength = clamp01(strength);
  material->anisotropy_rotation = rotation;
}

void BLB_Material_SetDispersion(BLB_Material *material, float dispersion) {
  if (material)
    material->dispersion = non_negative(dispersion);
}

void BLB_Material_SetGlow(BLB_Material *material, float strength, float radius, float falloff) {
  if (!material)
    return;

  material->glow_strength = non_negative(strength);
  material->glow_radius = non_negative(radius);
  material->glow_falloff = falloff <= 0.0f ? 1.0f : falloff;
}

void BLB_Material_SetRenderMode(BLB_Material *material, BLB_RenderMode mode) {
  if (material)
    material->render_mode = normalize_mode(mode);
}

void BLB_Material_SetAlphaMode(BLB_Material *material, BLB_AlphaMode mode) {
  if (material)
    material->alpha_mode = normalize_alpha_mode(mode);
}

void BLB_Material_SetLighting(BLB_Material *material, bool enabled) {
  if (material)
    material->lighting_enabled = enabled;
}

void BLB_Material_SetDepth(BLB_Material *material, bool enabled, bool write) {
  if (!material)
    return;

  material->depth_enabled = enabled;
  material->depth_write = write;
}

void BLB_Material_SetDoubleSided(BLB_Material *material, bool enabled) {
  if (material)
    material->double_sided = enabled;
}

void BLB_Material_SetUnlit(BLB_Material *material, bool enabled) {
  if (material)
    material->unlit = enabled;
}

void BLB_MaterialTexture_Init(BLB_MaterialTexture *texture) { init_texture(texture); }

void BLB_MaterialTexture_SetTexture(BLB_Material *material, BLB_MaterialTexture *slot, BLB_Texture *texture) {
  if (!material || !slot)
    return;

  release_material_texture(material, slot);

  slot->texture = texture;
  slot->owned = false;
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
}

void BLB_MaterialTexture_SetSampler(BLB_MaterialTexture *texture, BLB_TextureWrap wrap_u, BLB_TextureWrap wrap_v, BLB_TextureFilter min_filter,
                                    BLB_TextureFilter mag_filter) {
  if (!texture)
    return;

  texture->wrap_u = wrap_u;
  texture->wrap_v = wrap_v;
  texture->min_filter = min_filter;
  texture->mag_filter = mag_filter;
}

bool BLB_MaterialTexture_Valid(const BLB_MaterialTexture *texture) { return texture && texture->texture; }

bool BLB_Material_IsGlowing(const BLB_Material *material) {
  return material && (material->emission_strength > 0.0f || material->glow_strength > 0.0f);
}
