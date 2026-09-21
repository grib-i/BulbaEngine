#ifndef BULBA_CORE_RENDER_MATERIAL_H
#define BULBA_CORE_RENDER_MATERIAL_H

#include "bulba/core/render_mode.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLB_MATERIAL_NAME_MAX 64

typedef struct BLB_Texture BLB_Texture;

typedef enum { BLB_MATERIAL_2D = 0, BLB_MATERIAL_3D = 1 } BLB_MaterialDomain;

typedef enum { BLB_ALPHA_OPAQUE = 0, BLB_ALPHA_MASK = 1, BLB_ALPHA_BLEND = 2 } BLB_AlphaMode;

typedef enum { BLB_TEXTURE_WRAP_REPEAT = 0, BLB_TEXTURE_WRAP_CLAMP_TO_EDGE = 1, BLB_TEXTURE_WRAP_MIRRORED_REPEAT = 2 } BLB_TextureWrap;

typedef enum { BLB_TEXTURE_FILTER_NEAREST = 0, BLB_TEXTURE_FILTER_LINEAR = 1 } BLB_TextureFilter;

typedef struct BLB_MaterialTexture {
  BLB_Texture *texture;
  bool owned;
  int uv_set;
  float offset[2];
  float scale[2];
  float rotation;
  BLB_TextureWrap wrap_u;
  BLB_TextureWrap wrap_v;
  BLB_TextureFilter min_filter;
  BLB_TextureFilter mag_filter;
} BLB_MaterialTexture;

typedef BLB_Texture *(*BLB_MaterialTextureLoadFn)(const char *path, void *user_data);
typedef void (*BLB_MaterialTextureRetainFn)(BLB_Texture *texture, void *user_data);
typedef void (*BLB_MaterialTextureReleaseFn)(BLB_Texture *texture, void *user_data);

typedef struct BLB_Material BLB_Material;
typedef void (*BLB_MaterialConfigureFn)(BLB_Material *material);

struct BLB_Material {
  size_t ref_count;
  BLB_MaterialDomain domain;
  BLB_RenderMode render_mode;
  BLB_AlphaMode alpha_mode;

  bool lighting_enabled;
  bool depth_enabled;
  bool depth_write;
  bool double_sided;
  bool unlit;

  float base_color[4];
  float metallic;
  float roughness;

  float normal_scale;
  float occlusion_strength;

  float emission_color[4];
  float emission_strength;

  float alpha_cutoff;

  float specular_factor;
  float specular_color[3];

  float ior;
  float transmission;

  float volume_thickness;
  float attenuation_color[3];
  float attenuation_distance;

  float clearcoat_factor;
  float clearcoat_roughness;
  float clearcoat_normal_scale;

  float sheen_color[3];
  float sheen_roughness;

  float iridescence_factor;
  float iridescence_ior;
  float iridescence_thickness_min;
  float iridescence_thickness_max;

  float anisotropy_strength;
  float anisotropy_rotation;

  float dispersion;

  float glow_strength;
  float glow_radius;
  float glow_falloff;
  float temperature;

  BLB_MaterialTexture base_color_texture;
  BLB_MaterialTexture metallic_roughness_texture;
  BLB_MaterialTexture normal_texture;
  BLB_MaterialTexture occlusion_texture;
  BLB_MaterialTexture emission_texture;

  BLB_MaterialTexture specular_texture;
  BLB_MaterialTexture specular_color_texture;

  BLB_MaterialTexture clearcoat_texture;
  BLB_MaterialTexture clearcoat_roughness_texture;
  BLB_MaterialTexture clearcoat_normal_texture;

  BLB_MaterialTexture transmission_texture;
  BLB_MaterialTexture thickness_texture;

  BLB_MaterialTexture sheen_color_texture;
  BLB_MaterialTexture sheen_roughness_texture;

  BLB_MaterialTexture iridescence_texture;
  BLB_MaterialTexture iridescence_thickness_texture;

  BLB_MaterialTexture anisotropy_texture;

  char name[BLB_MATERIAL_NAME_MAX];

  void *shader_vertex;
  void *shader_fragment;

  BLB_MaterialTextureRetainFn texture_retain;
  BLB_MaterialTextureReleaseFn texture_release;
  void *texture_user_data;

  void *user_data;

  /* Renderer-side cache revision. Opaque backend state is owned by the renderer. */
  uint64_t revision;
  void *backend_data;
  void (*backend_destroy)(void *backend_data);
};

BLB_Material *BLB_Material_Create(BLB_MaterialDomain domain);
BLB_Material *BLB_Material_Create2D(void);
BLB_Material *BLB_Material_Create3D(void);
BLB_Material *BLB_Material_Build(BLB_MaterialDomain domain, BLB_MaterialConfigureFn configure);

BLB_Material *BLB_Material_Load(const char *path, BLB_MaterialTextureLoadFn texture_load, BLB_MaterialTextureRetainFn texture_retain,
                                BLB_MaterialTextureReleaseFn texture_release, void *user_data);

void BLB_Material_Retain(BLB_Material *material);
void BLB_Material_Release(BLB_Material *material);
BLB_Material *BLB_Material_Clone(const BLB_Material *material);

void BLB_Material_SetName(BLB_Material *material, const char *name);
void BLB_Material_SetDomain(BLB_Material *material, BLB_MaterialDomain domain);
void BLB_Material_SetBaseColor(BLB_Material *material, float r, float g, float b, float a);
void BLB_Material_SetEmission(BLB_Material *material, float r, float g, float b, float a, float strength);
void BLB_Material_SetAlphaCutoff(BLB_Material *material, float cutoff);
void BLB_Material_SetPBR(BLB_Material *material, float metallic, float roughness);
void BLB_Material_SetNormal(BLB_Material *material, float scale);
void BLB_Material_SetOcclusion(BLB_Material *material, float strength);
void BLB_Material_SetSpecular(BLB_Material *material, float factor, float r, float g, float b);
void BLB_Material_SetIOR(BLB_Material *material, float ior);
void BLB_Material_SetTransmission(BLB_Material *material, float transmission);
void BLB_Material_SetVolume(BLB_Material *material, float thickness, float attenuation_distance, float r, float g, float b);
void BLB_Material_SetClearcoat(BLB_Material *material, float factor, float roughness, float normal_scale);
void BLB_Material_SetSheen(BLB_Material *material, float r, float g, float b, float roughness);
void BLB_Material_SetIridescence(BLB_Material *material, float factor, float ior, float thickness_min, float thickness_max);
void BLB_Material_SetAnisotropy(BLB_Material *material, float strength, float rotation);
void BLB_Material_SetDispersion(BLB_Material *material, float dispersion);
void BLB_Material_SetGlow(BLB_Material *material, float strength, float radius, float falloff);
void BLB_Material_SetTemperature(BLB_Material *material, float kelvin);
void BLB_Material_SetRenderMode(BLB_Material *material, BLB_RenderMode mode);
void BLB_Material_SetAlphaMode(BLB_Material *material, BLB_AlphaMode mode);
void BLB_Material_SetLighting(BLB_Material *material, bool enabled);
void BLB_Material_SetDepth(BLB_Material *material, bool enabled, bool write);
void BLB_Material_SetDoubleSided(BLB_Material *material, bool enabled);
void BLB_Material_SetUnlit(BLB_Material *material, bool enabled);

void BLB_MaterialTexture_Init(BLB_MaterialTexture *texture);
void BLB_MaterialTexture_SetTexture(BLB_Material *material, BLB_MaterialTexture *slot, BLB_Texture *texture);
void BLB_MaterialTexture_AdoptTexture(BLB_MaterialTexture *slot, BLB_Texture *texture);
void BLB_MaterialTexture_SetTransform(BLB_MaterialTexture *texture, float offset_x, float offset_y, float scale_x, float scale_y, float rotation);
void BLB_MaterialTexture_SetSampler(BLB_MaterialTexture *texture, BLB_TextureWrap wrap_u, BLB_TextureWrap wrap_v, BLB_TextureFilter min_filter,
                                    BLB_TextureFilter mag_filter);
bool BLB_MaterialTexture_Valid(const BLB_MaterialTexture *texture);
uint64_t BLB_Material_TextureStateRevision(void);

bool BLB_Material_IsGlowing(const BLB_Material *material);

#endif
