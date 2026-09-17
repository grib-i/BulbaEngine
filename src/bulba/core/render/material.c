#include "bulba/core/render/material.h"

#include <stdlib.h>
#include <string.h>

static BLB_RenderMode normalize_mode(BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    return BLB_RENDER_OPAQUE;
  return mode;
}

BLB_Material *BLB_Material_Create(BLB_MaterialDomain domain) {
  BLB_Material *material = calloc(1, sizeof(*material));
  if (!material)
    return NULL;

  material->ref_count = 1;
  material->domain = domain;
  material->render_mode = BLB_RENDER_OPAQUE;
  material->lighting_enabled = domain == BLB_MATERIAL_3D;
  material->depth_enabled = domain == BLB_MATERIAL_3D;
  material->base_color[0] = 1.0f;
  material->base_color[1] = 1.0f;
  material->base_color[2] = 1.0f;
  material->base_color[3] = 1.0f;
  material->emission_color[0] = 1.0f;
  material->emission_color[1] = 1.0f;
  material->emission_color[2] = 1.0f;
  material->emission_color[3] = 1.0f;
  material->glow_falloff = 2.0f;
  material->roughness = 0.65f;
  material->alpha_cutoff = 0.5f;

  return material;
}

BLB_Material *BLB_Material_Create2D(void) {
  return BLB_Material_Create(BLB_MATERIAL_2D);
}

BLB_Material *BLB_Material_Create3D(void) {
  return BLB_Material_Create(BLB_MATERIAL_3D);
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
  return copy;
}

void BLB_Material_SetName(BLB_Material *material, const char *name) {
  if (!material)
    return;

  memset(material->name, 0, sizeof(material->name));
  if (name)
    strncpy(material->name, name, sizeof(material->name) - 1);
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
  material->emission_strength = strength < 0.0f ? 0.0f : strength;
}

void BLB_Material_SetGlow(BLB_Material *material, float strength, float radius, float falloff) {
  if (!material)
    return;
  material->glow_strength = strength < 0.0f ? 0.0f : strength;
  material->glow_radius = radius < 0.0f ? 0.0f : radius;
  material->glow_falloff = falloff <= 0.0f ? 1.0f : falloff;
}

void BLB_Material_SetRenderMode(BLB_Material *material, BLB_RenderMode mode) {
  if (material)
    material->render_mode = normalize_mode(mode);
}

void BLB_Material_SetLighting(BLB_Material *material, bool enabled) {
  if (material)
    material->lighting_enabled = enabled;
}

void BLB_Material_SetShader(BLB_Material *material, const char *vertex_path, const char *fragment_path) {
  if (!material)
    return;
  memset(material->shader_vertex, 0, sizeof(material->shader_vertex));
  memset(material->shader_fragment, 0, sizeof(material->shader_fragment));
  if (vertex_path)
    strncpy(material->shader_vertex, vertex_path, sizeof(material->shader_vertex) - 1);
  if (fragment_path)
    strncpy(material->shader_fragment, fragment_path, sizeof(material->shader_fragment) - 1);
}

bool BLB_Material_IsGlowing(const BLB_Material *material) {
  return material && (material->emission_strength > 0.0f || material->glow_strength > 0.0f);
}
