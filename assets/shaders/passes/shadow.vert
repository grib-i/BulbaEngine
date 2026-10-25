#version 450

layout(location = 0) in vec3 in_position;

struct InstanceData {
  mat4 model;
  vec4 color;
  uint entity_id;
  uint padding0;
  uint padding1;
  uint padding2;
};

layout(std430, set = 0, binding = 8) readonly buffer InstanceBuffer { InstanceData instances[]; };

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  uint flags;
  uint padding[3];
}
push_constants;

void main() {
  vec4 position = vec4(in_position, 1.0);

  if ((push_constants.flags & 1u) != 0u)
    position = instances[push_constants.padding[0] + gl_InstanceIndex].model * position;
  gl_Position = push_constants.mvp * position;
}
