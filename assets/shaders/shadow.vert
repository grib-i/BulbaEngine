#version 450

layout(location = 0) in vec3 in_position;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
} push_constants;

void main() {
  gl_Position =
    push_constants.mvp *
      vec4(in_position, 1.0);
}
