#include "bulba/graphics/vulkan/renderer.h"

#include <stddef.h>

int VULKAN_CreateRenderer(VULKAN *vulkan) {
  if (!vulkan)
    return -1;

  vulkan->clear_color[0] = 0.04f;
  vulkan->clear_color[1] = 0.04f;
  vulkan->clear_color[2] = 0.06f;
  vulkan->clear_color[3] = 1.0f;

  if (create_render_pass(vulkan) != 0)
    return -1;

  if (create_depth_resources(vulkan) != 0)
    return -1;

  if (create_framebuffers(vulkan) != 0)
    return -1;

  if (create_shadow_resources(vulkan) != 0)
    return -1;

  if (create_light_descriptor_layout(vulkan, &vulkan->light_descriptor_set_layout_3d) != 0)
    return -1;

  if (create_light_descriptor_layout(vulkan, &vulkan->light_descriptor_set_layout_2d) != 0)
    return -1;

  if (create_light_buffers(vulkan, vulkan->light_buffers_3d) != 0)
    return -1;

  if (create_light_buffers(vulkan, vulkan->light_buffers_2d) != 0)
    return -1;

  if (create_light_descriptors(vulkan, vulkan->light_descriptor_set_layout_3d, &vulkan->light_descriptor_pool_3d, vulkan->light_descriptor_sets_3d,
                               vulkan->light_buffers_3d) != 0)
    return -1;

  if (create_light_descriptors(vulkan, vulkan->light_descriptor_set_layout_2d, &vulkan->light_descriptor_pool_2d, vulkan->light_descriptor_sets_2d,
                               vulkan->light_buffers_2d) != 0)
    return -1;

  if (create_texture_descriptor_resources(vulkan) != 0)
    return -1;

  if (create_text_descriptor_layout(vulkan) != 0)
    return -1;

  if (create_command_resources(vulkan) != 0)
    return -1;

  if (create_sync(vulkan) != 0)
    return -1;

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (create_buffer(vulkan, sizeof(VulkanVertex) * VULKAN_MAX_VERTICES, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, &vulkan->vertex_buffers[i]) != 0)
      return -1;

    if (create_buffer(vulkan, sizeof(uint32_t) * VULKAN_MAX_INDICES, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &vulkan->index_buffers[i]) != 0)
      return -1;

    if (create_buffer(vulkan, sizeof(VulkanShadowVertex) * VULKAN_MAX_INDICES, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      &vulkan->shadow_vertex_buffers[i]) != 0)
      return -1;
  }

  if (create_text_buffers(vulkan) != 0)
    return -1;

  if (create_pipelines(vulkan) != 0)
    return -1;

  return 0;
}

void VULKAN_DestroyRenderer(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE)
    return;

  vkDeviceWaitIdle(vulkan->device);

  if (vulkan->default_texture) {
    BLB_Texture_Release(vulkan->default_texture);

    vulkan->default_texture = NULL;
  }

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    destroy_buffer(vulkan, &vulkan->vertex_buffers[i]);

    destroy_buffer(vulkan, &vulkan->index_buffers[i]);

    destroy_buffer(vulkan, &vulkan->text_vertex_buffers[i]);

    destroy_buffer(vulkan, &vulkan->shadow_vertex_buffers[i]);

    if (vulkan->image_available[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(vulkan->device, vulkan->image_available[i], NULL);

    if (vulkan->render_finished[i] != VK_NULL_HANDLE)
      vkDestroySemaphore(vulkan->device, vulkan->render_finished[i], NULL);

    if (vulkan->in_flight[i] != VK_NULL_HANDLE)
      vkDestroyFence(vulkan->device, vulkan->in_flight[i], NULL);
  }

  destroy_light_buffers(vulkan, vulkan->light_buffers_3d);

  destroy_light_buffers(vulkan, vulkan->light_buffers_2d);

  destroy_light_descriptors(vulkan, &vulkan->light_descriptor_pool_3d, &vulkan->light_descriptor_set_layout_3d);

  destroy_light_descriptors(vulkan, &vulkan->light_descriptor_pool_2d, &vulkan->light_descriptor_set_layout_2d);

  destroy_shadow_resources(vulkan);

  for (int mode = 0; mode < BLB_RENDER_MODE_COUNT; mode++) {
    if (vulkan->text_pipeline_2d[mode] != VK_NULL_HANDLE)
      vkDestroyPipeline(vulkan->device, vulkan->text_pipeline_2d[mode], NULL);

    if (vulkan->text_pipeline_3d[mode] != VK_NULL_HANDLE)
      vkDestroyPipeline(vulkan->device, vulkan->text_pipeline_3d[mode], NULL);

    if (vulkan->text_pipeline_layout_2d[mode] != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(vulkan->device, vulkan->text_pipeline_layout_2d[mode], NULL);

    if (vulkan->text_pipeline_layout_3d[mode] != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(vulkan->device, vulkan->text_pipeline_layout_3d[mode], NULL);

    if (vulkan->pipeline_2d[mode] != VK_NULL_HANDLE)
      vkDestroyPipeline(vulkan->device, vulkan->pipeline_2d[mode], NULL);

    if (vulkan->pipeline_3d[mode] != VK_NULL_HANDLE)
      vkDestroyPipeline(vulkan->device, vulkan->pipeline_3d[mode], NULL);

    if (vulkan->pipeline_layout_2d[mode] != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(vulkan->device, vulkan->pipeline_layout_2d[mode], NULL);

    if (vulkan->pipeline_layout_3d[mode] != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(vulkan->device, vulkan->pipeline_layout_3d[mode], NULL);
  }

  destroy_font_resources(vulkan);

  if (vulkan->text_descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->text_descriptor_set_layout, NULL);

    vulkan->text_descriptor_set_layout = VK_NULL_HANDLE;
  }

  destroy_texture_descriptor_resources(vulkan);

  free(vulkan->images_in_flight);

  vulkan->images_in_flight = NULL;

  if (vulkan->framebuffers) {
    for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
      if (vulkan->framebuffers[i] != VK_NULL_HANDLE)
        vkDestroyFramebuffer(vulkan->device, vulkan->framebuffers[i], NULL);
    }

    free(vulkan->framebuffers);

    vulkan->framebuffers = NULL;
  }

  if (vulkan->render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(vulkan->device, vulkan->render_pass, NULL);

    vulkan->render_pass = VK_NULL_HANDLE;
  }

  destroy_depth_resources(vulkan);

  if (vulkan->command_pool != VK_NULL_HANDLE) {
    vkDestroyCommandPool(vulkan->device, vulkan->command_pool, NULL);

    vulkan->command_pool = VK_NULL_HANDLE;
  }
}
