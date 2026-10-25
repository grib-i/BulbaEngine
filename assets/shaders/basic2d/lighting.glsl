float point_attenuation(float distance_to_light, float range) {
  if (distance_to_light >= range || range <= 0.0001)
    return 0.0;

  // Normalize by range: screen lights are in pixels, world lights in world units.
  float normalized_distance = distance_to_light / range;
  float fade = 1.0 - smoothstep(0.72, 1.0, normalized_distance);

  return fade * fade / (1.0 + 4.0 * normalized_distance * normalized_distance);
}

float spot_factor(vec2 direction_to_light, vec2 spot_direction, float inner_cone, float outer_cone) {
  float dl = length(direction_to_light);
  float sl = length(spot_direction);

  if (dl <= 0.0001 || sl <= 0.0001)
    return 0.0;

  float theta = dot(direction_to_light / dl, spot_direction / sl);

  float epsilon = max(inner_cone - outer_cone, 0.0001);

  return clamp((theta - outer_cone) / epsilon, 0.0, 1.0);
}

