#version 450

#define MAX_LIGHTS 64

const float LIGHT_POINT = 0.0;
const float LIGHT_DIRECTIONAL = 1.0;
const float LIGHT_SPOT = 2.0;

struct GPULight {
  vec4 direction;
  vec4 position;
  vec4 color;
  vec4 parameters;
  vec4 cone;
};

layout(std430, set = 0, binding = 0) readonly buffer LightBuffer {
  uint light_count;
  uint padding0;
  uint padding1;
  uint padding2;
  vec4 camera_position;
  GPULight lights[MAX_LIGHTS];
  mat4 shadow_mvp;
  vec4 shadow_params;
};

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_position;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  vec4 viewport;
  vec4 material;
} push_constants;

float point_attenuation(float distance_to_light, float range) {
  if (range <= 0.0001 || distance_to_light >= range)
    return 0.0;
  float t = clamp(1.0 - distance_to_light / range, 0.0, 1.0);
  float smooth_t = t * t * (3.0 - 2.0 * t);
  return smooth_t * smooth_t;
}

float spot_factor(vec2 direction_to_light, vec2 spot_direction, float inner_cone, float outer_cone) {
  if (length(direction_to_light) <= 0.0001 || length(spot_direction) <= 0.0001)
    return 0.0;
  float theta = dot(normalize(direction_to_light), normalize(spot_direction));
  float epsilon = max(inner_cone - outer_cone, 0.0001);
  return clamp((theta - outer_cone) / epsilon, 0.0, 1.0);
}

void main() {
  vec3 base = max(in_color.rgb, vec3(0.0));
  float emission = max(push_constants.material.y, 0.0);
  float glow = max(push_constants.material.z, 0.0);

  if (push_constants.material.x < 0.5 || light_count == 0u) {
    vec3 self_lit = base * (1.0 + emission + glow);
    out_color = vec4(min(self_lit, vec3(1.0)), in_color.a);
    return;
  }

  vec3 light_result = base * 0.12;
  vec2 normal = vec2(0.0, -1.0);

  for (uint i = 0u; i < light_count && i < MAX_LIGHTS; i++) {
    GPULight light = lights[i];
    vec3 light_color = max(light.color.rgb, vec3(0.0));
    float intensity = max(light.parameters.x, 0.0);
    float ambient = max(light.parameters.y, 0.0);
    float attenuation = 1.0;
    vec2 direction_to_light = vec2(0.0);

    if (light.cone.w == LIGHT_DIRECTIONAL) {
      direction_to_light = normalize(-light.direction.xy);
    } else {
      vec2 to_light = light.position.xy - in_position;
      float distance_to_light = length(to_light);
      if (distance_to_light <= 0.0001)
        continue;
      direction_to_light = to_light / distance_to_light;
      attenuation = point_attenuation(distance_to_light, max(light.cone.x, 0.0001));
      if (light.cone.w == LIGHT_SPOT)
        attenuation *= spot_factor(-direction_to_light, light.direction.xy, light.cone.y, light.cone.z);
    }

    if (attenuation <= 0.0)
      continue;

    float diffuse = max(dot(normal, direction_to_light), 0.0);
    light_result += base * light_color * (ambient + diffuse * intensity) * attenuation;
  }

  light_result += base * (emission + glow);
  out_color = vec4(min(light_result, vec3(1.0)), in_color.a);
}
