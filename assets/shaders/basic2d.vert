#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_position;

layout(push_constant) uniform PushConstants {
  vec4 viewport;
  vec4 material;
} push_constants;

void main() {
  float width = max(push_constants.viewport.x, 1.0);
  float height = max(push_constants.viewport.y, 1.0);

  vec2 clip = vec2(
      (in_position.x / width) * 2.0 - 1.0,
      (in_position.y / height) * 2.0 - 1.0
  );

  gl_Position = vec4(clip, 0.0, 1.0);
  out_color = in_color;
  out_position = in_position.xy;
}
