#include "common/lights.glsl"

#define TEX_COUNT 17
#define TEX_BASE 0u

layout(set = 1, binding = 0) uniform sampler2D material_textures[TEX_COUNT];

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_position;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  vec4 viewport;
  vec4 material;
  vec4 pbr;
  vec4 emission;
  uvec4 material_ext;
  uvec4 surface[2];
  uvec4 meta;
}
push_constants;

bool has_texture(uint slot) { return (push_constants.meta.x & (1u << slot)) != 0u; }

