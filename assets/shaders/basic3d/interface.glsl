#include "common/lights.glsl"

#define TEX_COUNT 17

const uint TEX_BASE = 0u;
const uint TEX_METAL_ROUGH = 1u;
const uint TEX_NORMAL = 2u;
const uint TEX_OCCLUSION = 3u;
const uint TEX_EMISSION = 4u;
const uint TEX_SPECULAR = 5u;
const uint TEX_SPECULAR_COLOR = 6u;
const uint TEX_CLEARCOAT = 7u;
const uint TEX_CLEARCOAT_ROUGH = 8u;
const uint TEX_CLEARCOAT_NORMAL = 9u;
const uint TEX_TRANSMISSION = 10u;
const uint TEX_THICKNESS = 11u;
const uint TEX_SHEEN_COLOR = 12u;
const uint TEX_SHEEN_ROUGH = 13u;
const uint TEX_IRIDESCENCE = 14u;
const uint TEX_IRIDESCENCE_THICKNESS = 15u;
const uint TEX_ANISOTROPY = 16u;

const float PI = 3.14159265359;

struct MaterialTextureParams {
  uint packed[3];
};

layout(set = 0, binding = 1) uniform sampler2DShadow shadow_map;
layout(set = 0, binding = 2) uniform sampler2DShadow shadow_map_1;
layout(set = 0, binding = 3) uniform sampler2DShadow shadow_map_2;
layout(set = 0, binding = 4) uniform sampler2DShadow shadow_map_3;
layout(set = 0, binding = 5) uniform sampler2DShadow shadow_map_4;
layout(set = 0, binding = 6) uniform sampler2DShadow shadow_map_5;
layout(set = 0, binding = 7) uniform sampler2DShadow shadow_map_6;

layout(set = 1, binding = 0) uniform sampler2D material_textures[TEX_COUNT];

layout(set = 1, binding = 2) uniform sampler3D material_texture_3d;

layout(std430, set = 1, binding = 1) readonly buffer MaterialBuffer { MaterialTextureParams textures[TEX_COUNT]; }
material_buffer;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_position;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_uv;
layout(location = 4) flat in uint in_entity_id;
layout(location = 5) in vec3 in_local_position;
layout(location = 6) in vec3 in_local_normal;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  vec4 model_rows[3];
  vec4 material;
  vec4 pbr;
  vec4 emission;
  uvec4 material_ext;
  uvec4 surface[2];
  uvec4 meta;
  vec4 uv_bounds_min;
  vec4 uv_bounds_max;
}
push_constants;
