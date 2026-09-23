#version 450

#define MAX_LIGHTS 64
#define TEX_COUNT 17
#define TEX_BASE 0u

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
  mat4 shadow_mvp[7];
  vec4 shadow_params;
};

layout(set = 1, binding = 0) uniform sampler2D material_textures[TEX_COUNT];

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_position;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  vec4 viewport;
  vec4 material;
} push_constants;

float point_attenuation(float distance_to_light, float range) {
  if (distance_to_light >= range || range <= 0.0001)
    return 0.0;

  float d = max(distance_to_light, 0.05);
  float inverse_square =
      1.0 / (1.0 + d * d * 0.08);

  float fade =
      1.0 - smoothstep(
          range * 0.72,
          range,
          distance_to_light
      );

  return inverse_square * fade * fade;
}

float spot_factor(
    vec2 direction_to_light,
    vec2 spot_direction,
    float inner_cone,
    float outer_cone
) {
  float dl = length(direction_to_light);
  float sl = length(spot_direction);

  if (dl <= 0.0001 || sl <= 0.0001)
    return 0.0;

  float theta =
      dot(
          direction_to_light / dl,
          spot_direction / sl
      );

  float epsilon =
      max(
          inner_cone - outer_cone,
          0.0001
      );

  return clamp(
      (theta - outer_cone) / epsilon,
      0.0,
      1.0
  );
}

void main() {
  vec4 texture_color =
      texture(
          material_textures[TEX_BASE],
          in_uv
      );

  vec3 base_color =
      in_color.rgb *
      texture_color.rgb;

  float alpha =
      in_color.a *
      texture_color.a;

  if (alpha <= 0.001)
    discard;

  if (
      push_constants.material.x <= 0.0 ||
      light_count == 0u
  ) {
    out_color =
        vec4(
            base_color,
            alpha
        );
    return;
  }

  vec3 final_color =
      base_color * 0.025;

  vec2 normal =
      vec2(0.0, 1.0);

  for (
      uint i = 0u;
      i < light_count &&
      i < MAX_LIGHTS;
      ++i
  ) {
    GPULight light =
        lights[i];

    vec3 light_color =
        max(
            light.color.rgb,
            vec3(0.0)
        );

    float intensity =
        max(
            light.parameters.x,
            0.0
        );

    float ambient =
        max(
            light.parameters.y,
            0.0
        );

    float specular_strength =
        max(
            light.parameters.z,
            0.0
        );

    float attenuation = 1.0;
    float cone = 1.0;

    vec2 light_direction =
        vec2(0.0);

    if (
        light.cone.w ==
        LIGHT_DIRECTIONAL
    ) {
      light_direction =
          normalize(
              -light.direction.xy
          );
    } else {
      vec2 to_light =
          light.position.xy -
          in_position;

      float distance_to_light =
          length(to_light);

      if (distance_to_light <= 0.0001)
        continue;

      light_direction =
          to_light /
          distance_to_light;

      attenuation =
          point_attenuation(
              distance_to_light,
              max(
                  light.cone.x,
                  0.0001
              )
          );

      if (
          light.cone.w ==
          LIGHT_SPOT
      ) {
        cone =
            spot_factor(
                -light_direction,
                light.direction.xy,
                light.cone.y,
                light.cone.z
            );
      }
    }

    if (attenuation <= 0.0)
      continue;

    float diffuse =
        max(
            dot(
                normal,
                light_direction
            ),
            0.0
        );

    vec3 ambient_color =
        base_color *
        light_color *
        ambient;

    vec3 diffuse_color =
        base_color *
        light_color *
        diffuse *
        intensity *
        attenuation *
        cone;

    vec2 view_direction =
        normalize(
            -in_position
        );

    vec3 specular_color =
        vec3(0.0);

    if (
        diffuse > 0.0 &&
        specular_strength > 0.0
    ) {
      vec2 halfway =
          normalize(
              light_direction +
              view_direction
          );

      float specular =
          pow(
              max(
                  dot(
                      normal,
                      halfway
                  ),
                  0.0
              ),
              16.0
          );

      specular_color =
          light_color *
          specular *
          specular_strength *
          intensity *
          attenuation *
          cone;
    }

    final_color +=
        ambient_color +
        diffuse_color +
        specular_color;
  }

  final_color +=
      base_color *
      max(
          push_constants.material.y,
          0.0
      );

  out_color =
      vec4(
          clamp(
              final_color,
              0.0,
              64.0
          ),
          alpha
      );
}
