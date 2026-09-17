#include "bulba/core/render/shader.h"

#include <string.h>

static void copy_path(char *destination, size_t size, const char *source) {
  memset(destination, 0, size);
  if (source)
    strncpy(destination, source, size - 1);
}

BLB_ShaderAsset BLB_Shader_Default2D(void) {
  BLB_ShaderAsset shader = {0};
  strncpy(shader.name, "Basic2D", sizeof(shader.name) - 1);
  copy_path(shader.vertex_path, sizeof(shader.vertex_path), "basic2d.vert.spv");
  copy_path(shader.fragment_path, sizeof(shader.fragment_path), "basic2d.frag.spv");
  shader.source_type = BLB_SHADER_SOURCE_SPIRV;
  shader.is_2d = true;
  return shader;
}

BLB_ShaderAsset BLB_Shader_Default3D(void) {
  BLB_ShaderAsset shader = {0};
  strncpy(shader.name, "Basic3D", sizeof(shader.name) - 1);
  copy_path(shader.vertex_path, sizeof(shader.vertex_path), "basic3d.vert.spv");
  copy_path(shader.fragment_path, sizeof(shader.fragment_path), "basic3d.frag.spv");
  shader.source_type = BLB_SHADER_SOURCE_SPIRV;
  shader.is_2d = false;
  return shader;
}

int BLB_Shader_SetPaths(BLB_ShaderAsset *shader, const char *vertex_path, const char *fragment_path, BLB_ShaderSourceType source_type) {
  if (!shader || !vertex_path || !fragment_path)
    return -1;
  copy_path(shader->vertex_path, sizeof(shader->vertex_path), vertex_path);
  copy_path(shader->fragment_path, sizeof(shader->fragment_path), fragment_path);
  shader->source_type = source_type;
  shader->custom = true;
  return 0;
}

bool BLB_Shader_IsCustom(const BLB_ShaderAsset *shader) {
  return shader && shader->custom;
}
