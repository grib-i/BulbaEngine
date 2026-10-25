#version 450
layout(set = 0, binding = 0) uniform sampler2D scene_color;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 out_color;
layout(push_constant) uniform Settings {
  vec4 correction;
  vec4 mapping;
  vec4 effects;
  vec4 viewport;
}
settings;
float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }
vec3 source(vec2 at) { return max(texture(scene_color, at).rgb, vec3(0.0)); }
vec3 antialias(vec3 center) {
  vec2 pixel = settings.viewport.xy;
  vec3 nw = source(uv + vec2(-1, -1) * pixel);
  vec3 ne = source(uv + vec2(1, -1) * pixel);
  vec3 sw = source(uv + vec2(-1, 1) * pixel);
  vec3 se = source(uv + vec2(1, 1) * pixel);
  float lc = luma(center), lnw = luma(nw), lne = luma(ne), lsw = luma(sw), lse = luma(se);
  float low = min(lc, min(min(lnw, lne), min(lsw, lse)));
  float high = max(lc, max(max(lnw, lne), max(lsw, lse)));

  if (high - low < max(0.0312, high * 0.125))
    return center;
  vec2 direction = vec2(-((lnw + lne) - (lsw + lse)), (lnw + lsw) - (lne + lse));
  float reduction = max((lnw + lne + lsw + lse) * 0.03125, 0.0078125);
  direction = clamp(direction / (min(abs(direction.x), abs(direction.y)) + reduction), vec2(-8), vec2(8)) * pixel;
  vec3 a = 0.5 * (source(uv + direction * (-1.0 / 6.0)) + source(uv + direction * (1.0 / 6.0)));
  vec3 b = a * 0.5 + 0.25 * (source(uv - direction * 0.5) + source(uv + direction * 0.5));
  float lb = luma(b);

  return lb < low || lb > high ? a : b;
}
vec3 bright(vec2 at) {
  vec3 c = source(at);
  float peak = max(c.r, max(c.g, c.b));

  return c * max(peak - settings.effects.y, 0.0) / max(peak, 0.0001);
}
void main() {
  vec4 original = texture(scene_color, uv);

  if (settings.correction.x < 0.5) {
    out_color = original;

    return;
  }
  vec3 color = max(original.rgb, vec3(0.0));

  if (settings.effects.w > 0.5)
    color = antialias(color);

  if (settings.effects.x > 0.0) {
    vec2 step_size = settings.viewport.xy * settings.effects.z;
    vec3 glow = bright(uv) * 0.2;

    for (int y = -1; y <= 1; ++y) {
      for (int x = -1; x <= 1; ++x) {
        if (x != 0 || y != 0)
          glow += bright(uv + vec2(x, y) * step_size) * 0.1;
      }
    }
    color += glow * settings.effects.x;
  }
  color *= settings.correction.y;

  if (settings.mapping.y > 1.5) {
    color = clamp((color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14), 0.0, 1.0);
  } else if (settings.mapping.y > 0.5) {
    color /= 1.0 + color;
  }
  color = mix(vec3(luma(color)), color, settings.correction.w);
  color = max((color - 0.18) * settings.correction.z + 0.18, vec3(0.0));
  vec2 edge = uv * (1.0 - uv);
  float vignette = pow(clamp(edge.x * edge.y * 16.0, 0.0, 1.0), settings.mapping.w);
  color *= vignette;
  color = pow(color, vec3(1.0 / settings.mapping.x));

  if (settings.viewport.z > 0.5) {
    color = mix(color * 12.92, 1.055 * pow(max(color, vec3(0)), vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), color));
  }
  out_color = vec4(color, original.a);
}
