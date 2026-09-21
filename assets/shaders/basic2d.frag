#version 450

#define MAX_LIGHTS 64
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
const float PI = 3.14159265359;
const float LIGHT_POINT = 0.0;
const float LIGHT_DIRECTIONAL = 1.0;
const float LIGHT_SPOT = 2.0;

struct GPULight {
  vec4 direction;
  vec4 position;
  vec4 color;
  vec4 parameters;
  vec4 cone;
};

struct MaterialTextureParams { uint packed[3]; };

layout(std430, set = 0, binding = 0) readonly buffer LightBuffer {
  uint light_count;
  uint padding0;
  uint padding1;
  uint padding2;
  vec4 camera_position;
  GPULight lights[MAX_LIGHTS];
  mat4 shadow_mvp[7];
  vec4 shadow_params;
};

layout(set = 1, binding = 0) uniform sampler2D material_textures[TEX_COUNT];
layout(std430, set = 1, binding = 1) readonly buffer MaterialBuffer {
  MaterialTextureParams textures[TEX_COUNT];
} material_buffer;

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_position;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  vec4 viewport;
  vec4 material;
  vec4 pbr;
  vec4 emission;
  uvec4 material_ext;
  uvec4 surface[2];
  uvec4 meta;
} push_constants;

bool has_texture(uint slot) { return (push_constants.meta.x & (1u << slot)) != 0u; }

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
  return vec3(float(packed & 1023u), float((packed >> 10u) & 1023u), float((packed >> 20u) & 1023u)) / 1023.0;
}
uint surface_word(uint index) {
  uint group = index >> 2u;
  uint lane = index & 3u;
  return group == 0u
      ? push_constants.surface[0][lane]
      : push_constants.surface[1][lane];
}

float unpack_low(uint packed) { return float(packed & 65535u) / 65535.0; }
float unpack_high(uint packed) { return float((packed >> 16u) & 65535u) / 65535.0; }
float remap(float x, float a, float b) { return mix(a, b, x); }

float point_attenuation(float d, float range) {
  if (d >= range || range <= 0.0001) return 0.0;
  float q = max(d, 0.05);
  float inv = 1.0 / (1.0 + q * q * 0.100);
  float fade = 1.0 - smoothstep(range * 0.72, range, d);
  return inv * fade * fade;
}

float spot_factor(vec2 dir_to_light, vec2 spot_dir, float inner_cone, float outer_cone) {
  float dl = length(dir_to_light);
  float sl = length(spot_dir);
  if (dl <= 0.0001 || sl <= 0.0001) return 0.0;
  float theta = dot(dir_to_light / dl, spot_dir / sl);
  float epsilon = max(inner_cone - outer_cone, 0.0001);
  return clamp((theta - outer_cone) / epsilon, 0.0, 1.0);
}

vec3 blackbody(float kelvin) {
  float k = clamp(kelvin, 1000.0, 40000.0) / 1000.0;
  float r = k <= 6.6 ? 1.0 : clamp(1.29293618662 * pow(k - 6.0, -0.1332047592), 0.0, 1.0);
  float g = k <= 6.6 ? clamp(0.39008157877 * log(k) - 0.63184144315, 0.0, 1.0) : clamp(1.12989086 * pow(k - 6.0, -0.07551485), 0.0, 1.0);
  float b = k >= 6.6 ? 1.0 : (k <= 1.9 ? 0.0 : clamp(0.54320678911 * log(k - 1.0) - 1.19625408914, 0.0, 1.0));
  return vec3(r, g, b);
}

vec3 fresnel_schlick(float c, vec3 f0) {
  float f = pow(clamp(1.0 - c, 0.0, 1.0), 5.0);
  return f0 + (1.0 - f0) * f;
}

float distribution_ggx(float ndoth, float roughness) {
  float a = max(roughness * roughness, 0.0025);
  float a2 = a * a;
  float d = ndoth * ndoth * (a2 - 1.0) + 1.0;
  return a2 / max(PI * d * d, 0.000001);
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

float geometry_schlick(float ndotv, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  return ndotv / max(ndotv * (1.0 - k) + k, 0.000001);
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
  bool unlit = (flags & 2u) != 0u;
  uint alpha_mode = (flags >> 2u) & 3u;
  float alpha_cutoff = float((flags >> 6u) & 1023u) / 1023.0;
  if (alpha_mode == 1u && alpha < alpha_cutoff) discard;
  if (alpha <= 0.001) discard;

  vec3 n = vec3(0.0, 0.0, 1.0);
  vec3 t = vec3(1.0, 0.0, 0.0);
  vec3 b = vec3(0.0, 1.0, 0.0);
  if (has_texture(TEX_NORMAL)) {
    vec3 tangent_normal = texture(material_textures[TEX_NORMAL], material_uv(TEX_NORMAL)).xyz * 2.0 - 1.0;
    tangent_normal.xy *= remap(unpack_low(surface_word(0)), 0.0, 4.0);
    n = normalize(mat3(t, b, n) * tangent_normal);
  }

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
    occlusion *= mix(1.0, texture(material_textures[TEX_OCCLUSION], material_uv(TEX_OCCLUSION)).r, 1.0);

  if (has_texture(TEX_SPECULAR)) specular_factor *= texture(material_textures[TEX_SPECULAR], material_uv(TEX_SPECULAR)).r;
  vec3 specular_color = unpack_rgb10(push_constants.material_ext.x);
  if (has_texture(TEX_SPECULAR_COLOR)) specular_color *= texture(material_textures[TEX_SPECULAR_COLOR], material_uv(TEX_SPECULAR_COLOR)).rgb;

  float ior = remap(unpack_high(surface_word(0)), 1.0, 4.0);
  float transmission = remap(unpack_low(surface_word(1)), 0.0, 1.0);
  float thickness = remap(unpack_high(surface_word(1)), 0.0, 100.0);
  float attenuation_distance = remap(unpack_low(surface_word(2)), 0.0, 1000.0);
  vec3 attenuation_color = unpack_rgb10(push_constants.material_ext.y);
  if (has_texture(TEX_TRANSMISSION)) transmission *= texture(material_textures[TEX_TRANSMISSION], material_uv(TEX_TRANSMISSION)).r;
  if (has_texture(TEX_THICKNESS)) thickness *= texture(material_textures[TEX_THICKNESS], material_uv(TEX_THICKNESS)).r;

  float clearcoat = remap(unpack_high(surface_word(2)), 0.0, 1.0);
  float clearcoat_roughness = remap(unpack_low(surface_word(3)), 0.0, 1.0);
  float clearcoat_normal_scale = remap(unpack_high(surface_word(3)), 0.0, 4.0);
  if (has_texture(TEX_CLEARCOAT)) clearcoat *= texture(material_textures[TEX_CLEARCOAT], material_uv(TEX_CLEARCOAT)).r;
  if (has_texture(TEX_CLEARCOAT_ROUGH)) clearcoat_roughness *= texture(material_textures[TEX_CLEARCOAT_ROUGH], material_uv(TEX_CLEARCOAT_ROUGH)).r;
  clearcoat = clamp(clearcoat, 0.0, 1.0);
  clearcoat_roughness = clamp(clearcoat_roughness, 0.03, 1.0);

  vec3 coat_n = n;
  if (has_texture(TEX_CLEARCOAT_NORMAL)) {
    vec3 coat_ts = texture(material_textures[TEX_CLEARCOAT_NORMAL], material_uv(TEX_CLEARCOAT_NORMAL)).xyz * 2.0 - 1.0;
    coat_ts.xy *= clearcoat_normal_scale;
    coat_n = normalize(mat3(t, b, n) * coat_ts);
  }

  vec3 sheen_color = unpack_rgb10(push_constants.material_ext.z);
  float sheen_roughness = remap(unpack_low(surface_word(4)), 0.0, 1.0);
  if (has_texture(TEX_SHEEN_COLOR)) sheen_color *= texture(material_textures[TEX_SHEEN_COLOR], material_uv(TEX_SHEEN_COLOR)).rgb;
  if (has_texture(TEX_SHEEN_ROUGH)) sheen_roughness *= texture(material_textures[TEX_SHEEN_ROUGH], material_uv(TEX_SHEEN_ROUGH)).r;

  float iridescence = remap(unpack_high(surface_word(4)), 0.0, 1.0);
  float iridescence_ior = remap(unpack_low(surface_word(5)), 1.0, 4.0);
  float film_min = remap(unpack_high(surface_word(5)), 0.0, 2000.0);
  float film_max = remap(unpack_low(surface_word(6)), 0.0, 2000.0);
  if (has_texture(TEX_IRIDESCENCE)) iridescence *= texture(material_textures[TEX_IRIDESCENCE], material_uv(TEX_IRIDESCENCE)).r;
  float film_thickness = mix(film_min, film_max, 0.5);
  if (has_texture(TEX_IRIDESCENCE_THICKNESS)) film_thickness = mix(film_min, film_max, texture(material_textures[TEX_IRIDESCENCE_THICKNESS], material_uv(TEX_IRIDESCENCE_THICKNESS)).r);

  float anisotropy = remap(unpack_high(surface_word(6)), 0.0, 1.0);
  float anisotropy_rotation = remap(unpack_low(surface_word(7)), -PI, PI);
  float dispersion = remap(unpack_high(surface_word(7)), 0.0, 1.0);
  if (has_texture(TEX_ANISOTROPY)) {
    vec2 atex = texture(material_textures[TEX_ANISOTROPY], material_uv(TEX_ANISOTROPY)).rg;
    anisotropy *= atex.r;
    anisotropy_rotation += (atex.g * 2.0 - 1.0) * PI;
  }
  anisotropy = clamp(anisotropy, 0.0, 0.95);
  vec3 anis_t = normalize(t * cos(anisotropy_rotation) + b * sin(anisotropy_rotation));
  vec3 anis_b = normalize(cross(n, anis_t));

  vec3 view_dir = normalize(vec3(camera_position.xy - in_position, 1.0));
  float ndotv = max(dot(n, view_dir), 0.0);
  float f0_scalar = pow((ior - 1.0) / max(ior + 1.0, 0.0001), 2.0);
  vec3 f0 = mix(vec3(f0_scalar * specular_factor) * specular_color, base, metallic);
  if (iridescence > 0.001) {
    float phase = 2.0 * PI * (iridescence_ior - 1.0) * max(film_thickness, 1.0) * 0.001 * ndotv;
    vec3 film = 0.5 + 0.5 * cos(vec3(phase, phase + 2.0944, phase + 4.18879));
    f0 = mix(f0, film, iridescence);
  }

  vec3 diffuse_light = vec3(0.0);
  vec3 specular_light = vec3(0.0);
  vec3 transmission_light = vec3(0.0);
  float ambient_light = 0.025;

  if (unlit || push_constants.material.x < 0.5) {
    vec3 emit_tex = vec3(1.0);
    if (has_texture(TEX_EMISSION))
      emit_tex = texture(material_textures[TEX_EMISSION], material_uv(TEX_EMISSION)).rgb;
    vec3 emission_color = push_constants.emission.rgb * emit_tex * blackbody(push_constants.emission.w);
    out_color = vec4(min(base + emission_color * max(push_constants.material.y, 0.0), vec3(64.0)), alpha);
    return;
  }

  float volume_factor = attenuation_distance > 0.0001 && attenuation_distance < 999.9 ? clamp(thickness / attenuation_distance, 0.0, 50.0) : 0.0;
  vec3 volume_tint = exp(log(max(attenuation_color, vec3(0.0001))) * volume_factor);

  for (uint i = 0u; i < light_count && i < MAX_LIGHTS; ++i) {
    GPULight light = lights[i];
    if (light.parameters.w > 0.0 && push_constants.material_ext.w > 0u && abs(light.parameters.w - uintBitsToFloat(push_constants.material_ext.w)) < 0.5) continue;

    vec3 light_color = max(light.color.rgb, vec3(0.0));
    float intensity = max(light.parameters.x, 0.0);
    float ambient = max(light.parameters.y, 0.0);
    float attenuation = 1.0;
    vec3 light_dir = vec3(0.0, 0.0, 1.0);
    if (light.cone.w == LIGHT_DIRECTIONAL) {
      light_dir = normalize(vec3(-light.direction.xy, 1.0));
    } else {
      vec2 to_light = light.position.xy - in_position;
      float d = length(to_light);
      if (d <= 0.0001) continue;
      light_dir = normalize(vec3(to_light, 1.0));
      attenuation = point_attenuation(d, max(light.cone.x, 0.0001));
      if (light.cone.w == LIGHT_SPOT) attenuation *= spot_factor(-to_light / d, light.direction.xy, light.cone.y, light.cone.z);
    }
    if (attenuation <= 0.0) continue;

    float ndotl = max(dot(n, light_dir), 0.0);
    if (ndotl <= 0.0) continue;
    ambient_light += ambient * intensity * attenuation;
    vec3 h = normalize(view_dir + light_dir);
    float ndoth = max(dot(n, h), 0.0);
    float vdoth = max(dot(view_dir, h), 0.0);
    float dterm = distribution_ggx_aniso(n, h, anis_t, anis_b, roughness, anisotropy);
    float gterm = geometry_schlick(ndotv, roughness) * geometry_schlick(ndotl, roughness);
    vec3 fresnel = fresnel_schlick(vdoth, f0);
    vec3 spec = dterm * gterm * fresnel / max(4.0 * ndotv * ndotl, 0.0001);
    vec3 kd = (1.0 - fresnel) * (1.0 - metallic);
    diffuse_light += light_color * intensity * attenuation * kd * base * ndotl / PI;
    specular_light += light_color * intensity * attenuation * spec * ndotl * (1.0 + anisotropy * 0.4);

    if (clearcoat > 0.001) {
      float c_ndotl = max(dot(coat_n, light_dir), 0.0);
      float c_ndotv = max(dot(coat_n, view_dir), 0.0);
      float c_ndoth = max(dot(coat_n, h), 0.0);
      float c_d = distribution_ggx(c_ndoth, clearcoat_roughness);
      float c_g = geometry_schlick(c_ndotv, clearcoat_roughness) * geometry_schlick(c_ndotl, clearcoat_roughness);
      vec3 cf = fresnel_schlick(vdoth, vec3(0.04));
      specular_light += light_color * intensity * attenuation * c_d * c_g * cf * c_ndotl * clearcoat * 0.25;
    }
    if (length(sheen_color) > 0.001) {
      float sheen = pow(1.0 - ndotv, 5.0) * (1.0 - 0.5 * sheen_roughness);
      diffuse_light += light_color * intensity * attenuation * sheen_color * sheen * 0.25 * ndotl;
    }
    if (transmission > 0.001) {
      vec3 spectral = mix(vec3(1.0), vec3(1.0 + dispersion * 0.25, 1.0, 1.0 - dispersion * 0.25), dispersion);
      transmission_light += light_color * intensity * attenuation * transmission * pow(1.0 - ndotl, 2.0) * volume_tint * spectral * 0.5;
    }
  }

  vec3 emit_tex = vec3(1.0);
  if (has_texture(TEX_EMISSION))
    emit_tex = texture(material_textures[TEX_EMISSION], material_uv(TEX_EMISSION)).rgb;
  vec3 emission_color = push_constants.emission.rgb * emit_tex * blackbody(push_constants.emission.w);
  vec3 color = base * ambient_light * occlusion + diffuse_light * (1.0 - transmission) + specular_light + transmission_light + emission_color * max(push_constants.material.y, 0.0);
  if (push_constants.material.z > 0.0) color += emission_color * push_constants.material.z * 0.05;

  float out_alpha = alpha;
  if (alpha_mode == 2u && transmission > 0.001) out_alpha *= max(0.05, 1.0 - transmission * 0.85);
  out_color = vec4(min(max(color, vec3(0.0)), vec3(64.0)), out_alpha);
}
