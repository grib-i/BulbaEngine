layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;
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
layout(location = 1) out vec3 out_position;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec2 out_uv;
layout(location = 4) flat out uint out_entity_id;
layout(location = 5) out vec3 out_local_position;
layout(location = 6) out vec3 out_local_normal;

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

void main() {
  vec4 local_position = vec4(in_position, 1.0);

  out_local_position = in_position;

  out_local_normal = in_normal;

  out_uv = in_uv;

  bool skybox = (push_constants.meta.z & (1u << 3u)) != 0u;

  if (skybox) {
    out_color = vec4(1.0);

    out_entity_id = 0u;

    out_position = vec3(in_position.x, in_position.y, 0.0);

    out_normal = vec3(0.0, 0.0, 1.0);

    gl_Position = vec4(in_position.x, in_position.y, 1.0, 1.0);

    return;
  }

  bool instanced = (push_constants.meta.z & 1u) != 0u;

  vec4 world_position4;
  vec3 world_position;
  vec3 world_normal;

  if (instanced) {
    InstanceData instance = instances[push_constants.meta.w + gl_InstanceIndex];

    world_position4 = instance.model * local_position;

    world_position = world_position4.xyz;

    vec3 r0 = vec3(instance.model[0][0], instance.model[1][0], instance.model[2][0]);

    vec3 r1 = vec3(instance.model[0][1], instance.model[1][1], instance.model[2][1]);

    vec3 r2 = vec3(instance.model[0][2], instance.model[1][2], instance.model[2][2]);

    float det = dot(r0, cross(r1, r2));

    if (abs(det) > 0.000001) {
      vec3 n0 = cross(r1, r2) / det;

      vec3 n1 = cross(r2, r0) / det;

      vec3 n2 = cross(r0, r1) / det;

      world_normal = normalize(vec3(dot(n0, in_normal), dot(n1, in_normal), dot(n2, in_normal)));
    } else {
      world_normal = normalize(in_normal);
    }

    out_color = in_color * instance.color;

    out_entity_id = instance.entity_id;
  } else {
    world_position = vec3(dot(push_constants.model_rows[0], local_position), dot(push_constants.model_rows[1], local_position),
                          dot(push_constants.model_rows[2], local_position));

    world_position4 = local_position;

    vec3 r0 = push_constants.model_rows[0].xyz;

    vec3 r1 = push_constants.model_rows[1].xyz;

    vec3 r2 = push_constants.model_rows[2].xyz;

    float det = dot(r0, cross(r1, r2));

    if (abs(det) > 0.000001) {
      vec3 n0 = cross(r1, r2) / det;

      vec3 n1 = cross(r2, r0) / det;

      vec3 n2 = cross(r0, r1) / det;

      world_normal = normalize(vec3(dot(n0, in_normal), dot(n1, in_normal), dot(n2, in_normal)));
    } else {
      world_normal = normalize(in_normal);
    }

    out_color = in_color;

    out_entity_id = push_constants.material_ext.w;
  }

  gl_Position = push_constants.mvp * world_position4;

  out_position = world_position;

  out_normal = world_normal;
}
