vec3 fresnel_schlick(float cos_theta, vec3 f0) {
  float f = pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);

  return f0 + (1.0 - f0) * f;
}

float distribution_ggx_aniso(vec3 n, vec3 h, vec3 t, vec3 b, float roughness, float anisotropy) {
  float hx = dot(h, t);

  float hy = dot(h, b);

  float hz = max(dot(h, n), 0.0);

  float a = max(roughness * roughness, 0.0025);

  float ax = max(a * (1.0 + anisotropy), 0.0025);

  float ay = max(a * (1.0 - anisotropy), 0.0025);

  float v = hx * hx / (ax * ax) + hy * hy / (ay * ay) + hz * hz;

  return 1.0 / max(PI * ax * ay * v * v, 0.000001);
}

float geometry_schlick_ggx(float ndotv, float roughness) {
  float r = roughness + 1.0;

  float k = (r * r) / 8.0;

  return ndotv / max(ndotv * (1.0 - k) + k, 0.000001);
}

float geometry_smith(float ndotv, float ndotl, float roughness) {
  return geometry_schlick_ggx(ndotv, roughness) * geometry_schlick_ggx(ndotl, roughness);
}
