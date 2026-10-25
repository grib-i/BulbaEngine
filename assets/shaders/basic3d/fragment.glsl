void main() {
  bool skybox = (push_constants.meta.z & (1u << 3u)) != 0u;

  if (skybox) {
    vec2 ndc = in_position.xy;

    vec3 direction =
        normalize(push_constants.model_rows[2].xyz + push_constants.model_rows[0].xyz * ndc.x + push_constants.model_rows[1].xyz * ndc.y);

    bool skybox_is_3d = (push_constants.meta.x & (1u << 31u)) != 0u;

    vec4 sky = vec4(0.0);

    if (skybox_is_3d) {
      vec3 coord = direction * 0.5 + 0.5;

      sky = texture(material_texture_3d, clamp(coord, vec3(0.001), vec3(0.999)));
    } else {
      ivec2 tex_size = textureSize(material_textures[TEX_BASE], 0);

      float texture_aspect = float(tex_size.x) / max(float(tex_size.y), 1.0);

      uint projection = 0u;

      if (abs(texture_aspect - 6.0) < 0.25) {
        projection = 1u;
      } else if (abs(texture_aspect - (1.0 / 6.0)) < 0.02) {
        projection = 2u;
      } else if (abs(texture_aspect - (4.0 / 3.0)) < 0.06) {
        projection = 3u;
      } else if (abs(texture_aspect - (3.0 / 4.0)) < 0.04) {
        projection = 4u;
      } else {
        projection = 0u;
      }

      vec2 uv;

      if (projection == 0u) {
        float u = 0.5 + atan(direction.x, -direction.z) / (2.0 * PI);

        float v = 0.5 + asin(clamp(direction.y, -1.0, 1.0)) / PI;

        uv = vec2(fract(u), clamp(v, 0.001, 0.999));
      } else {
        uv = cross_skybox_uv(direction, projection);

        uv = clamp_cross_uv(uv, projection);
      }

      sky = texture(material_textures[TEX_BASE], uv);
    }

    out_color = vec4(sky.rgb, 1.0);

    return;
  }

  vec3 base = in_color.rgb;

  float alpha = in_color.a;

  bool particle_round = (push_constants.meta.z & (1u << 2u)) != 0u;

  float particle_alpha = 1.0;

  if (particle_round) {
    float radius = length(in_local_position.xy * 2.0);

    float core = 1.0 - smoothstep(0.40, 0.72, radius);

    float halo = 1.0 - smoothstep(0.68, 1.0, radius);

    particle_alpha = max(core, halo * 0.35);

    if (particle_alpha <= 0.001)
      discard;
  }

  bool particle = particle_round;

  bool base_is_3d = (push_constants.meta.x & (1u << 31u)) != 0u;

  if (particle) {
    if (has_texture(TEX_BASE)) {
      vec4 c = texture(material_textures[TEX_BASE], material_uv(TEX_BASE));

      base *= c.rgb;
      alpha *= c.a;
    }
  } else if (base_is_3d) {
    vec3 extent = max(push_constants.uv_bounds_max.xyz - push_constants.uv_bounds_min.xyz, vec3(0.000001));

    vec3 coord = clamp((in_local_position - push_constants.uv_bounds_min.xyz) / extent, vec3(0.0), vec3(1.0));

    vec4 c = texture(material_texture_3d, coord);

    base *= c.rgb;
    alpha *= c.a;
  } else if (has_texture(TEX_BASE)) {
    vec4 c = texture(material_textures[TEX_BASE], material_uv(TEX_BASE));

    base *= c.rgb;
    alpha *= c.a;
  }

  uint flags = push_constants.meta.y;

  bool double_sided = (flags & 1u) != 0u;

  bool unlit = (flags & 2u) != 0u;

  uint alpha_mode = (flags >> 2u) & 3u;

  float alpha_cutoff = float((flags >> 6u) & 1023u) / 1023.0;

  if (alpha_mode == 1u && alpha < alpha_cutoff)
    discard;

  alpha *= particle_alpha;

  if (alpha <= 0.001)
    discard;

  if (unlit || push_constants.material.x < 0.5) {
    vec3 emit_tex = vec3(1.0);

    if (has_texture(TEX_EMISSION)) {
      emit_tex = texture(material_textures[TEX_EMISSION], material_uv(TEX_EMISSION)).rgb;
    }

    vec3 emission_color = push_constants.emission.rgb * emit_tex * blackbody(push_constants.emission.w);

    float emission_strength = max(push_constants.material.y, 0.0);

    vec3 final_unlit = base + emission_color * emission_strength;

    out_color = vec4(min(final_unlit, vec3(64.0)), alpha);

    return;
  }

  vec3 n = normalize(in_normal);

  if (!gl_FrontFacing && double_sided)
    n = -n;

  vec3 t;
  vec3 b;

  make_tbn(n, t, b);

  float metallic = push_constants.pbr.x;

  float roughness = push_constants.pbr.y;

  float specular_factor = push_constants.pbr.z;

  float occlusion = push_constants.pbr.w;

  if (has_texture(TEX_METAL_ROUGH)) {
    vec4 mr = texture(material_textures[TEX_METAL_ROUGH], material_uv(TEX_METAL_ROUGH));

    roughness *= mr.g;

    metallic *= mr.b;
  }

  roughness = clamp(roughness, 0.045, 1.0);

  metallic = clamp(metallic, 0.0, 1.0);

  if (has_texture(TEX_OCCLUSION)) {
    occlusion *= mix(1.0, texture(material_textures[TEX_OCCLUSION], material_uv(TEX_OCCLUSION)).r, push_constants.pbr.w);
  }

  occlusion = clamp(occlusion, 0.0, 1.0);

  float normal_scale = decode01(unpack_pair_low(surface_word(0)), 0.0, 4.0);

  n = unpack_surface_normal(n, t, b, normal_scale);

  float ior = decode01(unpack_pair_high(surface_word(0)), 1.0, 4.0);

  float transmission = decode01(unpack_pair_low(surface_word(1)), 0.0, 1.0);

  float volume_thickness = decode01(unpack_pair_high(surface_word(1)), 0.0, 100.0);

  float attenuation_distance = decode01(unpack_pair_low(surface_word(2)), 0.0, 1000.0);

  float clearcoat = decode01(unpack_pair_high(surface_word(2)), 0.0, 1.0);

  float clearcoat_roughness = decode01(unpack_pair_low(surface_word(3)), 0.0, 1.0);

  float clearcoat_normal_scale = decode01(unpack_pair_high(surface_word(3)), 0.0, 4.0);

  float sheen_roughness = decode01(unpack_pair_low(surface_word(4)), 0.0, 1.0);

  float iridescence = decode01(unpack_pair_high(surface_word(4)), 0.0, 1.0);

  float iridescence_ior = decode01(unpack_pair_low(surface_word(5)), 1.0, 4.0);

  float iridescence_min = decode01(unpack_pair_high(surface_word(5)), 0.0, 2000.0);

  float iridescence_max = decode01(unpack_pair_low(surface_word(6)), 0.0, 2000.0);

  float anisotropy = decode01(unpack_pair_high(surface_word(6)), 0.0, 1.0);

  float anisotropy_rotation = decode01(unpack_pair_low(surface_word(7)), -PI, PI);

  float dispersion = decode01(unpack_pair_high(surface_word(7)), 0.0, 1.0);

  if (has_texture(TEX_SPECULAR)) {
    specular_factor *= texture(material_textures[TEX_SPECULAR], material_uv(TEX_SPECULAR)).r;
  }

  vec3 specular_color = unpack_specular_color();

  if (has_texture(TEX_SPECULAR_COLOR)) {
    specular_color *= texture(material_textures[TEX_SPECULAR_COLOR], material_uv(TEX_SPECULAR_COLOR)).rgb;
  }

  float clearcoat_map = clearcoat;

  float clearcoat_rough_map = clearcoat_roughness;

  if (has_texture(TEX_CLEARCOAT)) {
    clearcoat_map *= texture(material_textures[TEX_CLEARCOAT], material_uv(TEX_CLEARCOAT)).r;
  }

  if (has_texture(TEX_CLEARCOAT_ROUGH)) {
    clearcoat_rough_map *= texture(material_textures[TEX_CLEARCOAT_ROUGH], material_uv(TEX_CLEARCOAT_ROUGH)).r;
  }

  clearcoat_map = clamp(clearcoat_map, 0.0, 1.0);

  clearcoat_rough_map = clamp(clearcoat_rough_map, 0.03, 1.0);

  vec3 coat_n = n;

  if (has_texture(TEX_CLEARCOAT_NORMAL)) {
    vec3 coat_ts = texture(material_textures[TEX_CLEARCOAT_NORMAL], material_uv(TEX_CLEARCOAT_NORMAL)).xyz * 2.0 - 1.0;

    coat_ts.xy *= clearcoat_normal_scale;

    coat_n = normalize(mat3(t, b, n) * coat_ts);
  }

  float sheen_r = sheen_roughness;

  vec3 sheen_c = unpack_sheen_color();

  if (has_texture(TEX_SHEEN_COLOR)) {
    sheen_c *= texture(material_textures[TEX_SHEEN_COLOR], material_uv(TEX_SHEEN_COLOR)).rgb;
  }

  if (has_texture(TEX_SHEEN_ROUGH)) {
    sheen_r *= texture(material_textures[TEX_SHEEN_ROUGH], material_uv(TEX_SHEEN_ROUGH)).r;
  }

  sheen_r = clamp(sheen_r, 0.0, 1.0);

  if (has_texture(TEX_IRIDESCENCE)) {
    iridescence *= texture(material_textures[TEX_IRIDESCENCE], material_uv(TEX_IRIDESCENCE)).r;
  }

  float film_thickness = mix(iridescence_min, iridescence_max, 0.5);

  if (has_texture(TEX_IRIDESCENCE_THICKNESS)) {
    film_thickness =
        mix(iridescence_min, iridescence_max, texture(material_textures[TEX_IRIDESCENCE_THICKNESS], material_uv(TEX_IRIDESCENCE_THICKNESS)).r);
  }

  float aniso_strength = anisotropy;

  float aniso_rotation = anisotropy_rotation;

  if (has_texture(TEX_ANISOTROPY)) {
    vec2 atex = texture(material_textures[TEX_ANISOTROPY], material_uv(TEX_ANISOTROPY)).rg;

    aniso_strength *= atex.r;

    aniso_rotation += (atex.g * 2.0 - 1.0) * PI;
  }

  aniso_strength = clamp(aniso_strength, 0.0, 0.95);

  vec3 anis_t = normalize(t * cos(aniso_rotation) + b * sin(aniso_rotation));

  vec3 anis_b = normalize(cross(n, anis_t));

  float ior_f0 = pow((ior - 1.0) / max(ior + 1.0, 0.0001), 2.0);

  vec3 f0 = mix(vec3(ior_f0 * specular_factor) * specular_color, base, metallic);

  vec3 view_dir = normalize(camera_position.xyz - in_position);

  float ndotv = max(dot(n, view_dir), 0.0);

  vec3 diffuse_light = vec3(0.0);

  vec3 specular_light = vec3(0.0);

  vec3 transmission_light = vec3(0.0);

  float ambient_light = 0.025;

  vec3 attenuation_color = unpack_attenuation_color();

  float transmission_map = transmission;

  if (has_texture(TEX_TRANSMISSION)) {
    transmission_map *= texture(material_textures[TEX_TRANSMISSION], material_uv(TEX_TRANSMISSION)).r;
  }

  float thickness = volume_thickness;

  if (has_texture(TEX_THICKNESS)) {
    thickness *= texture(material_textures[TEX_THICKNESS], material_uv(TEX_THICKNESS)).r;
  }

  float volume_tint_amount = attenuation_distance > 0.0001 && attenuation_distance < 999.9 ? clamp(thickness / attenuation_distance, 0.0, 50.0) : 0.0;

  vec3 volume_tint = exp(log(max(attenuation_color, vec3(0.0001))) * volume_tint_amount);

  vec3 iridescent_f0 = f0;

  if (iridescence > 0.001) {
    float phase = 2.0 * PI * (iridescence_ior - 1.0) * max(film_thickness, 1.0) * 0.001 * ndotv;

    vec3 film = 0.5 + 0.5 * cos(vec3(phase, phase + 2.0943951, phase + 4.1887902));

    iridescent_f0 = mix(f0, film, clamp(iridescence, 0.0, 1.0));
  }

  for (uint i = 0u; i < light_count && i < MAX_LIGHTS; ++i) {
    GPULight light = lights[i];

    uint entity_id = in_entity_id;

    if (floatBitsToUint(light.parameters.w) != 0u && floatBitsToUint(light.parameters.w) == entity_id)
      continue;

    vec3 light_color = max(light.color.rgb, vec3(0.0));

    float intensity = max(light.parameters.x, 0.0);

    float ambient = max(light.parameters.y, 0.0);

    float attenuation = 1.0;

    vec3 light_dir = vec3(0.0);

    bool casts_shadow = int(round(shadow_params.z)) == int(i);

    if (light.cone.w == LIGHT_DIRECTIONAL) {
      float direction_length = length(light.direction.xyz);

      if (direction_length <= 0.0001)
        continue;

      light_dir = -light.direction.xyz / direction_length;

    } else {
      vec3 to_light = light.position.xyz - in_position;

      float distance_to_light = length(to_light);

      if (distance_to_light <= 0.0001)
        continue;

      light_dir = to_light / distance_to_light;

      attenuation = point_attenuation(distance_to_light, max(light.cone.x, 0.0001));

      if (light.cone.w == LIGHT_SPOT) {
        attenuation *= spot_factor(-light_dir, light.direction.xyz, light.cone.y, light.cone.z);
      }
    }

    if (attenuation <= 0.0)
      continue;

    float ndotl = max(dot(n, light_dir), 0.0);

    float shadow = 1.0;

    if (casts_shadow && ndotl > 0.0) {
      shadow = shadow_visibility(in_position, n, light_dir, i);
    }

    ambient_light += ambient * intensity * attenuation;

    if (ndotl <= 0.0)
      continue;

    vec3 half_dir = normalize(view_dir + light_dir);

    float vdoth = max(dot(view_dir, half_dir), 0.0);

    float D = distribution_ggx_aniso(n, half_dir, anis_t, anis_b, roughness, aniso_strength);

    float G = geometry_smith(ndotv, ndotl, roughness);

    vec3 F = fresnel_schlick(vdoth, iridescent_f0);

    vec3 spec = D * G * F / max(4.0 * ndotv * ndotl, 0.0001);

    vec3 kd = (1.0 - F) * (1.0 - metallic);

    diffuse_light += light_color * intensity * attenuation * shadow * kd * base * ndotl / PI;

    specular_light += light_color * intensity * attenuation * shadow * spec * ndotl;

    if (clearcoat_map > 0.001) {
      float c_ndotl = max(dot(coat_n, light_dir), 0.0);

      float c_ndotv = max(dot(coat_n, view_dir), 0.0);

      vec3 c_h = normalize(view_dir + light_dir);

      float c_vdoth = max(dot(view_dir, c_h), 0.0);

      float c_d = distribution_ggx_aniso(coat_n, c_h, anis_t, anis_b, clearcoat_rough_map, 0.0);

      float c_g = geometry_smith(c_ndotv, c_ndotl, clearcoat_rough_map);

      vec3 c_f = fresnel_schlick(c_vdoth, vec3(0.04));

      specular_light +=
          light_color * intensity * attenuation * shadow * c_d * c_g * c_f * clearcoat_map * c_ndotl / max(4.0 * c_ndotv * c_ndotl, 0.0001);
    }

    if (length(sheen_c) > 0.001) {
      float sheen_term = pow(max(1.0 - ndotv, 0.0), 5.0) * (1.0 - 0.5 * sheen_r);

      diffuse_light += light_color * intensity * attenuation * shadow * sheen_c * sheen_term * 0.35 * ndotl;
    }

    if (transmission_map > 0.001) {
      vec3 refracted_tint = volume_tint;

      float backface = pow(1.0 - ndotl, 2.0);

      vec3 spectral = mix(vec3(1.0), vec3(1.0 + dispersion * 0.25, 1.0, 1.0 - dispersion * 0.25), dispersion);

      transmission_light += light_color * intensity * attenuation * shadow * transmission_map * backface * refracted_tint * spectral * 0.5;
    }
  }

  vec3 emit_tex = vec3(1.0);

  if (has_texture(TEX_EMISSION)) {
    emit_tex = texture(material_textures[TEX_EMISSION], material_uv(TEX_EMISSION)).rgb;
  }

  vec3 emission_color = push_constants.emission.rgb * emit_tex * blackbody(push_constants.emission.w);

  float emission_strength = max(push_constants.material.y, 0.0);

  vec3 ambient = base * ambient_light * occlusion * (1.0 - metallic);

  vec3 color = ambient + diffuse_light * (1.0 - transmission_map) + specular_light + transmission_light + emission_color * emission_strength;

  if (push_constants.material.z > 0.0) {
    color += emission_color * push_constants.material.z * 0.05;
  }

  float output_alpha = alpha;

  if (transmission_map > 0.001 && alpha_mode == 2u) {
    output_alpha *= max(0.05, 1.0 - transmission_map * 0.85);
  }

  out_color = vec4(min(max(color, vec3(0.0)), vec3(64.0)), output_alpha);
}
