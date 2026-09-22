#include "bulba/graphics/vulkan/renderer.h"

#include <stdlib.h>

int create_material_descriptor_resources(VULKAN *vulkan);
void destroy_material_descriptor_resources(VULKAN *vulkan);

static void destroy_pipeline_array(VULKAN *vulkan) {
  if (!vulkan || !vulkan->device)
    return;

  for (int mode = 0; mode < BLB_RENDER_MODE_COUNT; mode++) {
    for (int variant = 0; variant < VULKAN_DEPTH_VARIANTS; variant++) {
      if (vulkan->pipeline_3d[mode][variant]) {
        vkDestroyPipeline(vulkan->device, vulkan->pipeline_3d[mode][variant], NULL);
        vulkan->pipeline_3d[mode][variant] = VK_NULL_HANDLE;
      }

      if (vulkan->pipeline_layout_3d[mode][variant]) {
        vkDestroyPipelineLayout(vulkan->device, vulkan->pipeline_layout_3d[mode][variant], NULL);
        vulkan->pipeline_layout_3d[mode][variant] = VK_NULL_HANDLE;
      }

      if (vulkan->pipeline_2d[mode][variant]) {
        vkDestroyPipeline(vulkan->device, vulkan->pipeline_2d[mode][variant], NULL);
        vulkan->pipeline_2d[mode][variant] = VK_NULL_HANDLE;
      }

      if (vulkan->pipeline_layout_2d[mode][variant]) {
        vkDestroyPipelineLayout(vulkan->device, vulkan->pipeline_layout_2d[mode][variant], NULL);
        vulkan->pipeline_layout_2d[mode][variant] = VK_NULL_HANDLE;
      }
    }

    if (vulkan->text_pipeline_3d[mode]) {
      vkDestroyPipeline(vulkan->device, vulkan->text_pipeline_3d[mode], NULL);

      vulkan->text_pipeline_3d[mode] = VK_NULL_HANDLE;
    }

    if (vulkan->text_pipeline_layout_3d[mode]) {
      vkDestroyPipelineLayout(vulkan->device, vulkan->text_pipeline_layout_3d[mode], NULL);

      vulkan->text_pipeline_layout_3d[mode] = VK_NULL_HANDLE;
    }

    if (vulkan->text_pipeline_2d[mode]) {
      vkDestroyPipeline(vulkan->device, vulkan->text_pipeline_2d[mode], NULL);

      vulkan->text_pipeline_2d[mode] = VK_NULL_HANDLE;
    }

    if (vulkan->text_pipeline_layout_2d[mode]) {
      vkDestroyPipelineLayout(vulkan->device, vulkan->text_pipeline_layout_2d[mode], NULL);

      vulkan->text_pipeline_layout_2d[mode] = VK_NULL_HANDLE;
    }
  }

  if (vulkan->shadow_pipeline) {
    vkDestroyPipeline(vulkan->device, vulkan->shadow_pipeline, NULL);

    vulkan->shadow_pipeline = VK_NULL_HANDLE;
  }

  if (vulkan->shadow_pipeline_layout) {
    vkDestroyPipelineLayout(vulkan->device, vulkan->shadow_pipeline_layout, NULL);

    vulkan->shadow_pipeline_layout = VK_NULL_HANDLE;
  }
}

static void destroy_framebuffers_internal(VULKAN *vulkan) {
  if (!vulkan || !vulkan->device || !vulkan->framebuffers)
    return;

  for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
    if (vulkan->framebuffers[i]) {
      vkDestroyFramebuffer(vulkan->device, vulkan->framebuffers[i], NULL);

      vulkan->framebuffers[i] = VK_NULL_HANDLE;
    }
  }

  free(vulkan->framebuffers);
  vulkan->framebuffers = NULL;
}

static void destroy_sync_renderer(VULKAN *vulkan) {
  if (!vulkan || !vulkan->device)
    return;

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (vulkan->image_available[i]) {
      vkDestroySemaphore(vulkan->device, vulkan->image_available[i], NULL);

      vulkan->image_available[i] = VK_NULL_HANDLE;
    }

    if (vulkan->render_finished[i]) {
      vkDestroySemaphore(vulkan->device, vulkan->render_finished[i], NULL);

      vulkan->render_finished[i] = VK_NULL_HANDLE;
    }

    if (vulkan->in_flight[i]) {
      vkDestroyFence(vulkan->device, vulkan->in_flight[i], NULL);

      vulkan->in_flight[i] = VK_NULL_HANDLE;
    }
  }

  free(vulkan->images_in_flight);
  vulkan->images_in_flight = NULL;
}

int VULKAN_CreateRenderer(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE || vulkan->swapchain == VK_NULL_HANDLE)
    return -1;

  vulkan->vertex_cursor = 0;
  vulkan->index_cursor = 0;
  vulkan->text_vertex_cursor = 0;

  vulkan->main_render_pass_begun = false;
  vulkan->shadow_pass_begun = false;

  vulkan->shadow_enabled = false;
  vulkan->shadow_mode = 0;
  vulkan->shadow_bias = 0.002f;
  vulkan->light_buffer_dirty_3d = true;
  vulkan->light_buffer_dirty_2d = true;
  vulkan->frame_serial = 0;

  vulkan->clear_color[0] = 0.0f;
  vulkan->clear_color[1] = 0.0f;
  vulkan->clear_color[2] = 0.0f;
  vulkan->clear_color[3] = 1.0f;

  if (create_render_pass(vulkan) != 0)
    goto fail;

  if (create_depth_resources(vulkan) != 0)
    goto fail;

  if (create_framebuffers(vulkan) != 0)
    goto fail;

  if (create_command_resources(vulkan) != 0)
    goto fail;

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (create_buffer(vulkan, sizeof(VulkanVertex) * VULKAN_MAX_VERTICES, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vulkan->vertex_buffers[i]) != 0)
      goto fail;

    if (create_buffer(vulkan, sizeof(uint32_t) * VULKAN_MAX_INDICES, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &vulkan->index_buffers[i]) != 0)
      goto fail;
  }

  if (create_sync(vulkan) != 0)
    goto fail;

  if (create_light_descriptor_layout(vulkan, &vulkan->light_descriptor_set_layout_3d) != 0)
    goto fail;

  if (create_light_descriptor_layout(vulkan, &vulkan->light_descriptor_set_layout_2d) != 0)
    goto fail;

  if (create_light_buffers(vulkan, vulkan->light_buffers_3d) != 0)
    goto fail;

  if (create_light_buffers(vulkan, vulkan->light_buffers_2d) != 0)
    goto fail;

  if (create_shadow_resources(vulkan) != 0)
    goto fail;

  if (create_light_descriptors(vulkan, vulkan->light_descriptor_set_layout_3d, &vulkan->light_descriptor_pool_3d, vulkan->light_descriptor_sets_3d,
                               vulkan->light_buffers_3d) != 0)
    goto fail;

  if (create_light_descriptors(vulkan, vulkan->light_descriptor_set_layout_2d, &vulkan->light_descriptor_pool_2d, vulkan->light_descriptor_sets_2d,
                               vulkan->light_buffers_2d) != 0)
    goto fail;

  if (create_texture_descriptor_resources(vulkan) != 0)
    goto fail;

  if (create_material_descriptor_resources(vulkan) != 0)
    goto fail;

  if (create_text_descriptor_layout(vulkan) != 0)
    goto fail;

  if (create_text_buffers(vulkan) != 0)
    goto fail;

  if (create_pipelines(vulkan) != 0)
    goto fail;

  update_light_buffer_3d(vulkan);
  update_light_buffer_2d(vulkan);

  return 0;

fail:
  VULKAN_DestroyRenderer(vulkan);
  return -1;
}

void VULKAN_DestroyRenderer(VULKAN *vulkan) {
  if (!vulkan || !vulkan->device)
    return;

  vkDeviceWaitIdle(vulkan->device);

  VULKAN_RendererUnloadFont(vulkan);

  destroy_pipeline_array(vulkan);
  VULKAN_DestroyCustomPipelines(vulkan);

  destroy_shadow_resources(vulkan);

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    destroy_buffer(vulkan, &vulkan->text_vertex_buffers[i]);
  }

  if (vulkan->text_descriptor_set_layout) {
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->text_descriptor_set_layout, NULL);

    vulkan->text_descriptor_set_layout = VK_NULL_HANDLE;
  }

  destroy_material_descriptor_resources(vulkan);

  if (vulkan->default_texture) {
    BLB_Texture_Release(vulkan->default_texture);
    vulkan->default_texture = NULL;
  }

  destroy_texture_descriptor_resources(vulkan);

  destroy_light_descriptors(vulkan, &vulkan->light_descriptor_pool_3d, &vulkan->light_descriptor_set_layout_3d);

  destroy_light_descriptors(vulkan, &vulkan->light_descriptor_pool_2d, &vulkan->light_descriptor_set_layout_2d);

  destroy_light_buffers(vulkan, vulkan->light_buffers_3d);

  destroy_light_buffers(vulkan, vulkan->light_buffers_2d);

  vulkan->light3d_count = 0;
  vulkan->light2d_count = 0;

  destroy_sync_renderer(vulkan);

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    destroy_buffer(vulkan, &vulkan->vertex_buffers[i]);

    destroy_buffer(vulkan, &vulkan->index_buffers[i]);
  }

  if (vulkan->command_pool) {
    vkDestroyCommandPool(vulkan->device, vulkan->command_pool, NULL);

    vulkan->command_pool = VK_NULL_HANDLE;
  }

  destroy_framebuffers_internal(vulkan);

  destroy_depth_resources(vulkan);

  if (vulkan->render_pass) {
    vkDestroyRenderPass(vulkan->device, vulkan->render_pass, NULL);

    vulkan->render_pass = VK_NULL_HANDLE;
  }

  vulkan->main_render_pass_begun = false;
  vulkan->shadow_pass_begun = false;

  vulkan->shadow_enabled = false;
  vulkan->shadow_mode = 0;
  vulkan->shadow_bias = 0.0f;

  vulkan->vertex_cursor = 0;
  vulkan->index_cursor = 0;
  vulkan->text_vertex_cursor = 0;
}
