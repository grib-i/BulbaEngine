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

layout(set = 0, binding = 1) uniform sampler2DShadow shadow_map;

layout(set = 1, binding = 0) uniform sampler2D diffuse_texture;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_position;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  vec4 model_rows[3];
  vec4 normal_rows[3];
  vec4 material;
} push_constants;

float point_attenuation(float distance_to_light, float range) {
  if (distance_to_light >= range || range <= 0.0001)
    return 0.0;

  float d = max(distance_to_light, 0.05);
  float inverse_square = 1.0 / (1.0 + d * d * 0.055);
  float fade = 1.0 - smoothstep(range * 0.72, range, distance_to_light);

  return inverse_square * fade * fade;
}

float spot_factor(
  vec3 direction_to_light,
  vec3 spot_direction,
  float inner_cone,
  float outer_cone
) {
  float direction_length = length(direction_to_light);
  float spot_length = length(spot_direction);

  if (direction_length <= 0.0001 || spot_length <= 0.0001)
    return 0.0;

  float theta = dot(
      direction_to_light / direction_length,
      spot_direction / spot_length
    );

  float epsilon = max(inner_cone - outer_cone, 0.0001);

  return clamp(
    (theta - outer_cone) / epsilon,
    0.0,
    1.0
  );
}

float shadow_visibility(
  vec3 world_position,
  vec3 normal,
  vec3 light_direction
) {
  if (shadow_params.x < 0.5)
    return 1.0;

  vec4 shadow_clip =
    shadow_mvp * vec4(world_position, 1.0);

  if (shadow_clip.w <= 0.0001)
    return 1.0;

  vec3 shadow_ndc =
    shadow_clip.xyz / shadow_clip.w;

  vec2 shadow_uv =
    shadow_ndc.xy * 0.5 + 0.5;

  shadow_uv.y =
    1.0 - shadow_uv.y;

  if (
    shadow_uv.x <= 0.0 ||
      shadow_uv.x >= 1.0 ||
      shadow_uv.y <= 0.0 ||
      shadow_uv.y >= 1.0 ||
      shadow_ndc.z <= 0.0 ||
      shadow_ndc.z >= 1.0
  )
    return 1.0;

  float slope =
    1.0 - max(dot(normal, light_direction), 0.0);

  float bias =
    max(
      shadow_params.y +
        slope * shadow_params.z,
      0.0001
    );

  float depth =
    shadow_ndc.z - bias;

  vec2 texel =
    1.0 / vec2(textureSize(shadow_map, 0));

  float visibility = 0.0;

  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      visibility += texture(
          shadow_map,
          vec3(
            shadow_uv +
              vec2(x, y) * texel,
            depth
          )
        );
    }
  }

  return visibility / 9.0;
}

void main() {
  vec4 texture_color =
    texture(diffuse_texture, in_uv);

  vec4 surface_color =
    texture_color * in_color;

  vec3 base =
    max(surface_color.rgb, vec3(0.0));

  float alpha =
    surface_color.a;

  float lighting_enabled =
    push_constants.material.x;

  float emission =
    max(push_constants.material.y, 0.0);

  float glow =
    max(push_constants.material.z, 0.0);

  if (lighting_enabled < 0.5) {
    vec3 self_lit =
      base * (1.0 + emission + glow);

    out_color = vec4(
        min(self_lit, vec3(1.0)),
        alpha
      );

    return;
  }

  vec3 normal =
    normalize(in_normal);

  vec3 diffuse_light =
    vec3(0.0);

  float ambient_light =
    0.12;

  for (uint i = 0u;
    i < light_count && i < MAX_LIGHTS;
    i++) {
    GPULight light =
      lights[i];

    vec3 light_color =
      max(light.color.rgb, vec3(0.0));

    float intensity =
      max(light.parameters.x, 0.0);

    float ambient =
      max(light.parameters.y, 0.0);

    float attenuation =
      1.0;

    vec3 direction_to_light =
      vec3(0.0);

    bool casts_shadow =
      false;

    if (light.cone.w == LIGHT_DIRECTIONAL) {
      direction_to_light =
        normalize(-light.direction.xyz);

      casts_shadow =
        true;
    } else if (
      light.cone.w == LIGHT_POINT ||
        light.cone.w == LIGHT_SPOT
    ) {
      vec3 to_light =
        light.position.xyz - in_position;

      float distance_to_light =
        length(to_light);

      if (distance_to_light <= 0.0001)
        continue;

      direction_to_light =
        to_light / distance_to_light;

      attenuation =
        point_attenuation(
          distance_to_light,
          max(light.cone.x, 0.0001)
        );

      if (light.cone.w == LIGHT_SPOT) {
        attenuation *=
          spot_factor(
            -direction_to_light,
            light.direction.xyz,
            light.cone.y,
            light.cone.z
          );
      }
    }

    if (attenuation <= 0.0)
      continue;

    float ndotl =
      max(
        dot(normal, direction_to_light),
        0.0
      );

    float diffuse =
      ndotl * ndotl *
        (3.0 - 2.0 * ndotl);

    float shadow =
      casts_shadow && diffuse > 0.0
      ? shadow_visibility(
        in_position,
        normal,
        direction_to_light
      ) : 1.0;

    ambient_light +=
      ambient *
        intensity *
        attenuation;

    diffuse_light +=
      light_color *
        intensity *
        diffuse *
        attenuation *
        shadow;
  }

  vec3 lit_color =
    base *
      (ambient_light + diffuse_light);

  vec3 self_lit =
    base *
      (emission + glow);

  vec3 final_color =
    lit_color + self_lit;

  out_color = vec4(
      min(final_color, vec3(1.0)),
      alpha
    );
}
