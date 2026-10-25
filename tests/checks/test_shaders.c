#include "test.h"

static void spirv_cache_and_reload(void) {
  BLB_ShaderAsset asset = BLB_Shader_Default2D();

  TEST_CHECK(asset.is_2d && !BLB_Shader_IsCustom(&asset));
  BLB_ShaderAsset other = BLB_Shader_Default3D();

  TEST_CHECK(!other.is_2d);
  TEST_CHECK(BLB_Shader_SetPaths(&asset, BLB_TEST_SHADER_DIR "/basic2d.vert.spv", BLB_TEST_SHADER_DIR "/basic2d.frag.spv", BLB_SHADER_SOURCE_SPIRV) ==
             0);
  TEST_CHECK(BLB_Shader_IsCustom(&asset));
  BLB_ShaderProgram *shader = BLB_Shader_Load(&asset);

  TEST_REQUIRE(shader);
  TEST_CHECK(shader->vertex_size > 4 && shader->fragment_size > 4);
  BLB_ShaderProgram *cached = BLB_Shader_LoadSPIRV(asset.vertex_path, asset.fragment_path, true);

  TEST_CHECK(cached == shader);
  BLB_Shader_Release(cached);
  BLB_Shader_Retain(shader);
  TEST_CHECK(shader->ref_count == 2);
  BLB_Shader_Release(shader);
  uint64_t revision = BLB_Shader_GetRevision(shader);

  TEST_CHECK(BLB_Shader_Reload(shader) == 0);
  TEST_CHECK(BLB_Shader_GetRevision(shader) > revision);
  BLB_Shader_Release(shader);
  TEST_CHECK(BLB_Shader_LoadSPIRV("missing.vert.spv", "missing.frag.spv", true) == NULL);
  BLB_Shader_Release(NULL);
}

static const Test_Case cases[] = {TEST_CASE(spirv_cache_and_reload)};
const Test_Suite test_shaders = {"shaders", cases, sizeof(cases) / sizeof(cases[0])};
