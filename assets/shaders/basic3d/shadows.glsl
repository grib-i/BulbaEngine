float sample_shadow(uint face, vec3 uv_depth) {
  switch (face) {
  case 1u:
    return texture(shadow_map_1, uv_depth);
  case 2u:
    return texture(shadow_map_2, uv_depth);
  case 3u:
    return texture(shadow_map_3, uv_depth);
  case 4u:
    return texture(shadow_map_4, uv_depth);
  case 5u:
    return texture(shadow_map_5, uv_depth);
  case 6u:
    return texture(shadow_map_6, uv_depth);
  default:
    return texture(shadow_map, uv_depth);
  }
}

uint point_shadow_face(vec3 delta) {
  vec3 axis = abs(delta);

  if (axis.x >= axis.y && axis.x >= axis.z)
    return delta.x >= 0.0 ? 1u : 2u;

  if (axis.y >= axis.z)
    return delta.y >= 0.0 ? 3u : 4u;

  return delta.z >= 0.0 ? 5u : 6u;
}

float shadow_visibility(vec3 world_position, vec3 normal, vec3 light_direction, uint light_index) {
  if (shadow_params.x < 0.5 || int(round(shadow_params.z)) != int(light_index))
    return 1.0;

  uint face = shadow_params.x > 1.5 ? point_shadow_face(world_position - lights[light_index].position.xyz) : 0u;
  vec4 shadow_clip = shadow_mvp[face] * vec4(world_position, 1.0);

  if (shadow_clip.w <= 0.0001)
    return 1.0;

  vec3 ndc = shadow_clip.xyz / shadow_clip.w;
  vec2 uv = ndc.xy * 0.5 + 0.5;

  if (any(lessThanEqual(uv, vec2(0.0))) || any(greaterThanEqual(uv, vec2(1.0))) || ndc.z <= 0.0 || ndc.z >= 1.0)
    return 1.0;

  float slope = 1.0 - max(dot(normal, light_direction), 0.0);
  float depth = ndc.z - max(shadow_params.y * (1.0 + slope), 0.0001);
  vec2 texel = 1.0 / vec2(textureSize(shadow_map, 0));
  float visibility = 0.0;

  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x)
      visibility += sample_shadow(face, vec3(uv + vec2(x, y) * texel, depth));
  }

  return visibility / 9.0;
}
