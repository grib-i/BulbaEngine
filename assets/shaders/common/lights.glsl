#ifndef BLB_LIGHTS_GLSL
#define BLB_LIGHTS_GLSL

#define MAX_LIGHTS 64
#define SHADOW_MAP_COUNT 7

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

layout(std430, set = 0, binding = 0) readonly buffer LightBuffer {
  uint light_count;
  uint padding0;
  uint padding1;
  uint padding2;
  vec4 camera_position;
  GPULight lights[MAX_LIGHTS];
  mat4 shadow_mvp[SHADOW_MAP_COUNT];
  vec4 shadow_params;
  mat4 view_projection;
};

#endif
