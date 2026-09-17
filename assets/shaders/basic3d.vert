#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_position;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec2 out_uv;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  vec4 model_rows[3];
  vec4 normal_rows[3];
  vec4 material;
} push_constants;

void main() {
  vec4 local_position = vec4(in_position, 1.0);

  vec3 world_position = vec3(
      dot(push_constants.model_rows[0], local_position),
      dot(push_constants.model_rows[1], local_position),
      dot(push_constants.model_rows[2], local_position)
    );

  vec3 world_normal = normalize(vec3(
        dot(push_constants.normal_rows[0], vec4(in_normal, 0.0)),
        dot(push_constants.normal_rows[1], vec4(in_normal, 0.0)),
        dot(push_constants.normal_rows[2], vec4(in_normal, 0.0))
      ));

  gl_Position = push_constants.mvp * local_position;

  out_color = in_color;
  out_position = world_position;
  out_normal = world_normal;
  out_uv = in_uv;
}
