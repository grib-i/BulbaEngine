#include "bulba/graphics/vulkan/renderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static HMM_Vec3 calculate_normal(HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c) {
  HMM_Vec3 normal = HMM_Cross(HMM_SubV3(b, a), HMM_SubV3(c, a));

  float length = HMM_LenV3(normal);

  if (length <= 0.000001f)
    return HMM_V3(0.0f, 1.0f, 0.0f);

  return HMM_MulV3F(normal, 1.0f / length);
}

static HMM_Vec3 safe_normalize(HMM_Vec3 value, HMM_Vec3 fallback) {
  float length = HMM_LenV3(value);

  if (length <= 0.000001f)
    return fallback;

  return HMM_MulV3F(value, 1.0f / length);
}

static void upload_vertex(VulkanVertex *vertices, size_t index, HMM_Vec3 position, HMM_Vec3 normal, float r, float g, float b, float a, HMM_Vec2 uv) {
  vertices[index] = (VulkanVertex){
      .position = {position.x, position.y, position.z},
      .color = {r, g, b, a},
      .normal = {normal.x, normal.y, normal.z},
      .uv = {uv.x, uv.y},
  };
}

static VkPipeline select_3d_pipeline(VULKAN *vulkan, BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_3d[mode];
}

static VkPipelineLayout select_3d_layout(VULKAN *vulkan, BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_layout_3d[mode];
}

static VkPipeline select_2d_pipeline(VULKAN *vulkan, BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_2d[mode];
}

static VkPipelineLayout select_2d_layout(VULKAN *vulkan, BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;

  return vulkan->pipeline_layout_2d[mode];
}

static void identity_rows(float rows[12]) {
  const float value[12] = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
  };

  memcpy(rows, value, sizeof(value));
}

void VULKAN_RendererDrawTriangle(VULKAN *vulkan, HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color,
                                 HMM_Vec3 camera_position) {
  if (!vulkan || !mvp)
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  if (vulkan->vertex_cursor + 3 > VULKAN_MAX_VERTICES)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;

  size_t start = vulkan->vertex_cursor;

  HMM_Vec3 normal = calculate_normal(a, b, c);

  upload_vertex(vertices, start, a, normal, r, g, b_color, a_color, HMM_V2(0.0f, 0.0f));
  upload_vertex(vertices, start + 1, b, normal, r, g, b_color, a_color, HMM_V2(1.0f, 0.0f));
  upload_vertex(vertices, start + 2, c, normal, r, g, b_color, a_color, HMM_V2(0.5f, 1.0f));

  float rows[12];
  float normal_rows[12];

  identity_rows(rows);
  identity_rows(normal_rows);

  VulkanMaterial material = {
      .lighting_enabled = true,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .render_mode = BLB_RENDER_TRANSPARENT,
  };

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkBuffer buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize offset = start * sizeof(VulkanVertex);

  vulkan->camera_position = camera_position;

  VkPipeline pipeline = select_3d_pipeline(vulkan, BLB_RENDER_OPAQUE);
  VkPipelineLayout layout = select_3d_layout(vulkan, BLB_RENDER_OPAQUE);

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &buffer, &offset);

  VULKAN_RendererBindTexture(vulkan, layout, NULL);

  push_lighting(vulkan, layout, mvp, rows, normal_rows, &material, 0);

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

  unsigned int indices[3] = {0, 1, 2};

  size_t count = 3;

  BLB_Polygon2D polygon = {
      .vertices = vertices,
      .base_vertices = vertices,
      .uvs = uvs,
      .vertex_count = count,
      .indices = indices,
      .index_count = 3,
  };

  VulkanMaterial material = {
      .lighting_enabled = lighting_enabled != 0,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .render_mode = BLB_RENDER_TRANSPARENT,
  };

  (void)mvp;
  (void)camera_position;

  VULKAN_RendererDrawPolygon2D(vulkan, &polygon, (float)vulkan->swapchain_extent.width, (float)vulkan->swapchain_extent.height, r, g, b_color,
                               a_color, &material, NULL);
}

void VULKAN_RendererDrawMesh(VULKAN *vulkan, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, HMM_Vec3 camera_position) {
  if (!vulkan || !mesh || !mesh->vertices || !mesh->indices || !mvp)
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  if (mesh->vertex_count == 0 || mesh->index_count == 0)
    return;

  if (vulkan->vertex_cursor + mesh->vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + mesh->index_count > VULKAN_MAX_INDICES)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;

  size_t vertex_start = vulkan->vertex_cursor;
  size_t index_start = vulkan->index_cursor;

  for (size_t i = 0; i < mesh->vertex_count; i++) {
    HMM_Vec3 normal = mesh->normals ? safe_normalize(mesh->normals[i], HMM_V3(0.0f, 1.0f, 0.0f)) : HMM_V3(0.0f, 1.0f, 0.0f);

    HMM_Vec2 uv = mesh->uvs ? mesh->uvs[i] : HMM_V2(0.0f, 0.0f);

    upload_vertex(vertices, vertex_start + i, mesh->vertices[i], normal, r, g, b, a, uv);
  }

  for (size_t i = 0; i < mesh->index_count; i++) {
    if (mesh->indices[i] >= mesh->vertex_count)
      return;

    indices[index_start + i] = (uint32_t)mesh->indices[i];
  }

  float rows[12];
  float normal_rows[12];

  identity_rows(rows);
  identity_rows(normal_rows);

  VulkanMaterial material = {
      .lighting_enabled = true,
      .emission = 0.0f,
      .glow = 0.0f,
      .roundness = 0.0f,
      .glow_radius = 0.0f,
      .glow_falloff = 0.0f,
      .render_mode = BLB_RENDER_TRANSPARENT,
  };

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);

  vulkan->camera_position = camera_position;

  VkPipeline pipeline = select_3d_pipeline(vulkan, BLB_RENDER_OPAQUE);
  VkPipelineLayout layout = select_3d_layout(vulkan, BLB_RENDER_OPAQUE);

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);

  vkCmdBindIndexBuffer(command, vulkan->index_buffers[vulkan->current_frame].buffer, 0, VK_INDEX_TYPE_UINT32);

  VULKAN_RendererBindTexture(vulkan, layout, NULL);

  push_lighting(vulkan, layout, mvp, rows, normal_rows, &material, 0);

  vkCmdDrawIndexed(command, mesh->index_count, 1, (uint32_t)index_start, 0, 0);

  vulkan->vertex_cursor += mesh->vertex_count;
  vulkan->index_cursor += mesh->index_count;
}

void VULKAN_RendererDrawPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, const float *normal_rows,
                                  float r, float g, float b, float a, const VulkanMaterial *material, BLB_Texture *texture) {
  if (!vulkan || !polygon || !mvp || !model_rows || !normal_rows || !material || !polygon->vertices || !polygon->indices)
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  size_t vertex_count = polygon->vertex_count;

  if (vertex_count == 0 || polygon->index_count < 3 || polygon->index_count % 3 != 0)
    return;

  if (vulkan->vertex_cursor + vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + polygon->index_count > VULKAN_MAX_INDICES)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;

  size_t vertex_start = vulkan->vertex_cursor;
  size_t index_start = vulkan->index_cursor;

  HMM_Vec3 *normals = calloc(vertex_count, sizeof(HMM_Vec3));

  if (!normals)
    return;

  for (size_t i = 0; i < polygon->index_count; i += 3) {
    uint32_t ia = polygon->indices[i];
    uint32_t ib = polygon->indices[i + 1];
    uint32_t ic = polygon->indices[i + 2];

    if (ia >= vertex_count || ib >= vertex_count || ic >= vertex_count) {
      free(normals);
      return;
    }

    HMM_Vec3 normal = calculate_normal(polygon->vertices[ia], polygon->vertices[ib], polygon->vertices[ic]);

    normals[ia] = HMM_AddV3(normals[ia], normal);
    normals[ib] = HMM_AddV3(normals[ib], normal);
    normals[ic] = HMM_AddV3(normals[ic], normal);
  }

  for (size_t i = 0; i < vertex_count; i++) {
    HMM_Vec3 normal = safe_normalize(normals[i], HMM_V3(0.0f, 1.0f, 0.0f));

    HMM_Vec2 uv = polygon->uvs ? polygon->uvs[i] : HMM_V2(0.0f, 0.0f);

    upload_vertex(vertices, vertex_start + i, polygon->vertices[i], normal, r, g, b, a, uv);
  }

  free(normals);

  for (size_t i = 0; i < polygon->index_count; i++) {
    if (polygon->indices[i] >= vertex_count)
      return;

    indices[index_start + i] = (uint32_t)polygon->indices[i];
  }

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);

  VkPipeline pipeline = select_3d_pipeline(vulkan, material->render_mode);
  VkPipelineLayout layout = select_3d_layout(vulkan, material->render_mode);

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);

  vkCmdBindIndexBuffer(command, vulkan->index_buffers[vulkan->current_frame].buffer, 0, VK_INDEX_TYPE_UINT32);

  VULKAN_RendererBindTexture(vulkan, layout, texture);

  push_lighting(vulkan, layout, mvp, model_rows, normal_rows, material, 0);

  vkCmdDrawIndexed(command, polygon->index_count, 1, (uint32_t)index_start, 0, 0);

  vulkan->vertex_cursor += vertex_count;
  vulkan->index_cursor += polygon->index_count;
}

void VULKAN_RendererDrawPolygon2D(VULKAN *vulkan, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height, float r, float g,
                                  float b, float a, const VulkanMaterial *material, BLB_Texture *texture) {
  if (!vulkan || !polygon || !material || !polygon->vertices || !polygon->indices)
    return;

  VULKAN_RendererBeginMainPass(vulkan);

  size_t vertex_count = polygon->vertex_count;

  if (vertex_count == 0 || polygon->index_count < 3)
    return;

  if (vulkan->vertex_cursor + vertex_count > VULKAN_MAX_VERTICES || vulkan->index_cursor + polygon->index_count > VULKAN_MAX_INDICES)
    return;

  VulkanVertex *vertices = (VulkanVertex *)vulkan->vertex_buffers[vulkan->current_frame].mapped;
  uint32_t *indices = (uint32_t *)vulkan->index_buffers[vulkan->current_frame].mapped;

  size_t vertex_start = vulkan->vertex_cursor;
  size_t index_start = vulkan->index_cursor;

  for (size_t i = 0; i < vertex_count; i++) {
    HMM_Vec2 uv = polygon->uvs ? polygon->uvs[i] : HMM_V2(0.0f, 0.0f);

    vertices[vertex_start + i] = (VulkanVertex){
        .position =
            {
                polygon->vertices[i].x,
                polygon->vertices[i].y,
                0.0f,
            },
        .color = {r, g, b, a},
        .normal = {0.0f, 0.0f, 1.0f},
        .uv = {uv.x, uv.y},
    };
  }

  for (size_t i = 0; i < polygon->index_count; i++) {
    if (polygon->indices[i] >= vertex_count)
      return;

    indices[index_start + i] = (uint32_t)polygon->indices[i];
  }

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkBuffer vertex_buffer = vulkan->vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize offset = vertex_start * sizeof(VulkanVertex);

  VkPipeline pipeline = select_2d_pipeline(vulkan, material->render_mode);
  VkPipelineLayout layout = select_2d_layout(vulkan, material->render_mode);

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &vertex_buffer, &offset);

  vkCmdBindIndexBuffer(command, vulkan->index_buffers[vulkan->current_frame].buffer, 0, VK_INDEX_TYPE_UINT32);

  VULKAN_RendererBindTexture(vulkan, layout, texture);

  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &vulkan->light_descriptor_sets_2d[vulkan->current_frame], 0, NULL);

  Vulkan2DPushConstants push = {
      .viewport = {viewport_width, viewport_height, 0.0f, 0.0f},
      .material =
          {
              material->lighting_enabled ? 1.0f : 0.0f,
              fmaxf(material->emission, 0.0f),
              fmaxf(material->glow, 0.0f),
              fmaxf(material->roundness, 0.0f),
          },
  };

  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

  vkCmdDrawIndexed(command, polygon->index_count, 1, (uint32_t)index_start, 0, 0);

  vulkan->vertex_cursor += vertex_count;
  vulkan->index_cursor += polygon->index_count;
}

void VULKAN_RendererDrawShadowPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *model_mvp) {
  if (!vulkan || !polygon || !model_mvp || !polygon->vertices || !polygon->indices || polygon->index_count < 3)
    return;

  size_t vertex_count = polygon->vertex_count;

  if (vertex_count == 0)
    return;

  if (!vulkan->shadow_pipeline || vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;

  if (polygon->index_count > VULKAN_MAX_INDICES)
    return;

  VulkanShadowVertex *vertices = (VulkanShadowVertex *)vulkan->shadow_vertex_buffers[vulkan->current_frame].mapped;

  for (size_t i = 0; i < polygon->index_count; i++) {
    uint32_t index = polygon->indices[i];

    if (index >= vertex_count)
      return;

    HMM_Vec3 p = polygon->vertices[index];

    vertices[i] = (VulkanShadowVertex){
        {p.x, p.y, p.z},
    };
  }

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  VkDeviceSize offset = 0;

  VulkanShadowPushConstants push = {0};

  memcpy(push.mvp, model_mvp, sizeof(push.mvp));

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->shadow_pipeline);

  vkCmdBindVertexBuffers(command, 0, 1, &vulkan->shadow_vertex_buffers[vulkan->current_frame].buffer, &offset);

  vkCmdPushConstants(command, vulkan->shadow_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

  vkCmdDraw(command, (uint32_t)polygon->index_count, 1, 0, 0);
}

void push_lighting(VULKAN *vulkan, VkPipelineLayout layout, const float *mvp, const float *model_rows, const float *normal_rows,
                   const VulkanMaterial *material, int is_2d) {
  if (!vulkan || !mvp || !model_rows || !normal_rows || !material)
    return;

  VulkanLightingPushConstants data = {0};

  memcpy(data.mvp, mvp, sizeof(data.mvp));
  memcpy(data.model_rows, model_rows, sizeof(data.model_rows));
  memcpy(data.normal_rows, normal_rows, sizeof(data.normal_rows));

  data.material[0] = material->lighting_enabled ? 1.0f : 0.0f;
  data.material[1] = fmaxf(material->emission, 0.0f);
  data.material[2] = fmaxf(material->glow, 0.0f);
  data.material[3] = fmaxf(material->roundness, 0.0f);

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  VkDescriptorSet descriptor_set =
      is_2d ? vulkan->light_descriptor_sets_2d[vulkan->current_frame] : vulkan->light_descriptor_sets_3d[vulkan->current_frame];

  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptor_set, 0, NULL);

  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data), &data);
}
