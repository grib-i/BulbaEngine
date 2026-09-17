#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  vec4 material;
} push_constants;

void main() {
  gl_Position = push_constants.mvp * vec4(in_position.xy, 0.0, 1.0);
  out_color = in_color;
  out_uv = in_uv;
}
