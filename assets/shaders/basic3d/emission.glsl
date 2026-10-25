vec3 blackbody(float kelvin) {
  float k = clamp(kelvin, 1000.0, 40000.0) / 1000.0;

  float r = k <= 6.6 ? 1.0 : clamp(1.29293618662 * pow(k - 6.0, -0.1332047592), 0.0, 1.0);

  float g = k <= 6.6 ? clamp(0.39008157877 * log(k) - 0.63184144315, 0.0, 1.0) : clamp(1.12989086 * pow(k - 6.0, -0.07551485), 0.0, 1.0);

  float b = k >= 6.6 ? 1.0 : (k <= 1.9 ? 0.0 : clamp(0.54320678911 * log(k - 1.0) - 1.19625408914, 0.0, 1.0));

  return vec3(r, g, b);
}
