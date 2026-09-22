#ifndef BULBA_CORE_RENDER_SHADER_H
#define BULBA_CORE_RENDER_SHADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLB_SHADER_NAME_MAX 64
#define BLB_SHADER_PATH_MAX 512

typedef enum {
  BLB_SHADER_SOURCE_GLSL = 0,
  BLB_SHADER_SOURCE_SPIRV = 1,
  BLB_SHADER_SOURCE_HLSL = 2
} BLB_ShaderSourceType;

typedef struct {
  char name[BLB_SHADER_NAME_MAX];
  char vertex_path[BLB_SHADER_PATH_MAX];
  char fragment_path[BLB_SHADER_PATH_MAX];
  BLB_ShaderSourceType source_type;
  bool is_2d;
  bool custom;
} BLB_ShaderAsset;

typedef struct BLB_ShaderProgram {
  size_t ref_count;
  BLB_ShaderAsset asset;

  unsigned char *vertex_code;
  unsigned char *fragment_code;
  size_t vertex_size;
  size_t fragment_size;

  uint64_t revision;
} BLB_ShaderProgram;

BLB_ShaderAsset BLB_Shader_Default2D(void);
BLB_ShaderAsset BLB_Shader_Default3D(void);
int BLB_Shader_SetPaths(BLB_ShaderAsset *shader, const char *vertex_path, const char *fragment_path, BLB_ShaderSourceType source_type);
bool BLB_Shader_IsCustom(const BLB_ShaderAsset *shader);

BLB_ShaderProgram *BLB_Shader_Load(const BLB_ShaderAsset *asset);
BLB_ShaderProgram *BLB_Shader_LoadSPIRV(const char *vertex_path, const char *fragment_path, bool is_2d);
void BLB_Shader_Retain(BLB_ShaderProgram *shader);
void BLB_Shader_Release(BLB_ShaderProgram *shader);
int BLB_Shader_Reload(BLB_ShaderProgram *shader);
uint64_t BLB_Shader_GetRevision(const BLB_ShaderProgram *shader);

#endif
