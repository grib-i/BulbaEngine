#include "common/lights.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_world_position;
layout(location = 3) in vec2 in_uv;

struct InstanceData {
  mat4 model;
  vec4 color;
  uint entity_id;
  uint padding0;
  uint padding1;
  uint padding2;
};

layout(std430, set = 0, binding = 8) readonly buffer InstanceBuffer { InstanceData instances[]; };

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec2 out_position;

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

void main() {
  float width = max(push_constants.viewport.x, 1.0);
  float height = max(push_constants.viewport.y, 1.0);
  bool instanced = (push_constants.meta.z & 1u) != 0u;
  bool camera_attached = (push_constants.meta.z & 2u) != 0u;
  bool baked = (push_constants.meta.z & 8u) != 0u;

  vec2 pixel_position;
  vec2 world_position = in_world_position.xy;
  vec4 instance_color = vec4(1.0);

  if (instanced) {
    InstanceData instance = instances[push_constants.meta.w + gl_InstanceIndex];
    vec4 transformed = instance.model * vec4(in_position, 1.0);
    world_position = transformed.xy;
    instance_color = instance.color;

    if (camera_attached) {
      pixel_position = transformed.xy;
      vec2 clip = vec2((pixel_position.x / width) * 2.0 - 1.0, (pixel_position.y / height) * 2.0 - 1.0);
      gl_Position = vec4(clip, 0.0, 1.0);
    } else {
      gl_Position = view_projection * transformed;
    }
  } else if (baked) {
    world_position = in_world_position.xy;

    if (camera_attached) {
      pixel_position = in_position.xy;
      vec2 clip = vec2((pixel_position.x / width) * 2.0 - 1.0, (pixel_position.y / height) * 2.0 - 1.0);
      gl_Position = vec4(clip, 0.0, 1.0);
    } else {
      gl_Position = view_projection * vec4(in_position, 1.0);
    }
  } else {
    pixel_position = in_position.xy;
    vec2 clip = vec2((pixel_position.x / width) * 2.0 - 1.0, (pixel_position.y / height) * 2.0 - 1.0);
    gl_Position = vec4(clip, 0.0, 1.0);
  }

  out_color = in_color * instance_color;
  out_uv = in_uv;

  if ((instanced || baked) && !camera_attached)
    out_uv.y = 1.0 - out_uv.y;
  out_position = world_position;
}
