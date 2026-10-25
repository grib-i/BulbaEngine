void make_tbn(vec3 n, out vec3 t, out vec3 b) {
  vec3 dp1 = dFdx(in_position);

  vec3 dp2 = dFdy(in_position);

  vec2 duv1 = dFdx(in_uv);

  vec2 duv2 = dFdy(in_uv);

  vec3 fallback_t = normalize(abs(n.z) < 0.999 ? cross(n, vec3(0.0, 0.0, 1.0)) : cross(n, vec3(0.0, 1.0, 0.0)));

  float det = duv1.x * duv2.y - duv1.y * duv2.x;

  if (abs(det) < 0.000001) {
    t = fallback_t;

    b = normalize(cross(n, t));

    return;
  }

  t = normalize((dp1 * duv2.y - dp2 * duv1.y) / det);

  t = normalize(t - n * dot(n, t));

  b = normalize(cross(n, t));
}

vec3 unpack_surface_normal(vec3 n, vec3 t, vec3 b, float normal_scale) {
  if (!has_texture(TEX_NORMAL))
    return normalize(n);

  vec3 tangent_normal = texture(material_textures[TEX_NORMAL], material_uv(TEX_NORMAL)).xyz * 2.0 - 1.0;

  tangent_normal.xy *= normal_scale;

  return normalize(mat3(t, b, n) * tangent_normal);
}
