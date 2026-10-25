bool has_texture(uint slot) { return (push_constants.meta.x & (1u << slot)) != 0u; }

uint material_texture_projection(uint slot) {
  uint packed = material_buffer.textures[slot].packed[2];

  return (packed >> 24u) & 255u;
}

vec2 texture_offset(uint slot) { return unpackHalf2x16(material_buffer.textures[slot].packed[0]); }

vec2 texture_scale(uint slot) { return unpackHalf2x16(material_buffer.textures[slot].packed[1]); }

float texture_rotation(uint slot) { return unpackHalf2x16(material_buffer.textures[slot].packed[2]).x; }

vec2 material_uv(uint slot) {
  vec2 offset = texture_offset(slot);

  vec2 scale = texture_scale(slot);

  float rotation = texture_rotation(slot);

  uint projection = material_texture_projection(slot);

  vec2 uv = in_uv;

  bool authored_uv = slot == TEX_NORMAL || slot == TEX_CLEARCOAT_NORMAL;

  if (authored_uv)
    return uv;

  if (projection == 5u) {
    uv = in_uv;
  } else if (projection == 0u) {
    vec3 extent = max(push_constants.uv_bounds_max.xyz - push_constants.uv_bounds_min.xyz, vec3(0.000001));

    vec3 normalized = clamp((in_local_position - push_constants.uv_bounds_min.xyz) / extent, vec3(0.0), vec3(1.0));

    vec3 an = abs(normalize(in_local_normal));

    if (an.x >= an.y && an.x >= an.z) {
      uv = normalized.zy;
    } else if (an.y >= an.z) {
      uv = normalized.xz;
    } else {
      uv = normalized.xy;
    }
  } else {
    vec3 extent = max(push_constants.uv_bounds_max.xyz - push_constants.uv_bounds_min.xyz, vec3(0.000001));

    vec3 normalized = clamp((in_local_position - push_constants.uv_bounds_min.xyz) / extent, vec3(0.0), vec3(1.0));

    vec3 n = normalize(in_local_normal);

    vec2 face_uv;
    uint face;

    if (abs(n.x) >= abs(n.y) && abs(n.x) >= abs(n.z)) {
      face = n.x > 0.0 ? 1u : 3u;

      face_uv = vec2(n.x > 0.0 ? 1.0 - normalized.z : normalized.z, normalized.y);
    } else if (abs(n.y) >= abs(n.z)) {
      face = n.y > 0.0 ? 4u : 5u;

      face_uv = vec2(normalized.x, n.y > 0.0 ? normalized.z : 1.0 - normalized.z);
    } else {
      face = n.z > 0.0 ? 2u : 0u;

      face_uv = vec2(n.z > 0.0 ? 1.0 - normalized.x : normalized.x, normalized.y);
    }

    if (projection == 1u) {
      uv = vec2((float(face) + face_uv.x) / 6.0, face_uv.y);
    } else if (projection == 2u) {
      uv = vec2(face_uv.x, (float(face) + face_uv.y) / 6.0);
    } else if (projection == 3u) {
      uint column = 0u;
      uint row = 0u;

      if (face == 0u) {
        column = 3u;
        row = 1u;
      } else if (face == 1u) {
        column = 2u;
        row = 1u;
      } else if (face == 2u) {
        column = 1u;
        row = 1u;
      } else if (face == 3u) {
        column = 0u;
        row = 1u;
      } else if (face == 4u) {
        column = 1u;
        row = 0u;
      } else {
        column = 1u;
        row = 2u;
      }

      uv = vec2((float(column) + face_uv.x) / 4.0, (float(row) + face_uv.y) / 3.0);
    } else {
      uint column = 0u;
      uint row = 0u;

      if (face == 4u) {
        column = 1u;
        row = 0u;
      } else if (face == 3u) {
        column = 0u;
        row = 1u;
      } else if (face == 2u) {
        column = 1u;
        row = 1u;
      } else if (face == 1u) {
        column = 2u;
        row = 1u;
      } else if (face == 0u) {
        column = 1u;
        row = 2u;
      } else {
        column = 1u;
        row = 3u;
      }

      uv = vec2((float(column) + face_uv.x) / 3.0, (float(row) + face_uv.y) / 4.0);
    }
  }

  float c = cos(rotation);

  float s = sin(rotation);

  uv = mat2(c, -s, s, c) * ((uv - vec2(0.5)) * scale) + vec2(0.5) + offset;

  return uv;
}

vec2 cross_skybox_uv(vec3 direction, uint projection) {
  direction = normalize(direction);

  uint face = 0u;
  vec2 face_uv;

  if (abs(direction.x) >= abs(direction.y) && abs(direction.x) >= abs(direction.z)) {
    if (direction.x > 0.0) {
      face = 1u;

      face_uv = vec2((1.0 - direction.z) * 0.5, (direction.y + 1.0) * 0.5);
    } else {
      face = 3u;

      face_uv = vec2((direction.z + 1.0) * 0.5, (direction.y + 1.0) * 0.5);
    }
  } else if (abs(direction.y) >= abs(direction.z)) {
    if (direction.y > 0.0) {
      face = 4u;

      face_uv = vec2((direction.x + 1.0) * 0.5, (direction.z + 1.0) * 0.5);
    } else {
      face = 5u;

      face_uv = vec2((direction.x + 1.0) * 0.5, (1.0 - direction.z) * 0.5);
    }
  } else {
    if (direction.z > 0.0) {
      face = 2u;

      face_uv = vec2((1.0 - direction.x) * 0.5, (direction.y + 1.0) * 0.5);
    } else {
      face = 0u;

      face_uv = vec2((direction.x + 1.0) * 0.5, (direction.y + 1.0) * 0.5);
    }
  }

  if (projection == 1u) {
    return vec2((float(face) + face_uv.x) / 6.0, face_uv.y);
  }

  if (projection == 2u) {
    return vec2(face_uv.x, (float(face) + face_uv.y) / 6.0);
  }

  if (projection == 3u) {
    uint column = 0u;
    uint row = 0u;

    if (face == 0u) {
      column = 3u;
      row = 1u;
    } else if (face == 1u) {
      column = 2u;
      row = 1u;
    } else if (face == 2u) {
      column = 1u;
      row = 1u;
    } else if (face == 3u) {
      column = 0u;
      row = 1u;
    } else if (face == 4u) {
      column = 1u;
      row = 0u;
    } else {
      column = 1u;
      row = 2u;
    }

    return vec2((float(column) + face_uv.x) / 4.0, (float(row) + face_uv.y) / 3.0);
  }

  uint column = 0u;
  uint row = 0u;

  if (face == 4u) {
    column = 1u;
    row = 0u;
  } else if (face == 3u) {
    column = 0u;
    row = 1u;
  } else if (face == 2u) {
    column = 1u;
    row = 1u;
  } else if (face == 1u) {
    column = 2u;
    row = 1u;
  } else if (face == 0u) {
    column = 1u;
    row = 2u;
  } else {
    column = 1u;
    row = 3u;
  }

  return vec2((float(column) + face_uv.x) / 3.0, (float(row) + face_uv.y) / 4.0);
}

vec2 clamp_cross_uv(vec2 uv, uint projection) {
  ivec2 size = textureSize(material_textures[TEX_BASE], 0);

  float width = float(size.x);

  float height = float(size.y);

  float cells_x;
  float cells_y;

  if (projection == 1u) {
    cells_x = 6.0;
    cells_y = 1.0;
  } else if (projection == 2u) {
    cells_x = 1.0;
    cells_y = 6.0;
  } else if (projection == 3u) {
    cells_x = 4.0;
    cells_y = 3.0;
  } else {
    cells_x = 3.0;
    cells_y = 4.0;
  }

  float cell_w = 1.0 / cells_x;

  float cell_h = 1.0 / cells_y;

  float texel_x = 0.5 / width;

  float texel_y = 0.5 / height;

  float cell_x = floor(uv.x / cell_w);

  float cell_y = floor(uv.y / cell_h);

  float local_x = fract(uv.x / cell_w);

  float local_y = fract(uv.y / cell_h);

  float min_x = texel_x / cell_w;

  float min_y = texel_y / cell_h;

  local_x = clamp(local_x, min_x, 1.0 - min_x);

  local_y = clamp(local_y, min_y, 1.0 - min_y);

  return vec2((cell_x + local_x) * cell_w, (cell_y + local_y) * cell_h);
}
