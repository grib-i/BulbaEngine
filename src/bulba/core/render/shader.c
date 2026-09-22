#include "bulba/core/render/shader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  BLB_ShaderProgram *program;
  char *vertex_path;
  char *fragment_path;
  bool is_2d;
} BLB_ShaderCacheEntry;

static BLB_ShaderCacheEntry *shader_cache = NULL;
static size_t shader_cache_count = 0;
static size_t shader_cache_capacity = 0;

static char *shader_strdup(const char *text) {
  if (!text) return NULL;
  size_t n = strlen(text) + 1u;
  char *copy = malloc(n);
  if (!copy) return NULL;
  memcpy(copy, text, n);
  return copy;
}

static BLB_ShaderProgram *shader_cache_find(const char *vertex_path, const char *fragment_path, bool is_2d) {
  for (size_t i = 0; i < shader_cache_count; ++i) {
    BLB_ShaderCacheEntry *entry = &shader_cache[i];
    if (entry->program && entry->is_2d == is_2d && strcmp(entry->vertex_path, vertex_path) == 0 && strcmp(entry->fragment_path, fragment_path) == 0)
      return entry->program;
  }
  return NULL;
}

static void shader_cache_insert(BLB_ShaderProgram *program) {
  if (!program) return;
  if (shader_cache_count == shader_cache_capacity) {
    size_t new_capacity = shader_cache_capacity ? shader_cache_capacity * 2u : 16u;
    BLB_ShaderCacheEntry *next = realloc(shader_cache, new_capacity * sizeof(*next));
    if (!next) return;
    shader_cache = next;
    shader_cache_capacity = new_capacity;
  }
  shader_cache[shader_cache_count] = (BLB_ShaderCacheEntry){
      .program = program,
      .vertex_path = shader_strdup(program->asset.vertex_path),
      .fragment_path = shader_strdup(program->asset.fragment_path),
      .is_2d = program->asset.is_2d,
  };
  if (!shader_cache[shader_cache_count].vertex_path || !shader_cache[shader_cache_count].fragment_path) {
    free(shader_cache[shader_cache_count].vertex_path);
    free(shader_cache[shader_cache_count].fragment_path);
    shader_cache[shader_cache_count] = (BLB_ShaderCacheEntry){0};
    return;
  }
  ++shader_cache_count;
}

static void shader_cache_remove(BLB_ShaderProgram *program) {
  if (!program) return;
  for (size_t i = 0; i < shader_cache_count; ++i) {
    if (shader_cache[i].program != program) continue;
    free(shader_cache[i].vertex_path);
    free(shader_cache[i].fragment_path);
    shader_cache[i] = shader_cache[shader_cache_count - 1u];
    --shader_cache_count;
    return;
  }
}

static void copy_path(char *destination, size_t size, const char *source) {
  if (!destination || size == 0)
    return;

  memset(destination, 0, size);
  if (source)
    strncpy(destination, source, size - 1);
}

static unsigned char *read_binary_file(const char *path, size_t *out_size) {
  if (!path || !out_size)
    return NULL;

  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  long end = ftell(file);
  if (end <= 0 || (size_t)end % sizeof(uint32_t) != 0) {
    fclose(file);
    return NULL;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  size_t size = (size_t)end;
  unsigned char *data = malloc(size);
  if (!data) {
    fclose(file);
    return NULL;
  }

  size_t read_size = fread(data, 1, size, file);
  fclose(file);

  if (read_size != size) {
    free(data);
    return NULL;
  }

  *out_size = size;
  return data;
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

bool BLB_Shader_IsCustom(const BLB_ShaderAsset *shader) { return shader && shader->custom; }

BLB_ShaderProgram *BLB_Shader_Load(const BLB_ShaderAsset *asset) {
  if (!asset || asset->source_type != BLB_SHADER_SOURCE_SPIRV || !asset->vertex_path[0] || !asset->fragment_path[0])
    return NULL;

  BLB_ShaderProgram *cached = shader_cache_find(asset->vertex_path, asset->fragment_path, asset->is_2d);
  if (cached) {
    BLB_Shader_Retain(cached);
    return cached;
  }

  size_t vertex_size = 0;
  size_t fragment_size = 0;
  unsigned char *vertex = read_binary_file(asset->vertex_path, &vertex_size);
  unsigned char *fragment = read_binary_file(asset->fragment_path, &fragment_size);

  if (!vertex || !fragment || vertex_size < 4 || fragment_size < 4 || vertex_size % 4u != 0 || fragment_size % 4u != 0) {
    free(vertex);
    free(fragment);
    return NULL;
  }
  uint32_t vertex_magic = 0, fragment_magic = 0;
  memcpy(&vertex_magic, vertex, sizeof(vertex_magic));
  memcpy(&fragment_magic, fragment, sizeof(fragment_magic));
  if (vertex_magic != UINT32_C(0x07230203) || fragment_magic != UINT32_C(0x07230203)) {
    free(vertex);
    free(fragment);
    return NULL;
  }

  BLB_ShaderProgram *program = calloc(1, sizeof(*program));
  if (!program) {
    free(vertex);
    free(fragment);
    return NULL;
  }

  program->ref_count = 1;
  program->asset = *asset;
  program->vertex_code = vertex;
  program->fragment_code = fragment;
  program->vertex_size = vertex_size;
  program->fragment_size = fragment_size;
  program->revision = 1;
  shader_cache_insert(program);

  return program;
}

BLB_ShaderProgram *BLB_Shader_LoadSPIRV(const char *vertex_path, const char *fragment_path, bool is_2d) {
  BLB_ShaderAsset asset = is_2d ? BLB_Shader_Default2D() : BLB_Shader_Default3D();
  if (BLB_Shader_SetPaths(&asset, vertex_path, fragment_path, BLB_SHADER_SOURCE_SPIRV) != 0)
    return NULL;

  asset.is_2d = is_2d;
  return BLB_Shader_Load(&asset);
}

void BLB_Shader_Retain(BLB_ShaderProgram *shader) {
  if (shader)
    shader->ref_count++;
}

void BLB_Shader_Release(BLB_ShaderProgram *shader) {
  if (!shader)
    return;

  if (shader->ref_count > 1) {
    shader->ref_count--;
    return;
  }

  shader_cache_remove(shader);

  free(shader->vertex_code);
  free(shader->fragment_code);
  free(shader);
}

int BLB_Shader_Reload(BLB_ShaderProgram *shader) {
  if (!shader || shader->asset.source_type != BLB_SHADER_SOURCE_SPIRV)
    return -1;

  size_t vertex_size = 0;
  size_t fragment_size = 0;
  unsigned char *vertex = read_binary_file(shader->asset.vertex_path, &vertex_size);
  unsigned char *fragment = read_binary_file(shader->asset.fragment_path, &fragment_size);

  if (!vertex || !fragment || vertex_size < 4 || fragment_size < 4 || vertex_size % 4u != 0 || fragment_size % 4u != 0) {
    free(vertex);
    free(fragment);
    return -1;
  }
  uint32_t vertex_magic = 0, fragment_magic = 0;
  memcpy(&vertex_magic, vertex, sizeof(vertex_magic));
  memcpy(&fragment_magic, fragment, sizeof(fragment_magic));
  if (vertex_magic != UINT32_C(0x07230203) || fragment_magic != UINT32_C(0x07230203)) {
    free(vertex);
    free(fragment);
    return -1;
  }

  free(shader->vertex_code);
  free(shader->fragment_code);
  shader->vertex_code = vertex;
  shader->fragment_code = fragment;
  shader->vertex_size = vertex_size;
  shader->fragment_size = fragment_size;

  shader->revision++;
  if (shader->revision == 0)
    shader->revision = 1;

  return 0;
}

uint64_t BLB_Shader_GetRevision(const BLB_ShaderProgram *shader) { return shader ? shader->revision : 0; }
