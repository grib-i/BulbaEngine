#include "bulba/graphics/vulkan/renderer.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void pack_material_payload(float material_out[4], float pbr_out[4], float emission_out[4], uint32_t material_ext_out[4],
                                  uint32_t surface_out[8], uint32_t meta_out[4], const VulkanMaterial *material);

static HMM_Vec3 calculate_face_normal(HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c) { return HMM_Cross(HMM_SubV3(b, a), HMM_SubV3(c, a)); }

static HMM_Vec3 safe_normalize(HMM_Vec3 value, HMM_Vec3 fallback) {
  float length = HMM_LenV3(value);

  if (length <= 0.000001f)
    return fallback;

  return HMM_MulV3F(value, 1.0f / length);
}

static HMM_Vec3 calculate_normal(HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c) {
  return safe_normalize(calculate_face_normal(a, b, c), HMM_V3(0.0f, 1.0f, 0.0f));
}

static void upload_vertex(VulkanVertex *vertices, size_t index, HMM_Vec3 position, HMM_Vec3 normal, float r, float g, float b, float a, HMM_Vec2 uv) {
  vertices[index] = (VulkanVertex){
      .position =
          {
              position.x,
              position.y,
              position.z,
          },
      .color =
          {
              r,
              g,
              b,
              a,
          },
      .normal =
          {
              normal.x,
              normal.y,
              normal.z,
          },
      .uv =
          {
              uv.x,
              uv.y,
          },
  };
}

static uint32_t depth_variant(bool enabled, bool write) {
  if (enabled && write)
    return 3u;
  if (enabled)
    return 1u;
  if (write)
    return 2u;
  return 0u;
}

static VkPipeline select_3d_pipeline(VULKAN *vulkan, BLB_RenderMode mode, bool depth_enabled, bool depth_write) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_3d[mode][depth_variant(depth_enabled, depth_write)];
}

static VkPipelineLayout select_3d_layout(VULKAN *vulkan, BLB_RenderMode mode, bool depth_enabled, bool depth_write) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_layout_3d[mode][depth_variant(depth_enabled, depth_write)];
}

static VkPipeline select_2d_pipeline(VULKAN *vulkan, BLB_RenderMode mode, bool depth_enabled, bool depth_write) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_2d[mode][depth_variant(depth_enabled, depth_write)];
}

static VkPipelineLayout select_2d_layout(VULKAN *vulkan, BLB_RenderMode mode, bool depth_enabled, bool depth_write) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_layout_2d[mode][depth_variant(depth_enabled, depth_write)];
}

static void identity_rows(float rows[12]) {
  const float value[12] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };

  memcpy(rows, value, sizeof(value));
}

static bool find_cached_geometry_3d(VULKAN *vulkan, const BLB_Polygon3D *polygon, size_t *vertex_start, size_t *index_start) {
  if (!vulkan || !polygon || !vertex_start || !index_start || polygon->dynamic_vertices)
    return false;

  for (size_t i = 0; i < vulkan->geometry_cache_count; ++i) {
    const VULKAN_GeometryCacheEntry *entry = &vulkan->geometry_cache[i];
    if (entry->polygon != polygon)
      continue;
    if (entry->vertex_count != polygon->vertex_count || entry->index_count != polygon->index_count)
      continue;
    *vertex_start = entry->vertex_start;
    *index_start = entry->index_start;
    return true;
  }
  return false;
}

static void cache_geometry_3d(VULKAN *vulkan, const BLB_Polygon3D *polygon, size_t vertex_start, size_t index_start) {
  if (!vulkan || !polygon || polygon->dynamic_vertices || vulkan->geometry_cache_count >= VULKAN_MAX_GEOMETRY_CACHE_ENTRIES)
    return;

  vulkan->geometry_cache[vulkan->geometry_cache_count++] = (VULKAN_GeometryCacheEntry){
      .polygon = polygon,
      .vertex_start = vertex_start,
      .index_start = index_start,
      .vertex_count = polygon->vertex_count,
      .index_count = polygon->index_count,
  };
}

static bool ensure_polygon_normals(BLB_Polygon3D *polygon) {
  if (!polygon || !polygon->vertices || !polygon->indices || polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return false;

  if (polygon->normals)
    return true;

  HMM_Vec3 *normals = calloc(polygon->vertex_count, sizeof(*normals));

  if (!normals)
    return false;

  for (size_t i = 0; i < polygon->index_count; i += 3) {
    uint32_t ia = polygon->indices[i];
    uint32_t ib = polygon->indices[i + 1];
    uint32_t ic = polygon->indices[i + 2];

    if (ia >= polygon->vertex_count || ib >= polygon->vertex_count || ic >= polygon->vertex_count) {
      free(normals);
      return false;
    }

    HMM_Vec3 face_normal = calculate_face_normal(polygon->vertices[ia], polygon->vertices[ib], polygon->vertices[ic]);

    if (HMM_LenV3(face_normal) <= 0.000001f)
      continue;

    normals[ia] = HMM_AddV3(normals[ia], face_normal);
    normals[ib] = HMM_AddV3(normals[ib], face_normal);
    normals[ic] = HMM_AddV3(normals[ic], face_normal);
  }

  for (size_t i = 0; i < polygon->vertex_count; i++) {
    normals[i] = safe_normalize(normals[i], HMM_V3(0.0f, 1.0f, 0.0f));
  }

  polygon->normals = normals;

  return true;
}

static bool build_mesh_normals(const Mesh *mesh, HMM_Vec3 **output) {
  if (!mesh || !output || !mesh->vertices || !mesh->indices || mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return false;

  HMM_Vec3 *normals = calloc(mesh->vertex_count, sizeof(*normals));

  if (!normals)
    return false;

  for (size_t i = 0; i < mesh->index_count; i += 3) {
    size_t ia = mesh->indices[i];
    size_t ib = mesh->indices[i + 1];
    size_t ic = mesh->indices[i + 2];

    if (ia >= mesh->vertex_count || ib >= mesh->vertex_count || ic >= mesh->vertex_count) {
      free(normals);
      return false;
    }

    HMM_Vec3 face_normal = calculate_face_normal(mesh->vertices[ia], mesh->vertices[ib], mesh->vertices[ic]);

    if (HMM_LenV3(face_normal) <= 0.000001f)
      continue;

    normals[ia] = HMM_AddV3(normals[ia], face_normal);
    normals[ib] = HMM_AddV3(normals[ib], face_normal);
    normals[ic] = HMM_AddV3(normals[ic], face_normal);
  }

  for (size_t i = 0; i < mesh->vertex_count; i++) {
    normals[i] = safe_normalize(normals[i], HMM_V3(0.0f, 1.0f, 0.0f));
  }

  *output = normals;

  return true;
}

void VULKAN_RendererDrawTriangle(VULKAN *vulkan, HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color,
                                 HMM_Vec3 camera_position) {
  if (!vulkan || !mvp)
    return;

  if (vulkan->vertex_cursor + 3 > VULKAN_MAX_VERTICES)
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  if (!vulkan->main_render_pass_begun)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;

  if (!vertices)
    return;

  size_t start = vulkan->vertex_cursor;

  HMM_Vec3 normal = calculate_normal(a, b, c);

  upload_vertex(vertices, start, a, normal, r, g, b_color, a_color, HMM_V2(0.0f, 0.0f));
  upload_vertex(vertices, start + 1, b, normal, r, g, b_color, a_color, HMM_V2(1.0f, 0.0f));
  upload_vertex(vertices, start + 2, c, normal, r, g, b_color, a_color, HMM_V2(0.5f, 1.0f));

  float rows[12];
  identity_rows(rows);

  VulkanMaterial material = {
      .lighting_enabled = true,
      .depth_enabled = true,
      .depth_write = true,
      .double_sided = false,
      .unlit = false,
      .alpha_mode = BLB_ALPHA_OPAQUE,
      .alpha_cutoff = 0.5f,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .metallic = 0.0f,
      .roughness = 0.65f,
      .specular = 1.0f,
      .occlusion = 1.0f,
      .specular_color = {1.0f, 1.0f, 1.0f},
      .emission_color = {1.0f, 1.0f, 1.0f, 1.0f},
      .temperature = 6500.0f,
      .render_mode = BLB_RENDER_OPAQUE,
  };

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  VkBuffer buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;

  VkDeviceSize offset = start * sizeof(VulkanVertex);

  vulkan->camera_position = camera_position;

  update_light_buffer_3d(vulkan);

  VkPipeline pipeline = select_3d_pipeline(vulkan, material.render_mode, material.depth_enabled, material.depth_write);

  VkPipelineLayout layout = select_3d_layout(vulkan, material.render_mode, material.depth_enabled, material.depth_write);

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &buffer, &offset);
  VULKAN_RendererBindMaterial(vulkan, layout, &material);
  push_lighting(vulkan, layout, mvp, rows, &material, 0);
  vkCmdDraw(command, 3, 1, 0, 0);

  vulkan->vertex_cursor += 3;
}

void VULKAN_RendererDrawTriangle2D(VULKAN *vulkan, HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c, const float *mvp, float r, float g, float b_color,
                                   float a_color, HMM_Vec3 camera_position, int lighting_enabled) {
  if (!vulkan)
    return;

  HMM_Vec2 vertices[3] = {
      HMM_V2(a.x, a.y),
      HMM_V2(b.x, b.y),
      HMM_V2(c.x, c.y),
  };

  HMM_Vec2 uvs[3] = {
      HMM_V2(0.0f, 0.0f),
      HMM_V2(1.0f, 0.0f),
      HMM_V2(0.5f, 1.0f),
  };

  unsigned int indices[3] = {
      0,
      1,
      2,
  };

  BLB_Polygon2D polygon = {
      .vertices = vertices,
      .base_vertices = vertices,
      .uvs = uvs,
      .vertex_count = 3,
      .indices = indices,
      .index_count = 3,
  };

  VulkanMaterial material = {
      .lighting_enabled = lighting_enabled != 0,
      .depth_enabled = false,
      .depth_write = false,
      .double_sided = true,
      .unlit = lighting_enabled == 0,
      .alpha_mode = BLB_ALPHA_OPAQUE,
      .alpha_cutoff = 0.5f,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .metallic = 0.0f,
      .roughness = 0.65f,
      .specular = 1.0f,
      .occlusion = 1.0f,
      .specular_color = {1.0f, 1.0f, 1.0f},
      .emission_color = {1.0f, 1.0f, 1.0f, 1.0f},
      .temperature = 6500.0f,
      .render_mode = BLB_RENDER_OPAQUE,
  };

  (void)mvp;
  (void)camera_position;

  HMM_Vec2 world_positions[3] = {
      vertices[0],
      vertices[1],
      vertices[2],
  };

  VULKAN_RendererDrawPolygon2D(vulkan, &polygon, world_positions, (float)vulkan->swapchain_extent.width, (float)vulkan->swapchain_extent.height, r, g,
                               b_color, a_color, &material, NULL);
}

void VULKAN_RendererDrawMesh(VULKAN *vulkan, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, HMM_Vec3 camera_position) {
  if (!vulkan || !mesh || !mesh->vertices || !mesh->indices || !mvp)
    return;

  if (mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return;

  if (vulkan->vertex_cursor + mesh->vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + mesh->index_count > VULKAN_MAX_INDICES)
    return;

  for (size_t i = 0; i < mesh->index_count; i++) {
    if (mesh->indices[i] >= mesh->vertex_count)
      return;
  }

  HMM_Vec3 *computed_normals = NULL;

  if (!mesh->normals && !build_mesh_normals(mesh, &computed_normals))
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  if (!vulkan->main_render_pass_begun) {
    free(computed_normals);
    return;
  }

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;

  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;

  if (!vertices || !indices) {
    free(computed_normals);
    return;
  }

  size_t vertex_start = vulkan->vertex_cursor;
  size_t index_start = vulkan->index_cursor;

  for (size_t i = 0; i < mesh->vertex_count; i++) {
    HMM_Vec3 normal = mesh->normals ? safe_normalize(mesh->normals[i], HMM_V3(0.0f, 1.0f, 0.0f)) : computed_normals[i];
    HMM_Vec2 uv = mesh->uvs ? mesh->uvs[i] : HMM_V2(0.0f, 0.0f);

    upload_vertex(vertices, vertex_start + i, mesh->vertices[i], normal, r, g, b, a, uv);
  }

  for (size_t i = 0; i < mesh->index_count; i++) {
    indices[index_start + i] = (uint32_t)mesh->indices[i];
  }

  free(computed_normals);

  float rows[12];
  identity_rows(rows);

  VulkanMaterial material = {
      .lighting_enabled = true,
      .depth_enabled = true,
      .depth_write = true,
      .double_sided = false,
      .unlit = false,
      .alpha_mode = BLB_ALPHA_OPAQUE,
      .alpha_cutoff = 0.5f,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .metallic = 0.0f,
      .roughness = 0.65f,
      .specular = 1.0f,
      .occlusion = 1.0f,
      .specular_color = {1.0f, 1.0f, 1.0f},
      .emission_color = {1.0f, 1.0f, 1.0f, 1.0f},
      .temperature = 6500.0f,
      .render_mode = BLB_RENDER_OPAQUE,
  };

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;

  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);

  vulkan->camera_position = camera_position;

  update_light_buffer_3d(vulkan);

  VkPipeline pipeline = select_3d_pipeline(vulkan, material.render_mode, material.depth_enabled, material.depth_write);

  VkPipelineLayout layout = select_3d_layout(vulkan, material.render_mode, material.depth_enabled, material.depth_write);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }

  if (vulkan->bound_vertex_buffer != vertex_buffer || vulkan->bound_vertex_offset != offset) {
    vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);
    vulkan->bound_vertex_buffer = vertex_buffer;
    vulkan->bound_vertex_offset = offset;
  }

  VkBuffer index_buffer = vulkan->index_buffers[vulkan->current_frame].buffer;
  if (vulkan->bound_index_buffer != index_buffer) {
    vkCmdBindIndexBuffer(command, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = index_buffer;
  }

  VULKAN_RendererBindMaterial(vulkan, layout, &material);

  push_lighting(vulkan, layout, mvp, rows, &material, 0);

  vkCmdDrawIndexed(command, (uint32_t)mesh->index_count, 1, (uint32_t)index_start, 0, 0);
  vulkan->vertex_cursor += mesh->vertex_count;

  vulkan->index_cursor += mesh->index_count;
}

void VULKAN_RendererDrawPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g, float b,
                                  float a, const VulkanMaterial *material, BLB_Texture *texture) {
  if (!vulkan || !polygon || !mvp || !model_rows || !material || !polygon->vertices || !polygon->indices)
    return;

  if (polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  if (!vulkan->main_render_pass_begun)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;

  if (!vertices || !indices)
    return;

  size_t vertex_start = 0;
  size_t index_start = 0;
  bool cached_geometry = find_cached_geometry_3d(vulkan, polygon, &vertex_start, &index_start);

  if (!cached_geometry) {
    /* Stop early if this frame has no room; no need to scan an oversized
     * mesh on every glow pass. */
    if (vulkan->vertex_cursor + polygon->vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + polygon->index_count > VULKAN_MAX_INDICES)
      return;

    /* Validate and prepare geometry only on the first draw this frame. Glow
     * and other secondary passes reuse the same cached range, so rescanning a
     * large FBX index array for every pass needlessly stalls the CPU. */
    for (size_t i = 0; i < polygon->index_count; i++) {
      if (polygon->indices[i] >= polygon->vertex_count)
        return;
    }

    if (!ensure_polygon_normals((BLB_Polygon3D *)polygon))
      return;

    vertex_start = vulkan->vertex_cursor;
    index_start = vulkan->index_cursor;

    for (size_t i = 0; i < polygon->vertex_count; i++) {
      HMM_Vec3 normal = safe_normalize(polygon->normals[i], HMM_V3(0.0f, 1.0f, 0.0f));
      HMM_Vec2 uv = polygon->uvs ? polygon->uvs[i] : HMM_V2(0.0f, 0.0f);
      upload_vertex(vertices, vertex_start + i, polygon->vertices[i], normal, r, g, b, a, uv);
    }

    for (size_t i = 0; i < polygon->index_count; i++)
      indices[index_start + i] = (uint32_t)polygon->indices[i];

    cache_geometry_3d(vulkan, polygon, vertex_start, index_start);

    vulkan->vertex_cursor += polygon->vertex_count;
    vulkan->index_cursor += polygon->index_count;
  }

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);

  uint32_t variant = depth_variant(material->depth_enabled, material->depth_write);
  VkPipeline pipeline = select_3d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_3d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);

  if (material->source_material && material->source_material->shader_program && material->source_material->shader_program->asset.is_2d == false &&
      VULKAN_GetOrCreateCustomPipeline(vulkan, material->source_material->shader_program, false, material->render_mode, variant, &pipeline,
                                       &layout) != 0) {
    pipeline = select_3d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
    layout = select_3d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  }

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }

  if (vulkan->bound_vertex_buffer != vertex_buffer || vulkan->bound_vertex_offset != offset) {
    vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);
    vulkan->bound_vertex_buffer = vertex_buffer;
    vulkan->bound_vertex_offset = offset;
  }

  VkBuffer index_buffer = vulkan->index_buffers[vulkan->current_frame].buffer;
  if (vulkan->bound_index_buffer != index_buffer) {
    vkCmdBindIndexBuffer(command, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = index_buffer;
  }

  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;
  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);

  /* Push constants must describe the same material/texture that was bound.
   * In particular, material_texture_mask() reads this struct to tell the
   * fragment shader which texture slots are present. */
  push_lighting(vulkan, layout, mvp, model_rows, &bound_material, 0);

  vkCmdDrawIndexed(command, (uint32_t)polygon->index_count, 1, (uint32_t)index_start, 0, 0);
}

void VULKAN_RendererDrawPolygon2D(VULKAN *vulkan, const BLB_Polygon2D *polygon, const HMM_Vec2 *world_positions, float viewport_width,
                                  float viewport_height, float r, float g, float b, float a, const VulkanMaterial *material, BLB_Texture *texture) {
  if (!vulkan || !polygon || !material || !polygon->vertices || !polygon->indices)
    return;

  if (polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;

  if (vulkan->vertex_cursor + polygon->vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + polygon->index_count > VULKAN_MAX_INDICES)
    return;

  for (size_t i = 0; i < polygon->index_count; i++) {
    if (polygon->indices[i] >= polygon->vertex_count)
      return;
  }

  VULKAN_RendererBeginMainPass(vulkan);

  if (!vulkan->main_render_pass_begun)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;

  if (!vertices || !indices)
    return;

  size_t vertex_start = vulkan->vertex_cursor;
  size_t index_start = vulkan->index_cursor;

  for (size_t i = 0; i < polygon->vertex_count; i++) {
    HMM_Vec2 uv = polygon->uvs ? polygon->uvs[i] : HMM_V2(0.0f, 0.0f);

    vertices[vertex_start + i] = (VulkanVertex){
        .position =
            {
                polygon->vertices[i].x,
                polygon->vertices[i].y,
                0.0f,
            },
        .color =
            {
                r,
                g,
                b,
                a,
            },
        .normal =
            {
                world_positions ? world_positions[i].x : polygon->vertices[i].x,
                world_positions ? world_positions[i].y : polygon->vertices[i].y,
                0.0f,
            },
        .uv =
            {
                uv.x,
                uv.y,
            },
    };
  }

  for (size_t i = 0; i < polygon->index_count; i++)
    indices[index_start + i] = (uint32_t)polygon->indices[i];

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  uint32_t variant = depth_variant(material->depth_enabled, material->depth_write);
  VkPipeline pipeline = select_2d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_2d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);

  if (material->source_material && material->source_material->shader_program && material->source_material->shader_program->asset.is_2d == true &&
      VULKAN_GetOrCreateCustomPipeline(vulkan, material->source_material->shader_program, true, material->render_mode, variant, &pipeline, &layout) !=
          0) {
    pipeline = select_2d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
    layout = select_2d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  }

  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;

  Vulkan2DPushConstants push = {0};
  push.viewport[0] = viewport_width;
  push.viewport[1] = viewport_height;
  pack_material_payload(push.material, push.pbr, push.emission, push.material_ext, push.surface, push.meta, &bound_material);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }

  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);
  if (vulkan->bound_vertex_buffer != vulkan->vertex_buffers[vulkan->current_frame].buffer || vulkan->bound_vertex_offset != offset) {
    VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
    vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);
    vulkan->bound_vertex_buffer = vertex_buffer;
    vulkan->bound_vertex_offset = offset;
  }

  VkBuffer index_buffer = vulkan->index_buffers[vulkan->current_frame].buffer;

  if (vulkan->bound_index_buffer != index_buffer) {
    vkCmdBindIndexBuffer(command, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = index_buffer;
  }

  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);

  VkDescriptorSet light_set = vulkan->light_descriptor_sets_2d[vulkan->current_frame];

  if (vulkan->bound_light_set != light_set || vulkan->bound_light_layout != layout) {
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &light_set, 0, NULL);
    vulkan->bound_light_set = light_set;
    vulkan->bound_light_layout = layout;
  }

  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
  vkCmdDrawIndexed(command, (uint32_t)polygon->index_count, 1, (uint32_t)index_start, 0, 0);

  vulkan->vertex_cursor += polygon->vertex_count;
  vulkan->index_cursor += polygon->index_count;
}

void VULKAN_RendererDrawInstancedPolygon2D(VULKAN *vulkan, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                           const VulkanMaterial *material, BLB_Texture *texture, const VulkanInstanceData *instances,
                                           size_t instance_count, bool screen_space) {
  if (!vulkan || !polygon || !material || !instances || instance_count == 0 || instance_count > UINT32_MAX || !polygon->vertices ||
      !polygon->indices || polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;
  if (vulkan->instance_cursor + instance_count > vulkan->instance_capacity)
    return;
  if (vulkan->vertex_cursor + polygon->vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + polygon->index_count > VULKAN_MAX_INDICES)
    return;
  for (size_t i = 0; i < polygon->index_count; ++i)
    if (polygon->indices[i] >= polygon->vertex_count)
      return;

  VULKAN_RendererBeginMainPass(vulkan);
  if (!vulkan->main_render_pass_begun)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;
  VulkanInstanceData *mapped = (VulkanInstanceData *)vulkan->instance_buffers[vulkan->current_frame].mapped;
  if (!vertices || !indices || !mapped)
    return;

  size_t vertex_start = vulkan->vertex_cursor;
  size_t index_start = vulkan->index_cursor;
  const float r = 1.0f;
  const float g = 1.0f;
  const float b = 1.0f;
  const float a = 1.0f;
  for (size_t i = 0; i < polygon->vertex_count; ++i) {
    HMM_Vec2 uv = polygon->uvs ? polygon->uvs[i] : HMM_V2(0.0f, 0.0f);
    vertices[vertex_start + i] = (VulkanVertex){
        .position = {polygon->vertices[i].x, polygon->vertices[i].y, 0.0f},
        .color = {r, g, b, a},
        .normal = {0.0f, 1.0f, 0.0f},
        .uv = {uv.x, uv.y},
    };
  }
  for (size_t i = 0; i < polygon->index_count; ++i)
    indices[index_start + i] = polygon->indices[i];

  uint32_t instance_base = (uint32_t)vulkan->instance_cursor;
  memcpy(mapped + vulkan->instance_cursor, instances, instance_count * sizeof(*instances));
  vulkan->instance_cursor += instance_count;
  vulkan->vertex_cursor += polygon->vertex_count;
  vulkan->index_cursor += polygon->index_count;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkPipeline pipeline = select_2d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_2d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }
  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);
  if (vulkan->bound_vertex_buffer != vulkan->vertex_buffers[vulkan->current_frame].buffer || vulkan->bound_vertex_offset != offset) {
    VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
    vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);
    vulkan->bound_vertex_buffer = vertex_buffer;
    vulkan->bound_vertex_offset = offset;
  }
  VkBuffer index_buffer = vulkan->index_buffers[vulkan->current_frame].buffer;
  if (vulkan->bound_index_buffer != index_buffer) {
    vkCmdBindIndexBuffer(command, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = index_buffer;
  }
  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;
  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);

  VkDescriptorSet light_set = vulkan->light_descriptor_sets_2d[vulkan->current_frame];
  if (vulkan->bound_light_set != light_set || vulkan->bound_light_layout != layout) {
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &light_set, 0, NULL);
    vulkan->bound_light_set = light_set;
    vulkan->bound_light_layout = layout;
  }

  Vulkan2DPushConstants push = {0};
  push.viewport[0] = viewport_width;
  push.viewport[1] = viewport_height;
  pack_material_payload(push.material, push.pbr, push.emission, push.material_ext, push.surface, push.meta, &bound_material);
  push.meta[2] |= 1u | (screen_space ? 2u : 0u);
  push.meta[3] = instance_base;
  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
  vkCmdDrawIndexed(command, (uint32_t)polygon->index_count, (uint32_t)instance_count, (uint32_t)index_start, 0, 0);
}

static bool find_cached_shadow_geometry(VULKAN *vulkan, const BLB_Polygon3D *polygon, size_t *vertex_start) {
  if (!vulkan || !polygon || !vertex_start)
    return false;
  for (size_t i = 0; i < vulkan->shadow_geometry_cache_count; ++i) {
    const VULKAN_ShadowGeometryCacheEntry *entry = &vulkan->shadow_geometry_cache[i];
    if (entry->polygon == polygon && entry->vertex_count == polygon->index_count) {
      *vertex_start = entry->vertex_start;
      return true;
    }
  }
  return false;
}

static void cache_shadow_geometry(VULKAN *vulkan, const BLB_Polygon3D *polygon, size_t vertex_start) {
  if (!vulkan || !polygon || vulkan->shadow_geometry_cache_count >= VULKAN_MAX_SHADOW_GEOMETRY_CACHE_ENTRIES)
    return;
  vulkan->shadow_geometry_cache[vulkan->shadow_geometry_cache_count++] = (VULKAN_ShadowGeometryCacheEntry){
      .polygon = polygon,
      .vertex_start = vertex_start,
      .vertex_count = polygon->index_count,
  };
}

void VULKAN_RendererDrawBatchedPolygon2D(VULKAN *vulkan, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                         const VulkanMaterial *material, BLB_Texture *texture, const VulkanInstanceData *instances,
                                         size_t instance_count, bool screen_space) {
  if (!vulkan || !polygon || !material || !instances || instance_count == 0 || !polygon->vertices || !polygon->indices ||
      polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;
  if (instance_count > VULKAN_MAX_VERTICES / polygon->vertex_count || instance_count > VULKAN_MAX_INDICES / polygon->index_count)
    return;

  const size_t total_vertices = polygon->vertex_count * instance_count;
  const size_t total_indices = polygon->index_count * instance_count;
  if (vulkan->vertex_cursor > VULKAN_MAX_VERTICES - total_vertices || vulkan->index_cursor > VULKAN_MAX_INDICES - total_indices ||
      total_indices > UINT32_MAX)
    return;

  for (size_t i = 0; i < polygon->index_count; ++i)
    if (polygon->indices[i] >= polygon->vertex_count)
      return;

  VULKAN_RendererBeginMainPass(vulkan);
  if (!vulkan->main_render_pass_begun)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;
  if (!vertices || !indices)
    return;

  const size_t vertex_start = vulkan->vertex_cursor;
  const size_t index_start = vulkan->index_cursor;

  for (size_t instance_index = 0; instance_index < instance_count; ++instance_index) {
    const VulkanInstanceData *instance = &instances[instance_index];
    const float *m = instance->model;
    const size_t base_vertex = instance_index * polygon->vertex_count;

    for (size_t vertex_index = 0; vertex_index < polygon->vertex_count; ++vertex_index) {
      const HMM_Vec2 local = polygon->vertices[vertex_index];
      const HMM_Vec2 uv = polygon->uvs ? polygon->uvs[vertex_index] : HMM_V2(0.0f, 0.0f);
      const float x = m[0] * local.x + m[4] * local.y + m[12];
      const float y = m[1] * local.x + m[5] * local.y + m[13];
      const float z = m[2] * local.x + m[6] * local.y + m[14];
      vertices[vertex_start + base_vertex + vertex_index] = (VulkanVertex){
          .position = {x, y, z},
          .color = {instance->color[0], instance->color[1], instance->color[2], instance->color[3]},
          .normal = {0.0f, 0.0f, 1.0f},
          .uv = {uv.x, uv.y},
      };
    }

    for (size_t index = 0; index < polygon->index_count; ++index)
      indices[index_start + instance_index * polygon->index_count + index] = (uint32_t)(base_vertex + polygon->indices[index]);
  }

  vulkan->vertex_cursor += total_vertices;
  vulkan->index_cursor += total_indices;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkPipeline pipeline = select_2d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_2d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }

  VkDeviceSize vertex_offset = vertex_start * sizeof(VulkanVertex);
  if (vulkan->bound_vertex_buffer != vulkan->vertex_buffers[vulkan->current_frame].buffer || vulkan->bound_vertex_offset != vertex_offset) {
    VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
    vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &vertex_offset);
    vulkan->bound_vertex_buffer = vertex_buffer;
    vulkan->bound_vertex_offset = vertex_offset;
  }

  VkBuffer index_buffer = vulkan->index_buffers[vulkan->current_frame].buffer;
  if (vulkan->bound_index_buffer != index_buffer) {
    vkCmdBindIndexBuffer(command, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = index_buffer;
  }

  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;
  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);

  VkDescriptorSet light_set = vulkan->light_descriptor_sets_2d[vulkan->current_frame];
  if (vulkan->bound_light_set != light_set || vulkan->bound_light_layout != layout) {
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &light_set, 0, NULL);
    vulkan->bound_light_set = light_set;
    vulkan->bound_light_layout = layout;
  }

  Vulkan2DPushConstants push = {0};
  push.viewport[0] = viewport_width;
  push.viewport[1] = viewport_height;
  pack_material_payload(push.material, push.pbr, push.emission, push.material_ext, push.surface, push.meta, &bound_material);
  push.meta[2] |= 8u | (screen_space ? 2u : 0u);
  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
  vkCmdDrawIndexed(command, (uint32_t)total_indices, 1, (uint32_t)index_start, 0, 0);
}

void VULKAN_RendererDrawShadowPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *model_mvp) {
  if (!vulkan || !polygon || !model_mvp || !polygon->vertices || !polygon->indices || polygon->index_count < 3)
    return;

  if (polygon->vertex_count == 0 || polygon->index_count > VULKAN_MAX_INDICES || polygon->index_count % 3 != 0)
    return;

  if (!vulkan->shadow_pipeline || vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;

  for (size_t i = 0; i < polygon->index_count; i++) {
    if (polygon->indices[i] >= polygon->vertex_count)
      return;
  }

  size_t vertex_start = 0;
  if (!find_cached_shadow_geometry(vulkan, polygon, &vertex_start)) {
    if (vulkan->shadow_vertex_cursor + polygon->index_count > VULKAN_MAX_INDICES)
      return;

    VulkanShadowVertex *vertices = (VulkanShadowVertex *)vulkan->shadow_vertex_buffers[vulkan->current_frame].mapped;
    if (!vertices)
      return;

    vertex_start = vulkan->shadow_vertex_cursor;
    for (size_t i = 0; i < polygon->index_count; i++) {
      uint32_t index = polygon->indices[i];
      HMM_Vec3 p = polygon->vertices[index];
      vertices[vertex_start + i] = (VulkanShadowVertex){{p.x, p.y, p.z}};
    }
    vulkan->shadow_vertex_cursor += polygon->index_count;
    cache_shadow_geometry(vulkan, polygon, vertex_start);
  }

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkDeviceSize offset = vertex_start * sizeof(VulkanShadowVertex);
  VulkanShadowPushConstants push = {0};
  memcpy(push.mvp, model_mvp, sizeof(push.mvp));

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &vulkan->shadow_vertex_buffers[vulkan->current_frame].buffer, &offset);
  VkDescriptorSet descriptor_set = vulkan->light_descriptor_sets_3d[vulkan->current_frame];
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
  vkCmdPushConstants(command, vulkan->shadow_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
  vkCmdDraw(command, (uint32_t)polygon->index_count, 1, (uint32_t)vertex_start, 0);
}

static uint16_t pack_unorm16(float value, float min_value, float max_value) {
  if (!(max_value > min_value))
    return 0;

  float t = (value - min_value) / (max_value - min_value);
  t = fminf(fmaxf(t, 0.0f), 1.0f);
  return (uint16_t)lroundf(t * 65535.0f);
}

static uint32_t pack_pair16(float a, float a_min, float a_max, float b, float b_min, float b_max) {
  uint32_t pa = pack_unorm16(a, a_min, a_max);
  uint32_t pb = pack_unorm16(b, b_min, b_max);
  return pa | (pb << 16u);
}

static uint32_t pack_rgb10(const float color[3]) {
  uint32_t r = (uint32_t)lroundf(fminf(fmaxf(color[0], 0.0f), 1.0f) * 1023.0f);
  uint32_t g = (uint32_t)lroundf(fminf(fmaxf(color[1], 0.0f), 1.0f) * 1023.0f);
  uint32_t b = (uint32_t)lroundf(fminf(fmaxf(color[2], 0.0f), 1.0f) * 1023.0f);
  return r | (g << 10u) | (b << 20u);
}

static uint32_t material_texture_mask(const VulkanMaterial *material) {
  if (!material)
    return 0;

  const BLB_Material *m = material->source_material;
  if (!m) {
    if (!material->base_texture_override)
      return 0;
    return material->base_texture_override->type == BLB_TEXTURE_3D ? (1u << 31u) : 1u;
  }
  uint32_t mask = 0;
  const BLB_MaterialTexture *slots[] = {
      &m->base_color_texture,
      &m->metallic_roughness_texture,
      &m->normal_texture,
      &m->occlusion_texture,
      &m->emission_texture,
      &m->specular_texture,
      &m->specular_color_texture,
      &m->clearcoat_texture,
      &m->clearcoat_roughness_texture,
      &m->clearcoat_normal_texture,
      &m->transmission_texture,
      &m->thickness_texture,
      &m->sheen_color_texture,
      &m->sheen_roughness_texture,
      &m->iridescence_texture,
      &m->iridescence_thickness_texture,
      &m->anisotropy_texture,
  };

  for (uint32_t i = 0; i < 17u; i++) {
    if (slots[i] && slots[i]->texture)
      mask |= 1u << i;
  }

  if (material->base_texture_override) {
    if (material->base_texture_override->type == BLB_TEXTURE_3D) {
      mask &= ~1u;
      mask |= (1u << 31u);
    } else if (!(mask & 1u)) {
      mask |= 1u;
    }
  } else if (slots[0] && slots[0]->texture && slots[0]->texture->type == BLB_TEXTURE_3D) {
    mask &= ~1u;
    mask |= (1u << 31u);
  }

  return mask;
}

static void pack_material_payload(float material_out[4], float pbr_out[4], float emission_out[4], uint32_t material_ext_out[4],
                                  uint32_t surface_out[8], uint32_t meta_out[4], const VulkanMaterial *material) {
  if (!material)
    return;

  uint32_t flags = 0;
  if (material->double_sided)
    flags |= 1u << 0u;
  if (material->unlit)
    flags |= 1u << 1u;
  flags |= ((uint32_t)material->alpha_mode & 0x3u) << 2u;
  if (material->depth_enabled)
    flags |= 1u << 4u;
  if (material->depth_write)
    flags |= 1u << 5u;
  uint32_t cutoff = (uint32_t)lroundf(fminf(fmaxf(material->alpha_cutoff, 0.0f), 1.0f) * 1023.0f);
  flags |= (cutoff & 0x3ffu) << 6u;

  material_out[0] = material->lighting_enabled ? 1.0f : 0.0f;
  material_out[1] = fmaxf(material->emission, 0.0f);
  material_out[2] = fmaxf(material->glow, 0.0f);
  material_out[3] = 0.0f;

  pbr_out[0] = fminf(fmaxf(material->metallic, 0.0f), 1.0f);
  pbr_out[1] = fminf(fmaxf(material->roughness, 0.045f), 1.0f);
  pbr_out[2] = fminf(fmaxf(material->specular, 0.0f), 1.0f);
  pbr_out[3] = fminf(fmaxf(material->occlusion, 0.0f), 1.0f);

  emission_out[0] = material->emission_color[0];
  emission_out[1] = material->emission_color[1];
  emission_out[2] = material->emission_color[2];
  emission_out[3] = material->temperature;

  material_ext_out[0] = pack_rgb10(material->specular_color);
  material_ext_out[1] = pack_rgb10(material->attenuation_color);
  material_ext_out[2] = pack_rgb10(material->sheen_color);
  union {
    uint32_t u;
    float f;
  } entity_bits;
  entity_bits.f = material->entity_id;
  material_ext_out[3] = entity_bits.u;

  surface_out[0] = pack_pair16(material->normal_scale, 0.0f, 4.0f, material->ior, 1.0f, 4.0f);
  surface_out[1] = pack_pair16(material->transmission, 0.0f, 1.0f, material->volume_thickness, 0.0f, 100.0f);
  surface_out[2] = pack_pair16(material->attenuation_distance, 0.0f, 1000.0f, material->clearcoat_factor, 0.0f, 1.0f);
  surface_out[3] = pack_pair16(material->clearcoat_roughness, 0.0f, 1.0f, material->clearcoat_normal_scale, 0.0f, 4.0f);
  surface_out[4] = pack_pair16(material->sheen_roughness, 0.0f, 1.0f, material->iridescence_factor, 0.0f, 1.0f);
  surface_out[5] = pack_pair16(material->iridescence_ior, 1.0f, 4.0f, material->iridescence_thickness_min, 0.0f, 2000.0f);
  surface_out[6] = pack_pair16(material->iridescence_thickness_max, 0.0f, 2000.0f, material->anisotropy_strength, 0.0f, 1.0f);
  surface_out[7] = pack_pair16(material->anisotropy_rotation, -3.14159265358979323846f, 3.14159265358979323846f, material->dispersion, 0.0f, 1.0f);
  memset(meta_out, 0, sizeof(uint32_t) * 4u);
  meta_out[0] = material_texture_mask(material);
  meta_out[1] = flags;
  if (material->particle)
    meta_out[2] |= 1u << 2u;
  if (material->skybox)
    meta_out[2] |= 1u << 3u;
}

static void push_lighting_ex(VULKAN *vulkan, VkPipelineLayout layout, const float *mvp, const float *model_rows, const VulkanMaterial *material,
                             int is_2d, bool instanced, uint32_t instance_base) {
  if (!vulkan || !mvp || !model_rows || !material || vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;

  VulkanLightingPushConstants data = {0};
  memcpy(data.mvp, mvp, sizeof(data.mvp));
  memcpy(data.model_rows, model_rows, sizeof(data.model_rows));
  pack_material_payload(data.material, data.pbr, data.emission, data.material_ext, data.surface, data.meta, material);
  data.meta[2] |= instanced ? 1u : 0u;
  data.meta[3] = instance_base;
  for (int i = 0; i < 3; ++i) {
    data.uv_bounds_min[i] = material->uv_bounds_min[i];
    data.uv_bounds_max[i] = material->uv_bounds_max[i];
  }

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkDescriptorSet descriptor_set =
      is_2d ? vulkan->light_descriptor_sets_2d[vulkan->current_frame] : vulkan->light_descriptor_sets_3d[vulkan->current_frame];

  if (vulkan->bound_light_set != descriptor_set || vulkan->bound_light_layout != layout) {
    vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor_set, 0, NULL);
    vulkan->bound_light_set = descriptor_set;
    vulkan->bound_light_layout = layout;
  }
  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data), &data);
}

void push_lighting(VULKAN *vulkan, VkPipelineLayout layout, const float *mvp, const float *model_rows, const VulkanMaterial *material, int is_2d) {
  push_lighting_ex(vulkan, layout, mvp, model_rows, material, is_2d, false, 0);
}

static VULKAN_MeshCacheEntry *mesh_cache = NULL;
static size_t mesh_cache_count = 0;
static size_t mesh_cache_capacity = 0;

static VulkanMaterial create_cached_mesh_material(void) {
  VulkanMaterial material = {
      .lighting_enabled = true,
      .depth_enabled = true,
      .depth_write = true,
      .double_sided = false,
      .unlit = false,
      .alpha_mode = BLB_ALPHA_OPAQUE,
      .alpha_cutoff = 0.5f,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .metallic = 0.0f,
      .roughness = 0.65f,
      .specular = 1.0f,
      .occlusion = 1.0f,
      .specular_color = {1.0f, 1.0f, 1.0f},
      .emission_color = {1.0f, 1.0f, 1.0f, 1.0f},
      .temperature = 6500.0f,
      .render_mode = BLB_RENDER_OPAQUE,
  };

  return material;
}

static VULKAN_MeshCacheEntry *find_mesh_cache_entry(VULKAN *vulkan, const Mesh *mesh) {
  if (!vulkan || !mesh)
    return NULL;

  for (size_t i = 0; i < mesh_cache_count; i++) {
    if (mesh_cache[i].renderer == vulkan && mesh_cache[i].mesh == mesh)
      return &mesh_cache[i];
  }

  return NULL;
}

static bool ensure_mesh_cache_capacity(void) {
  if (mesh_cache_count < mesh_cache_capacity)
    return true;

  size_t new_capacity = mesh_cache_capacity ? mesh_cache_capacity * 2 : 64;

  VULKAN_MeshCacheEntry *entries = realloc(mesh_cache, new_capacity * sizeof(*entries));

  if (!entries)
    return false;

  mesh_cache = entries;
  mesh_cache_capacity = new_capacity;

  return true;
}

static void destroy_cached_mesh(VULKAN *vulkan, VULKAN_CachedMesh *cached) {
  if (!vulkan || !cached)
    return;

  destroy_buffer(vulkan, &cached->vertex);
  destroy_buffer(vulkan, &cached->index);
  destroy_buffer(vulkan, &cached->shadow);

  memset(cached, 0, sizeof(*cached));
}

static bool upload_cached_mesh_main(VULKAN *vulkan, const Mesh *mesh, float r, float g, float b, float a, VULKAN_CachedMesh *cached) {
  if (!vulkan || !mesh || !cached || !mesh->vertices || !mesh->indices)
    return false;

  if (mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return false;

  if (mesh->vertex_count > UINT32_MAX || mesh->index_count > UINT32_MAX)
    return false;

  for (size_t i = 0; i < mesh->index_count; i++) {
    if (mesh->indices[i] >= mesh->vertex_count)
      return false;
  }

  HMM_Vec3 *computed_normals = NULL;

  if (!mesh->normals && !build_mesh_normals(mesh, &computed_normals))
    return false;

  VkDeviceSize vertex_size = mesh->vertex_count * sizeof(VulkanVertex);
  VkDeviceSize index_size = mesh->index_count * sizeof(uint32_t);

  if (create_buffer(vulkan, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &cached->vertex) != 0)
    goto fail;

  if (create_buffer(vulkan, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &cached->index) != 0)
    goto fail;

  if (!cached->vertex.mapped || !cached->index.mapped)
    goto fail;

  VulkanVertex *vertices = (VulkanVertex *)cached->vertex.mapped;

  uint32_t *indices = (uint32_t *)cached->index.mapped;

  for (size_t i = 0; i < mesh->vertex_count; i++) {
    HMM_Vec3 normal = mesh->normals ? safe_normalize(mesh->normals[i], HMM_V3(0.0f, 1.0f, 0.0f)) : computed_normals[i];

    HMM_Vec2 uv = mesh->uvs ? mesh->uvs[i] : HMM_V2(0.0f, 0.0f);

    upload_vertex(vertices, i, mesh->vertices[i], normal, r, g, b, a, uv);
  }

  for (size_t i = 0; i < mesh->index_count; i++)
    indices[i] = mesh->indices[i];

  cached->vertex_count = (uint32_t)mesh->vertex_count;

  cached->index_count = (uint32_t)mesh->index_count;

  cached->color[0] = r;
  cached->color[1] = g;
  cached->color[2] = b;
  cached->color[3] = a;
  cached->main_valid = true;

  free(computed_normals);

  return true;
fail:
  free(computed_normals);
  destroy_buffer(vulkan, &cached->vertex);
  destroy_buffer(vulkan, &cached->index);
  return false;
}

static bool upload_cached_mesh_shadow(VULKAN *vulkan, const Mesh *mesh, VULKAN_CachedMesh *cached) {
  if (!vulkan || !mesh || !cached || !mesh->vertices || !mesh->indices)
    return false;

  if (mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return false;

  for (size_t i = 0; i < mesh->index_count; i++) {
    if (mesh->indices[i] >= mesh->vertex_count)
      return false;
  }

  VkDeviceSize shadow_size = mesh->index_count * sizeof(VulkanShadowVertex);

  if (create_buffer(vulkan, shadow_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &cached->shadow) != 0)
    return false;

  if (!cached->shadow.mapped) {
    destroy_buffer(vulkan, &cached->shadow);
    return false;
  }

  VulkanShadowVertex *vertices = (VulkanShadowVertex *)cached->shadow.mapped;

  for (size_t i = 0; i < mesh->index_count; i++) {
    uint32_t index = mesh->indices[i];

    HMM_Vec3 position = mesh->vertices[index];

    vertices[i] = (VulkanShadowVertex){
        .position =
            {
                position.x,
                position.y,
                position.z,
            },
    };
  }

  cached->shadow_vertex_count = (uint32_t)mesh->index_count;
  cached->shadow_valid = true;

  return true;
}

static bool ensure_cached_mesh_entry(VULKAN *vulkan, const Mesh *mesh, VULKAN_MeshCacheEntry **entry_out) {
  if (!vulkan || !mesh || !entry_out)
    return false;

  VULKAN_MeshCacheEntry *entry = find_mesh_cache_entry(vulkan, mesh);

  if (entry) {
    *entry_out = entry;
    return true;
  }

  if (!ensure_mesh_cache_capacity())
    return false;

  entry = &mesh_cache[mesh_cache_count];
  memset(entry, 0, sizeof(*entry));

  entry->renderer = vulkan;
  entry->mesh = mesh;

  mesh_cache_count++;

  *entry_out = entry;

  return true;
}

static bool ensure_cached_mesh_main(VULKAN *vulkan, const Mesh *mesh, float r, float g, float b, float a, VULKAN_MeshCacheEntry **entry_out) {
  if (!ensure_cached_mesh_entry(vulkan, mesh, entry_out))
    return false;

  VULKAN_MeshCacheEntry *entry = *entry_out;

  if (entry->cached.main_valid) {
    if (entry->cached.color[0] == r && entry->cached.color[1] == g && entry->cached.color[2] == b && entry->cached.color[3] == a)
      return true;

    vkDeviceWaitIdle(vulkan->device);

    destroy_buffer(vulkan, &entry->cached.vertex);

    destroy_buffer(vulkan, &entry->cached.index);

    entry->cached.vertex_count = 0;
    entry->cached.index_count = 0;
    entry->cached.main_valid = false;
  }

  return upload_cached_mesh_main(vulkan, mesh, r, g, b, a, &entry->cached);
}

static bool ensure_cached_mesh_shadow(VULKAN *vulkan, const Mesh *mesh, VULKAN_MeshCacheEntry **entry_out) {
  if (!ensure_cached_mesh_entry(vulkan, mesh, entry_out))
    return false;

  VULKAN_MeshCacheEntry *entry = *entry_out;

  if (entry->cached.shadow_valid)
    return true;

  return upload_cached_mesh_shadow(vulkan, mesh, &entry->cached);
}

void VULKAN_RendererDrawCachedMesh(VULKAN *vulkan, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, HMM_Vec3 camera_position) {
  if (!vulkan || !mesh || !mvp)
    return;

  if (mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return;

  VULKAN_RendererBeginMainPass(vulkan);
  if (!vulkan->main_render_pass_begun)
    return;

  VULKAN_MeshCacheEntry *entry = NULL;

  if (!ensure_cached_mesh_main(vulkan, mesh, r, g, b, a, &entry))
    return;

  if (!entry || !entry->cached.main_valid || entry->cached.vertex.buffer == VK_NULL_HANDLE || entry->cached.index.buffer == VK_NULL_HANDLE)
    return;

  float rows[12];

  identity_rows(rows);

  VulkanMaterial material = create_cached_mesh_material();
  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkPipeline pipeline = select_3d_pipeline(vulkan, material.render_mode, material.depth_enabled, material.depth_write);
  VkPipelineLayout layout = select_3d_layout(vulkan, material.render_mode, material.depth_enabled, material.depth_write);

  vulkan->camera_position = camera_position;

  update_light_buffer_3d(vulkan);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vulkan->bound_pipeline = pipeline;
  }

  VkDeviceSize vertex_offset = 0;

  if (vulkan->bound_vertex_buffer != entry->cached.vertex.buffer || vulkan->bound_vertex_offset != vertex_offset) {
    vkCmdBindVertexBuffers(command, 0, 1, &entry->cached.vertex.buffer, &vertex_offset);
    vulkan->bound_vertex_buffer = entry->cached.vertex.buffer;
    vulkan->bound_vertex_offset = vertex_offset;
  }

  if (vulkan->bound_index_buffer != entry->cached.index.buffer) {
    vkCmdBindIndexBuffer(command, entry->cached.index.buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = entry->cached.index.buffer;
  }

  VULKAN_RendererBindMaterial(vulkan, layout, &material);
  push_lighting(vulkan, layout, mvp, rows, &material, 0);
  vkCmdDrawIndexed(command, entry->cached.index_count, 1, 0, 0, 0);
}

void VULKAN_RendererDrawCachedMeshMaterial(VULKAN *vulkan, const Mesh *mesh, const float *mvp, const float *model_rows, float r, float g, float b,
                                           float a, const VulkanMaterial *material, BLB_Texture *texture, HMM_Vec3 camera_position) {
  if (!vulkan || !mesh || !mvp || !material)
    return;
  if (mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return;

  VULKAN_RendererBeginMainPass(vulkan);
  if (!vulkan->main_render_pass_begun)
    return;

  VULKAN_MeshCacheEntry *entry = NULL;
  if (!ensure_cached_mesh_main(vulkan, mesh, r, g, b, a, &entry))
    return;
  if (!entry || !entry->cached.main_valid || entry->cached.vertex.buffer == VK_NULL_HANDLE || entry->cached.index.buffer == VK_NULL_HANDLE)
    return;

  float rows[12];
  if (model_rows)
    memcpy(rows, model_rows, sizeof(rows));
  else
    identity_rows(rows);
  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkPipeline pipeline = select_3d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_3d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  vulkan->camera_position = camera_position;
  update_light_buffer_3d(vulkan);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }

  VkDeviceSize vertex_offset = 0;
  if (vulkan->bound_vertex_buffer != entry->cached.vertex.buffer || vulkan->bound_vertex_offset != vertex_offset) {
    vkCmdBindVertexBuffers(command, 0, 1, &entry->cached.vertex.buffer, &vertex_offset);
    vulkan->bound_vertex_buffer = entry->cached.vertex.buffer;
    vulkan->bound_vertex_offset = vertex_offset;
  }

  if (vulkan->bound_index_buffer != entry->cached.index.buffer) {
    vkCmdBindIndexBuffer(command, entry->cached.index.buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = entry->cached.index.buffer;
  }

  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;
  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);
  push_lighting(vulkan, layout, mvp, rows, &bound_material, 0);
  vkCmdDrawIndexed(command, entry->cached.index_count, 1, 0, 0, 0);
}

void VULKAN_RendererDrawInstancedPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *mvp, const VulkanMaterial *material,
                                           BLB_Texture *texture, const VulkanInstanceData *instances, size_t instance_count,
                                           HMM_Vec3 camera_position) {
  if (!vulkan || !polygon || !mvp || !material || !instances || instance_count == 0 || instance_count > UINT32_MAX)
    return;
  if (!polygon->vertices || !polygon->indices || polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;
  if (vulkan->vertex_cursor + polygon->vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + polygon->index_count > VULKAN_MAX_INDICES)
    return;
  if (vulkan->instance_cursor + instance_count > vulkan->instance_capacity)
    return;
  for (size_t i = 0; i < polygon->index_count; ++i)
    if (polygon->indices[i] >= polygon->vertex_count)
      return;
  if (!ensure_polygon_normals((BLB_Polygon3D *)polygon))
    return;

  VULKAN_RendererBeginMainPass(vulkan);
  if (!vulkan->main_render_pass_begun)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;
  if (!vertices || !indices)
    return;

  size_t vertex_start = 0;
  size_t index_start = 0;
  bool cached_geometry = find_cached_geometry_3d(vulkan, polygon, &vertex_start, &index_start);
  if (!cached_geometry) {
    vertex_start = vulkan->vertex_cursor;
    index_start = vulkan->index_cursor;
    for (size_t i = 0; i < polygon->vertex_count; ++i) {
      HMM_Vec3 normal = safe_normalize(polygon->normals[i], HMM_V3(0.0f, 1.0f, 0.0f));
      HMM_Vec2 uv = polygon->uvs ? polygon->uvs[i] : HMM_V2(0.0f, 0.0f);
      upload_vertex(vertices, vertex_start + i, polygon->vertices[i], normal, 1.0f, 1.0f, 1.0f, 1.0f, uv);
    }
    for (size_t i = 0; i < polygon->index_count; ++i)
      indices[index_start + i] = polygon->indices[i];
    cache_geometry_3d(vulkan, polygon, vertex_start, index_start);
    vulkan->vertex_cursor += polygon->vertex_count;
    vulkan->index_cursor += polygon->index_count;
  }

  VulkanInstanceData *mapped = (VulkanInstanceData *)vulkan->instance_buffers[vulkan->current_frame].mapped;
  if (!mapped)
    return;
  uint32_t first_instance = (uint32_t)vulkan->instance_cursor;
  memcpy(mapped + vulkan->instance_cursor, instances, instance_count * sizeof(*instances));
  vulkan->instance_cursor += instance_count;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  uint32_t variant = depth_variant(material->depth_enabled, material->depth_write);
  VkPipeline pipeline = select_3d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_3d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  vulkan->camera_position = camera_position;
  update_light_buffer_3d(vulkan);
  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }
  VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize vertex_offset = vertex_start * sizeof(VulkanVertex);
  if (vulkan->bound_vertex_buffer != vertex_buffer || vulkan->bound_vertex_offset != vertex_offset) {
    vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &vertex_offset);
    vulkan->bound_vertex_buffer = vertex_buffer;
    vulkan->bound_vertex_offset = vertex_offset;
  }
  VkBuffer index_buffer = vulkan->index_buffers[vulkan->current_frame].buffer;
  if (vulkan->bound_index_buffer != index_buffer) {
    vkCmdBindIndexBuffer(command, index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = index_buffer;
  }
  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;
  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);
  float rows[12];
  identity_rows(rows);
  push_lighting_ex(vulkan, layout, mvp, rows, &bound_material, 0, true, first_instance);
  (void)variant;
  vkCmdDrawIndexed(command, (uint32_t)polygon->index_count, (uint32_t)instance_count, (uint32_t)index_start, 0, 0);
}

void VULKAN_RendererDrawInstancedMesh(VULKAN *vulkan, const Mesh *mesh, const float *mvp, const VulkanMaterial *material, BLB_Texture *texture,
                                      const VulkanInstanceData *instances, size_t instance_count, HMM_Vec3 camera_position) {
  if (!vulkan || !mesh || !mvp || !material || !instances || instance_count == 0 || instance_count > UINT32_MAX)
    return;

  if (mesh->vertex_count == 0 || mesh->index_count < 3 || mesh->index_count % 3 != 0)
    return;

  VULKAN_RendererBeginMainPass(vulkan);
  if (!vulkan->main_render_pass_begun)
    return;

  if (vulkan->instance_cursor + instance_count > vulkan->instance_capacity)
    return;

  VULKAN_MeshCacheEntry *entry = NULL;
  if (!ensure_cached_mesh_main(vulkan, mesh, 1.0f, 1.0f, 1.0f, 1.0f, &entry))
    return;
  if (!entry || !entry->cached.main_valid || entry->cached.vertex.buffer == VK_NULL_HANDLE || entry->cached.index.buffer == VK_NULL_HANDLE)
    return;

  VulkanInstanceData *mapped = (VulkanInstanceData *)vulkan->instance_buffers[vulkan->current_frame].mapped;
  if (!mapped)
    return;

  uint32_t first_instance = (uint32_t)vulkan->instance_cursor;
  memcpy(mapped + vulkan->instance_cursor, instances, instance_count * sizeof(*instances));
  vulkan->instance_cursor += instance_count;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkPipeline pipeline = select_3d_pipeline(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  VkPipelineLayout layout = select_3d_layout(vulkan, material->render_mode, material->depth_enabled, material->depth_write);
  vulkan->camera_position = camera_position;
  update_light_buffer_3d(vulkan);

  if (vulkan->bound_pipeline != pipeline) {
    vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vulkan->bound_pipeline = pipeline;
  }

  VkDeviceSize vertex_offset = 0;
  if (vulkan->bound_vertex_buffer != entry->cached.vertex.buffer || vulkan->bound_vertex_offset != vertex_offset) {
    vkCmdBindVertexBuffers(command, 0, 1, &entry->cached.vertex.buffer, &vertex_offset);
    vulkan->bound_vertex_buffer = entry->cached.vertex.buffer;
    vulkan->bound_vertex_offset = vertex_offset;
  }

  if (vulkan->bound_index_buffer != entry->cached.index.buffer) {
    vkCmdBindIndexBuffer(command, entry->cached.index.buffer, 0, VK_INDEX_TYPE_UINT32);
    vulkan->bound_index_buffer = entry->cached.index.buffer;
  }

  VulkanMaterial bound_material = *material;
  bound_material.base_texture_override = texture;
  VULKAN_RendererBindMaterial(vulkan, layout, &bound_material);

  float rows[12];
  identity_rows(rows);
  push_lighting_ex(vulkan, layout, mvp, rows, &bound_material, 0, true, first_instance);

  vkCmdDrawIndexed(command, entry->cached.index_count, (uint32_t)instance_count, 0, 0, 0);
}

void VULKAN_RendererDrawInstancedShadowMesh(VULKAN *vulkan, const Mesh *mesh, const float *mvp, const VulkanInstanceData *instances,
                                            size_t instance_count) {
  if (!vulkan || !mesh || !mvp || !instances || instance_count == 0 || instance_count > UINT32_MAX || !vulkan->shadow_pipeline)
    return;
  if (vulkan->instance_cursor + instance_count > vulkan->instance_capacity)
    return;

  VULKAN_MeshCacheEntry *entry = NULL;
  if (!ensure_cached_mesh_shadow(vulkan, mesh, &entry))
    return;
  if (!entry || !entry->cached.shadow_valid || entry->cached.shadow.buffer == VK_NULL_HANDLE)
    return;

  VulkanInstanceData *mapped = (VulkanInstanceData *)vulkan->instance_buffers[vulkan->current_frame].mapped;
  if (!mapped)
    return;

  uint32_t first_instance = (uint32_t)vulkan->instance_cursor;
  memcpy(mapped + vulkan->instance_cursor, instances, instance_count * sizeof(*instances));
  vulkan->instance_cursor += instance_count;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkDeviceSize offset = 0;
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &entry->cached.shadow.buffer, &offset);
  VkDescriptorSet descriptor_set = vulkan->light_descriptor_sets_3d[vulkan->current_frame];
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);

  VulkanShadowPushConstants push = {0};
  memcpy(push.mvp, mvp, sizeof(push.mvp));
  push.flags = 1u;
  push.padding[0] = first_instance;
  vkCmdPushConstants(command, vulkan->shadow_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
  vkCmdDraw(command, entry->cached.shadow_vertex_count, (uint32_t)instance_count, 0, 0);
}

void VULKAN_RendererDrawInstancedShadowPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *mvp, const VulkanInstanceData *instances,
                                                 size_t instance_count) {
  if (!vulkan || !polygon || !mvp || !instances || instance_count == 0 || instance_count > UINT32_MAX || !vulkan->shadow_pipeline)
    return;
  if (vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;
  if (!polygon->vertices || !polygon->indices || polygon->vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;
  if (vulkan->instance_cursor + instance_count > vulkan->instance_capacity)
    return;
  if (polygon->index_count > VULKAN_MAX_INDICES || vulkan->shadow_vertex_cursor + polygon->index_count > VULKAN_MAX_INDICES)
    return;
  for (size_t i = 0; i < polygon->index_count; ++i)
    if (polygon->indices[i] >= polygon->vertex_count)
      return;

  size_t vertex_start = 0;
  if (!find_cached_shadow_geometry(vulkan, polygon, &vertex_start)) {
    VulkanShadowVertex *vertices = (VulkanShadowVertex *)vulkan->shadow_vertex_buffers[vulkan->current_frame].mapped;
    if (!vertices)
      return;
    vertex_start = vulkan->shadow_vertex_cursor;
    for (size_t i = 0; i < polygon->index_count; ++i) {
      uint32_t index = polygon->indices[i];
      HMM_Vec3 p = polygon->vertices[index];
      vertices[vertex_start + i] = (VulkanShadowVertex){{p.x, p.y, p.z}};
    }
    vulkan->shadow_vertex_cursor += polygon->index_count;
    cache_shadow_geometry(vulkan, polygon, vertex_start);
  }

  VulkanInstanceData *mapped = (VulkanInstanceData *)vulkan->instance_buffers[vulkan->current_frame].mapped;
  if (!mapped)
    return;
  uint32_t first_instance = (uint32_t)vulkan->instance_cursor;
  memcpy(mapped + vulkan->instance_cursor, instances, instance_count * sizeof(*instances));
  vulkan->instance_cursor += instance_count;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkDeviceSize offset = vertex_start * sizeof(VulkanShadowVertex);
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &vulkan->shadow_vertex_buffers[vulkan->current_frame].buffer, &offset);
  VkDescriptorSet descriptor_set = vulkan->light_descriptor_sets_3d[vulkan->current_frame];
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
  VulkanShadowPushConstants push = {0};
  memcpy(push.mvp, mvp, sizeof(push.mvp));
  push.flags = 1u;
  push.padding[0] = first_instance;
  vkCmdPushConstants(command, vulkan->shadow_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
  vkCmdDraw(command, (uint32_t)polygon->index_count, (uint32_t)instance_count, (uint32_t)vertex_start, 0);
}

void VULKAN_RendererDrawCachedShadowMesh(VULKAN *vulkan, const Mesh *mesh, const float *model_mvp) {
  if (!vulkan || !mesh || !model_mvp)
    return;

  if (!vulkan->shadow_pipeline)
    return;

  VULKAN_MeshCacheEntry *entry = NULL;

  if (!ensure_cached_mesh_shadow(vulkan, mesh, &entry))
    return;

  if (!entry || !entry->cached.shadow_valid || entry->cached.shadow.buffer == VK_NULL_HANDLE)
    return;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  VkDeviceSize offset = 0;

  VulkanShadowPushConstants push = {0};

  memcpy(push.mvp, model_mvp, sizeof(push.mvp));

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline);

  vkCmdBindVertexBuffers(command, 0, 1, &entry->cached.shadow.buffer, &offset);
  VkDescriptorSet descriptor_set = vulkan->light_descriptor_sets_3d[vulkan->current_frame];
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);

  vkCmdPushConstants(command, vulkan->shadow_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

  vkCmdDraw(command, entry->cached.shadow_vertex_count, 1, 0, 0);
}

void VULKAN_RendererInvalidateCachedMesh(VULKAN *vulkan, const Mesh *mesh) {
  if (!vulkan || !mesh)
    return;

  for (size_t i = 0; i < mesh_cache_count; i++) {

    if (mesh_cache[i].renderer != vulkan || mesh_cache[i].mesh != mesh)
      continue;

    vkDeviceWaitIdle(vulkan->device);
    destroy_cached_mesh(vulkan, &mesh_cache[i].cached);

    if (i + 1 < mesh_cache_count) {
      memmove(&mesh_cache[i], &mesh_cache[i + 1], (mesh_cache_count - i - 1) * sizeof(*mesh_cache));
    }

    mesh_cache_count--;

    return;
  }
}

void VULKAN_RendererClearCachedMeshes(VULKAN *vulkan) {
  if (!vulkan)
    return;

  if (vulkan->device != VK_NULL_HANDLE)
    vkDeviceWaitIdle(vulkan->device);

  size_t write = 0;

  for (size_t i = 0; i < mesh_cache_count; i++) {

    if (mesh_cache[i].renderer != vulkan) {
      if (write != i)
        mesh_cache[write] = mesh_cache[i];

      write++;
      continue;
    }

    destroy_cached_mesh(vulkan, &mesh_cache[i].cached);
  }

  mesh_cache_count = write;
  if (mesh_cache_count == 0) {
    free(mesh_cache);
    mesh_cache = NULL;
    mesh_cache_capacity = 0;
  }
}
