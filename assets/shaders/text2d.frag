#version 450

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;

layout(binding = 0) uniform sampler2D font_texture;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  vec4 viewport;
  vec4 material;
} push_constants;

void main() {
  float coverage = texture(font_texture, in_uv).r;
  if (coverage <= 0.002)
    discard;

  float self_light = max(push_constants.material.x + push_constants.material.y, 0.0);
  vec3 color = min(in_color.rgb * (1.0 + self_light), vec3(1.0));
  out_color = vec4(color, in_color.a * coverage);
}
