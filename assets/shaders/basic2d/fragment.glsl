#include "basic2d/interface.glsl"
#include "basic2d/lighting.glsl"

void main() {
  bool particle_round = (push_constants.meta.z & (1u << 2u)) != 0u;
  float particle_alpha = 1.0;

  if (particle_round) {
    float radius = length(in_uv * 2.0 - 1.0);
    float core = 1.0 - smoothstep(0.40, 0.72, radius);
    float halo = 1.0 - smoothstep(0.68, 1.0, radius);
    particle_alpha = max(core, halo * 0.35);

    if (particle_alpha <= 0.001)
      discard;
  }

  vec4 texture_color = vec4(1.0);

  if (has_texture(TEX_BASE))
    texture_color = texture(material_textures[TEX_BASE], in_uv);

  vec3 base_color = in_color.rgb * texture_color.rgb;

  float alpha = in_color.a * texture_color.a * particle_alpha;

  uint alpha_mode = (push_constants.meta.y >> 2u) & 3u;
  float alpha_cutoff = float((push_constants.meta.y >> 6u) & 1023u) / 1023.0;

  if (alpha_mode == 1u && alpha < alpha_cutoff)
    discard;

  if (alpha <= 0.001)
    discard;

  if (push_constants.material.x <= 0.0 || light_count == 0u) {
    out_color = vec4(base_color, alpha);

    return;
  }

  vec3 final_color = base_color * 0.025;

  bool screen_space = (push_constants.meta.z & 2u) != 0u;

  for (uint i = 0u; i < min(light_count, uint(MAX_LIGHTS)); ++i) {
    GPULight light = lights[i];

    // Do not mix a HUD light's pixel position with a world-space object's position.
    if ((light.position.w > 0.5) != screen_space)
      continue;

    vec3 light_color = max(light.color.rgb, vec3(0.0));
    float intensity = max(light.parameters.x, 0.0);
    float attenuation = 1.0;
    float cone = 1.0;
    float diffuse = 1.0;
    float specular = 0.0;

    if (light.cone.w != LIGHT_DIRECTIONAL) {
      vec2 offset = light.position.xy - in_position;
      float distance_to_light = length(offset);
      float range = max(light.cone.x, 0.0001);
      float elevation = max(range * 0.25, 0.0001);
      vec3 direction = normalize(vec3(offset, elevation));

      attenuation = point_attenuation(distance_to_light, range);
      diffuse = direction.z; // The sprite is a plane facing the viewer, not a vertical line.
      specular = pow(normalize(direction + vec3(0.0, 0.0, 1.0)).z, 32.0);

      if (light.cone.w == LIGHT_SPOT && distance_to_light > 0.0001)
        cone = spot_factor(-offset, light.direction.xy, light.cone.y, light.cone.z);
    }

    final_color += base_color * light_color * max(light.parameters.y, 0.0);
    final_color += light_color * intensity * attenuation * cone *
                   (base_color * diffuse + specular * max(light.parameters.z, 0.0));
  }

  final_color += base_color * max(push_constants.material.y, 0.0);

  out_color = vec4(clamp(final_color, 0.0, 64.0), alpha);
}
