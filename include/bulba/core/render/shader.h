#ifndef BULBA_CORE_RENDER_SHADER_H
#define BULBA_CORE_RENDER_SHADER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  BLB_SHADER_SOURCE_GLSL = 0,
  BLB_SHADER_SOURCE_SPIRV = 1,
  BLB_SHADER_SOURCE_HLSL = 2
} BLB_ShaderSourceType;

typedef struct {
  char name[64];
  char vertex_path[512];
  char fragment_path[512];
  BLB_ShaderSourceType source_type;
  bool is_2d;
  bool custom;
} BLB_ShaderAsset;

BLB_ShaderAsset BLB_Shader_Default2D(void);
BLB_ShaderAsset BLB_Shader_Default3D(void);
int BLB_Shader_SetPaths(BLB_ShaderAsset *shader, const char *vertex_path, const char *fragment_path, BLB_ShaderSourceType source_type);
bool BLB_Shader_IsCustom(const BLB_ShaderAsset *shader);

#endif
