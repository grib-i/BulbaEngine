#version 450

#define MAX_LIGHTS 64
#define SHADOW_MAP_COUNT 7
#define TEX_COUNT 17

const uint TEX_BASE = 0u;
const uint TEX_METAL_ROUGH = 1u;
const uint TEX_NORMAL = 2u;
const uint TEX_OCCLUSION = 3u;
const uint TEX_EMISSION = 4u;
const uint TEX_SPECULAR = 5u;
const uint TEX_SPECULAR_COLOR = 6u;
const uint TEX_CLEARCOAT = 7u;
const uint TEX_CLEARCOAT_ROUGH = 8u;
const uint TEX_CLEARCOAT_NORMAL = 9u;
const uint TEX_TRANSMISSION = 10u;
const uint TEX_THICKNESS = 11u;
const uint TEX_SHEEN_COLOR = 12u;
const uint TEX_SHEEN_ROUGH = 13u;
const uint TEX_IRIDESCENCE = 14u;
const uint TEX_IRIDESCENCE_THICKNESS = 15u;
const uint TEX_ANISOTROPY = 16u;

const float LIGHT_POINT = 0.0;
const float LIGHT_DIRECTIONAL = 1.0;
const float LIGHT_SPOT = 2.0;
const float PI = 3.14159265359;

struct GPULight {
  vec4 direction;
  vec4 position;
  vec4 color;
  vec4 parameters;
  vec4 cone;
};

struct MaterialTextureParams {
  uint packed[3];
};

layout(std430, set = 0, binding = 0) readonly buffer LightBuffer {
  uint light_count;
  uint padding0;
  uint padding1;
  uint padding2;
  vec4 camera_position;
  GPULight lights[MAX_LIGHTS];
  mat4 shadow_mvp[SHADOW_MAP_COUNT];
  vec4 shadow_params;
};

layout(set = 0, binding = 1) uniform sampler2DShadow shadow_map;
layout(set = 1, binding = 0) uniform sampler2D material_textures[TEX_COUNT];
layout(std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
  MaterialTextureParams textures[TEX_COUNT];
} material_buffer;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec3 in_position;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  mat4 mvp;
  vec4 model_rows[3];
  vec4 material;
  vec4 pbr;
  vec4 emission;
  uvec4 material_ext;
  uvec4 surface[2];
  uvec4 meta;
} push_constants;

bool has_texture(uint slot) {
  return (push_constants.meta.x & (1u << slot)) != 0u;
}

vec2 material_uv(uint slot) {
  vec2 offset = unpackHalf2x16(material_buffer.textures[slot].packed[0]);
  vec2 scale = unpackHalf2x16(material_buffer.textures[slot].packed[1]);
  float rotation = unpackHalf2x16(material_buffer.textures[slot].packed[2]).x;
  vec2 uv = (in_uv - vec2(0.5)) * scale;
  float s = sin(rotation);
  float c = cos(rotation);
  uv = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c) + vec2(0.5) + offset;
  return uv;
}

vec3 unpack_rgb10(uint packed) {
  return vec3(
    float(packed & 1023u),
    float((packed >> 10u) & 1023u),
    float((packed >> 20u) & 1023u)
  ) / 1023.0;
}

float unpack_pair_low(uint packed) {
  return float(packed & 65535u) / 65535.0;
}

float unpack_pair_high(uint packed) {
  return float((packed >> 16u) & 65535u) / 65535.0;
}

uint surface_word(uint index) {
  uint group = index >> 2u;
  uint lane = index & 3u;
  return group == 0u
  ? push_constants.surface[0][lane] : push_constants.surface[1][lane];
}

float decode01(float packed, float min_value, float max_value) {
  return mix(min_value, max_value, packed);
}

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

float shadow_visibility(vec3 world_position, vec3 normal, vec3 light_direction) {
  if (shadow_params.x < 0.5)
    return 1.0;

  vec4 shadow_clip = shadow_mvp[0] * vec4(world_position, 1.0);
  if (shadow_clip.w <= 0.0001)
    return 1.0;

  vec3 shadow_ndc = shadow_clip.xyz / shadow_clip.w;
  vec2 shadow_uv = shadow_ndc.xy * 0.5 + 0.5;
  shadow_uv.y = 1.0 - shadow_uv.y;

  if (shadow_uv.x <= 0.0 || shadow_uv.x >= 1.0 || shadow_uv.y <= 0.0 || shadow_uv.y >= 1.0 || shadow_ndc.z <= 0.0 || shadow_ndc.z >= 1.0)
    return 1.0;

  float slope = 1.0 - max(dot(normal, light_direction), 0.0);
  float bias = max(shadow_params.y + slope * shadow_params.z, 0.0001);
  float depth = shadow_ndc.z - bias;
  vec2 texel_size = 1.0 / vec2(textureSize(shadow_map, 0));
  float visibility = 0.0;

  for (int y = -1; y <= 1; ++y)
    for (int x = -1; x <= 1; ++x)
      visibility += texture(shadow_map, vec3(shadow_uv + vec2(x, y) * texel_size, depth));

  return visibility / 9.0;
}

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

vec3 blackbody(float kelvin) {
  float k = clamp(kelvin, 1000.0, 40000.0) / 1000.0;
  float r = k <= 6.6 ? 1.0 : clamp(1.29293618662 * pow(k - 6.0, -0.1332047592), 0.0, 1.0);
  float g = k <= 6.6 ? clamp(0.39008157877 * log(k) - 0.63184144315, 0.0, 1.0) : clamp(1.12989086 * pow(k - 6.0, -0.07551485), 0.0, 1.0);
  float b = k >= 6.6 ? 1.0 : (k <= 1.9 ? 0.0 : clamp(0.54320678911 * log(k - 1.0) - 1.19625408914, 0.0, 1.0));
  return vec3(r, g, b);
}

vec3 unpack_specular_color() {
  return unpack_rgb10(push_constants.material_ext.x);
}

vec3 unpack_attenuation_color() {
  return unpack_rgb10(push_constants.material_ext.y);
}

vec3 unpack_sheen_color() {
  return unpack_rgb10(push_constants.material_ext.z);
}

void main() {
  vec3 base = in_color.rgb;
  float alpha = in_color.a;
  if (has_texture(TEX_BASE)) {
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
  if (alpha <= 0.001)
    discard;

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

  if (has_texture(TEX_OCCLUSION))
    occlusion *= mix(1.0, texture(material_textures[TEX_OCCLUSION], material_uv(TEX_OCCLUSION)).r, push_constants.pbr.w);
  occlusion = clamp(occlusion, 0.0, 1.0);

  float normal_scale = decode01(unpack_pair_low(surface_word(0)), 0.0, 4.0);
  n = unpack_surface_normal(n, t, b, normal_scale);
  if (!gl_FrontFacing && double_sided)
    n = -n;

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

  if (has_texture(TEX_SPECULAR))
    specular_factor *= texture(material_textures[TEX_SPECULAR], material_uv(TEX_SPECULAR)).r;
  vec3 specular_color = unpack_specular_color();
  if (has_texture(TEX_SPECULAR_COLOR))
    specular_color *= texture(material_textures[TEX_SPECULAR_COLOR], material_uv(TEX_SPECULAR_COLOR)).rgb;

  float clearcoat_map = clearcoat;
  float clearcoat_rough_map = clearcoat_roughness;
  if (has_texture(TEX_CLEARCOAT))
    clearcoat_map *= texture(material_textures[TEX_CLEARCOAT], material_uv(TEX_CLEARCOAT)).r;
  if (has_texture(TEX_CLEARCOAT_ROUGH))
    clearcoat_rough_map *= texture(material_textures[TEX_CLEARCOAT_ROUGH], material_uv(TEX_CLEARCOAT_ROUGH)).r;
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
  if (has_texture(TEX_SHEEN_COLOR))
    sheen_c *= texture(material_textures[TEX_SHEEN_COLOR], material_uv(TEX_SHEEN_COLOR)).rgb;
  if (has_texture(TEX_SHEEN_ROUGH))
    sheen_r *= texture(material_textures[TEX_SHEEN_ROUGH], material_uv(TEX_SHEEN_ROUGH)).r;
  sheen_r = clamp(sheen_r, 0.0, 1.0);

  if (has_texture(TEX_IRIDESCENCE))
    iridescence *= texture(material_textures[TEX_IRIDESCENCE], material_uv(TEX_IRIDESCENCE)).r;
  float film_thickness = mix(iridescence_min, iridescence_max, 0.5);
  if (has_texture(TEX_IRIDESCENCE_THICKNESS))
    film_thickness = mix(iridescence_min, iridescence_max, texture(material_textures[TEX_IRIDESCENCE_THICKNESS], material_uv(TEX_IRIDESCENCE_THICKNESS)).r);
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

  if (unlit || push_constants.material.x < 0.5) {
    vec3 emit_tex = vec3(1.0);
    if (has_texture(TEX_EMISSION))
      emit_tex = texture(material_textures[TEX_EMISSION], material_uv(TEX_EMISSION)).rgb;
    vec3 emission_color = push_constants.emission.rgb * emit_tex * blackbody(push_constants.emission.w);
    float emission_strength = max(push_constants.material.y, 0.0);
    vec3 final_unlit = base + emission_color * emission_strength;
    out_color = vec4(min(final_unlit, vec3(64.0)), alpha);
    return;
  }

  vec3 attenuation_color = unpack_attenuation_color();
  float transmission_map = transmission;
  if (has_texture(TEX_TRANSMISSION))
    transmission_map *= texture(material_textures[TEX_TRANSMISSION], material_uv(TEX_TRANSMISSION)).r;
  float thickness = volume_thickness;
  if (has_texture(TEX_THICKNESS))
    thickness *= texture(material_textures[TEX_THICKNESS], material_uv(TEX_THICKNESS)).r;
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
    if (light.parameters.w > 0.0 && push_constants.material_ext.w > 0u && abs(light.parameters.w - uintBitsToFloat(push_constants.material_ext.w)) < 0.5)
      continue;

    vec3 light_color = max(light.color.rgb, vec3(0.0));
    float intensity = max(light.parameters.x, 0.0);
    float ambient = max(light.parameters.y, 0.0);
    float attenuation = 1.0;
    vec3 light_dir = vec3(0.0);
    bool casts_shadow = false;

    if (light.cone.w == LIGHT_DIRECTIONAL) {
      light_dir = normalize(-light.direction.xyz);
      casts_shadow = true;
    } else {
      vec3 to_light = light.position.xyz - in_position;
      float distance_to_light = length(to_light);
      if (distance_to_light <= 0.0001)
        continue;
      light_dir = to_light / distance_to_light;
      attenuation = point_attenuation(distance_to_light, max(light.cone.x, 0.0001));
      if (light.cone.w == LIGHT_SPOT)
        attenuation *= spot_factor(-light_dir, light.direction.xyz, light.cone.y, light.cone.z);
    }
    if (attenuation <= 0.0)
      continue;

    float ndotl = max(dot(n, light_dir), 0.0);
    float shadow = 1.0;
    if (casts_shadow && ndotl > 0.0)
      shadow = shadow_visibility(in_position, n, light_dir);
    ambient_light += ambient * intensity * attenuation;
    if (ndotl <= 0.0)
      continue;

    vec3 half_dir = normalize(view_dir + light_dir);
    float ndoth = max(dot(n, half_dir), 0.0);
    float vdoth = max(dot(view_dir, half_dir), 0.0);
    float D = distribution_ggx_aniso(n, half_dir, anis_t, anis_b, roughness, aniso_strength);
    float G = geometry_smith(ndotv, ndotl, roughness);
    vec3 F = fresnel_schlick(vdoth, iridescent_f0);
    vec3 spec = D * G * F / max(4.0 * ndotv * ndotl, 0.0001);
    vec3 kd = (1.0 - F) * (1.0 - metallic);

    float diffuse_term = ndotl;
    diffuse_light += light_color * intensity * attenuation * shadow * kd * base * diffuse_term / PI;
    specular_light += light_color * intensity * attenuation * shadow * spec * ndotl;

    if (clearcoat_map > 0.001) {
      float c_ndotl = max(dot(coat_n, light_dir), 0.0);
      float c_ndotv = max(dot(coat_n, view_dir), 0.0);
      vec3 c_h = normalize(view_dir + light_dir);
      float c_ndoth = max(dot(coat_n, c_h), 0.0);
      float c_vdoth = max(dot(view_dir, c_h), 0.0);
      float c_d = distribution_ggx_aniso(coat_n, c_h, anis_t, anis_b, clearcoat_rough_map, 0.0);
      float c_g = geometry_smith(c_ndotv, c_ndotl, clearcoat_rough_map);
      vec3 c_f = fresnel_schlick(c_vdoth, vec3(0.04));
      specular_light += light_color * intensity * attenuation * shadow * c_d * c_g * c_f * clearcoat_map * c_ndotl / max(4.0 * c_ndotv * c_ndotl, 0.0001);
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
  if (has_texture(TEX_EMISSION))
    emit_tex = texture(material_textures[TEX_EMISSION], material_uv(TEX_EMISSION)).rgb;
  vec3 emission_color = push_constants.emission.rgb * emit_tex * blackbody(push_constants.emission.w);
  float emission_strength = max(push_constants.material.y, 0.0);
  vec3 ambient = base * ambient_light * occlusion * (1.0 - metallic);
  vec3 color = ambient + diffuse_light * (1.0 - transmission_map) + specular_light + transmission_light + emission_color * emission_strength;
  if (push_constants.material.z > 0.0)
    color += emission_color * push_constants.material.z * 0.05;

  float output_alpha = alpha;
  if (transmission_map > 0.001 && alpha_mode == 2u)
    output_alpha *= max(0.05, 1.0 - transmission_map * 0.85);

  out_color = vec4(min(max(color, vec3(0.0)), vec3(64.0)), output_alpha);
}
