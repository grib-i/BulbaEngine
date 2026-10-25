vec3 unpack_rgb10(uint packed) { return vec3(float(packed & 1023u), float((packed >> 10u) & 1023u), float((packed >> 20u) & 1023u)) / 1023.0; }

float unpack_pair_low(uint packed) { return float(packed & 65535u) / 65535.0; }

float unpack_pair_high(uint packed) { return float((packed >> 16u) & 65535u) / 65535.0; }

uint surface_word(uint index) {
  uint group = index >> 2u;

  uint lane = index & 3u;

  return group == 0u ? push_constants.surface[0][lane] : push_constants.surface[1][lane];
}

float decode01(float packed, float min_value, float max_value) { return mix(min_value, max_value, packed); }
