float point_attenuation(float distance_to_light, float range) {
  if (distance_to_light >= range || range <= 0.0001)
    return 0.0;

  float d = max(distance_to_light, 0.05);

  float inverse_square = 1.0 / (1.0 + d * d * 0.100);

  float fade = 1.0 - smoothstep(range * 0.72, range, distance_to_light);

  return inverse_square * fade * fade;
}

float spot_factor(vec3 direction_to_light, vec3 spot_direction, float inner_cone, float outer_cone) {
  float dl = length(direction_to_light);

  float sl = length(spot_direction);

  if (dl <= 0.0001 || sl <= 0.0001)
    return 0.0;

  float theta = dot(direction_to_light / dl, spot_direction / sl);

  float epsilon = max(inner_cone - outer_cone, 0.0001);

  return clamp((theta - outer_cone) / epsilon, 0.0, 1.0);
}
