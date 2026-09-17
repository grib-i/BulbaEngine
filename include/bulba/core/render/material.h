#ifndef BULBA_CORE_RENDER_MATERIAL_H
#define BULBA_CORE_RENDER_MATERIAL_H

#include "bulba/core/render_mode.h"

#include <stdbool.h>
#include <stddef.h>

#define BLB_MATERIAL_NAME_MAX 64
#define BLB_MATERIAL_PATH_MAX 512

typedef enum {
  BLB_MATERIAL_2D = 0,
  BLB_MATERIAL_3D = 1
} BLB_MaterialDomain;

typedef struct BLB_Material {
  size_t ref_count;
  BLB_MaterialDomain domain;
  BLB_RenderMode render_mode;
  bool lighting_enabled;
  bool depth_enabled;
  float base_color[4];
  float emission_color[4];
  float emission_strength;
  float glow_strength;
  float glow_radius;
  float glow_falloff;
  float roughness;
  float metallic;
  float alpha_cutoff;
  char name[BLB_MATERIAL_NAME_MAX];
  char shader_vertex[BLB_MATERIAL_PATH_MAX];
  char shader_fragment[BLB_MATERIAL_PATH_MAX];
  void *user_data;
} BLB_Material;

BLB_Material *BLB_Material_Create(BLB_MaterialDomain domain);
BLB_Material *BLB_Material_Create2D(void);
BLB_Material *BLB_Material_Create3D(void);
void BLB_Material_Retain(BLB_Material *material);
void BLB_Material_Release(BLB_Material *material);
BLB_Material *BLB_Material_Clone(const BLB_Material *material);
void BLB_Material_SetName(BLB_Material *material, const char *name);
void BLB_Material_SetBaseColor(BLB_Material *material, float r, float g, float b, float a);
void BLB_Material_SetEmission(BLB_Material *material, float r, float g, float b, float a, float strength);
void BLB_Material_SetGlow(BLB_Material *material, float strength, float radius, float falloff);
void BLB_Material_SetRenderMode(BLB_Material *material, BLB_RenderMode mode);
void BLB_Material_SetLighting(BLB_Material *material, bool enabled);
void BLB_Material_SetShader(BLB_Material *material, const char *vertex_path, const char *fragment_path);
bool BLB_Material_IsGlowing(const BLB_Material *material);

#endif
