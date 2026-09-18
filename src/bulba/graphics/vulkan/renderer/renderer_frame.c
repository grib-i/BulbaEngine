#include "bulba/graphics/vulkan/renderer.h"
#include "bulba/graphics/vulkan/swapchain.h"

#include <stdlib.h>
#include <string.h>

int create_depth_resources(VULKAN *vulkan) {
  vulkan->depth_images = calloc(vulkan->swapchain_image_count, sizeof(VkImage));
  vulkan->depth_memories = calloc(vulkan->swapchain_image_count, sizeof(VkDeviceMemory));
  vulkan->depth_image_views = calloc(vulkan->swapchain_image_count, sizeof(VkImageView));

  if (!vulkan->depth_images || !vulkan->depth_memories || !vulkan->depth_image_views) {
    free(vulkan->depth_images);
    free(vulkan->depth_memories);
    free(vulkan->depth_image_views);
    vulkan->depth_images = NULL;
    vulkan->depth_memories = NULL;
    vulkan->depth_image_views = NULL;
    return -1;
  }

  for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
    VkImageCreateInfo image_info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                    .imageType = VK_IMAGE_TYPE_2D,
                                    .format = VK_FORMAT_D32_SFLOAT,
                                    .extent = {vulkan->swapchain_extent.width, vulkan->swapchain_extent.height, 1},
                                    .mipLevels = 1,
                                    .arrayLayers = 1,
                                    .samples = VK_SAMPLE_COUNT_1_BIT,
                                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                                    .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

    if (vkCreateImage(vulkan->device, &image_info, NULL, &vulkan->depth_images[i]) != VK_SUCCESS)
      return -1;

    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(vulkan->device, vulkan->depth_images[i], &requirements);

    uint32_t memory_type = find_memory_type(vulkan, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == UINT32_MAX)
      return -1;

    VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = requirements.size, .memoryTypeIndex = memory_type};

    if (vkAllocateMemory(vulkan->device, &alloc, NULL, &vulkan->depth_memories[i]) != VK_SUCCESS)
      return -1;

    if (vkBindImageMemory(vulkan->device, vulkan->depth_images[i], vulkan->depth_memories[i], 0) != VK_SUCCESS)
      return -1;

    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vulkan->depth_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

    if (vkCreateImageView(vulkan->device, &view, NULL, &vulkan->depth_image_views[i]) != VK_SUCCESS)
      return -1;
  }

  return 0;
}

void destroy_depth_resources(VULKAN *vulkan) {
  if (!vulkan || !vulkan->depth_images || !vulkan->depth_memories || !vulkan->depth_image_views)
    return;

  for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
    if (vulkan->depth_image_views[i] != VK_NULL_HANDLE)
      vkDestroyImageView(vulkan->device, vulkan->depth_image_views[i], NULL);
    if (vulkan->depth_images[i] != VK_NULL_HANDLE)
      vkDestroyImage(vulkan->device, vulkan->depth_images[i], NULL);
    if (vulkan->depth_memories[i] != VK_NULL_HANDLE)
      vkFreeMemory(vulkan->device, vulkan->depth_memories[i], NULL);
  }

  free(vulkan->depth_image_views);
  free(vulkan->depth_images);
  free(vulkan->depth_memories);
  vulkan->depth_image_views = NULL;
  vulkan->depth_images = NULL;
  vulkan->depth_memories = NULL;
}

int create_framebuffers(VULKAN *vulkan) {
  vulkan->framebuffers = calloc(vulkan->swapchain_image_count, sizeof(VkFramebuffer));
  if (!vulkan->framebuffers)
    return -1;

  for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
    VkImageView attachments[] = {vulkan->swapchain_image_views[i], vulkan->depth_image_views[i]};

    VkFramebufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                    .renderPass = vulkan->render_pass,
                                    .attachmentCount = 2,
                                    .pAttachments = attachments,
                                    .width = vulkan->swapchain_extent.width,
                                    .height = vulkan->swapchain_extent.height,
                                    .layers = 1};

    if (vkCreateFramebuffer(vulkan->device, &info, NULL, &vulkan->framebuffers[i]) != VK_SUCCESS)
      return -1;
  }

  return 0;
}

int create_command_resources(VULKAN *vulkan) {
  VkCommandPoolCreateInfo pool = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                  .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                  .queueFamilyIndex = vulkan->graphics_queue_family};

  if (vkCreateCommandPool(vulkan->device, &pool, NULL, &vulkan->command_pool) != VK_SUCCESS)
    return -1;

  VkCommandBufferAllocateInfo alloc = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                       .commandPool = vulkan->command_pool,
                                       .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                       .commandBufferCount = VULKAN_MAX_FRAMES_IN_FLIGHT};

  if (vkAllocateCommandBuffers(vulkan->device, &alloc, vulkan->command_buffers) != VK_SUCCESS)
    return -1;

  return 0;
}

int create_sync(VULKAN *vulkan) {
  VkSemaphoreCreateInfo semaphore = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkFenceCreateInfo fence = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(vulkan->device, &semaphore, NULL, &vulkan->image_available[i]) != VK_SUCCESS)
      return -1;
    if (vkCreateSemaphore(vulkan->device, &semaphore, NULL, &vulkan->render_finished[i]) != VK_SUCCESS)
      return -1;
    if (vkCreateFence(vulkan->device, &fence, NULL, &vulkan->in_flight[i]) != VK_SUCCESS)
      return -1;
  }

  vulkan->images_in_flight = calloc(vulkan->swapchain_image_count, sizeof(VkFence));
  return vulkan->images_in_flight ? 0 : -1;
}

int VULKAN_RendererBeginFrame(VULKAN *vulkan) {
  uint32_t frame = vulkan->current_frame;

  VkResult result = vkWaitForFences(vulkan->device, 1, &vulkan->in_flight[frame], VK_TRUE, UINT64_MAX);
  if (result != VK_SUCCESS)
    return -1;

  result =
      vkAcquireNextImageKHR(vulkan->device, vulkan->swapchain, UINT64_MAX, vulkan->image_available[frame], VK_NULL_HANDLE, &vulkan->current_image);

  if (result == VK_ERROR_OUT_OF_DATE_KHR)
    return -2;

  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    return -1;

  if (vulkan->images_in_flight[vulkan->current_image] != VK_NULL_HANDLE) {
    result = vkWaitForFences(vulkan->device, 1, &vulkan->images_in_flight[vulkan->current_image], VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
      return -1;
  }

  vulkan->images_in_flight[vulkan->current_image] = vulkan->in_flight[frame];

  if (vkResetFences(vulkan->device, 1, &vulkan->in_flight[frame]) != VK_SUCCESS)
    return -1;

  update_light_buffer_3d(vulkan);
  update_light_buffer_2d(vulkan);

  if (vkResetCommandBuffer(vulkan->command_buffers[frame], 0) != VK_SUCCESS)
    return -1;

  VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

  if (vkBeginCommandBuffer(vulkan->command_buffers[frame], &begin) != VK_SUCCESS)
    return -1;

  vulkan->vertex_cursor = 0;
  vulkan->index_cursor = 0;
  vulkan->text_vertex_cursor = 0;
  vulkan->main_render_pass_begun = false;

  return 0;
}

void VULKAN_RendererBeginMainPass(VULKAN *vulkan) {
  if (!vulkan || vulkan->main_render_pass_begun)
    return;

  VkClearValue clear_values[2] = {{.color = {{vulkan->clear_color[0], vulkan->clear_color[1], vulkan->clear_color[2], vulkan->clear_color[3]}}},
                                  {.depthStencil = {1.0f, 0}}};

  VkRenderPassBeginInfo render = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                  .renderPass = vulkan->render_pass,
                                  .framebuffer = vulkan->framebuffers[vulkan->current_image],
                                  .renderArea = {.offset = {0, 0}, .extent = vulkan->swapchain_extent},
                                  .clearValueCount = 2,
                                  .pClearValues = clear_values};

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {.x = 0.0f,
                         .y = 0.0f,
                         .width = (float)vulkan->swapchain_extent.width,
                         .height = (float)vulkan->swapchain_extent.height,
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f};

  VkRect2D scissor = {.offset = {0, 0}, .extent = vulkan->swapchain_extent};
  vkCmdSetViewport(command, 0, 1, &viewport);
  vkCmdSetScissor(command, 0, 1, &scissor);

  vulkan->vertex_cursor = 0;
  vulkan->index_cursor = 0;
  vulkan->text_vertex_cursor = 0;
  vulkan->main_render_pass_begun = true;
}

void VULKAN_RendererSetClearColor(VULKAN *vulkan, float r, float g, float b, float a) {
  if (!vulkan)
    return;

  vulkan->clear_color[0] = r;
  vulkan->clear_color[1] = g;
  vulkan->clear_color[2] = b;
  vulkan->clear_color[3] = a;
}

void VULKAN_RendererSetShadow(VULKAN *vulkan, const float *shadow_mvp, bool enabled, float bias) {
  if (!vulkan || !shadow_mvp)
    return;

  memcpy(&vulkan->shadow_mvp.Elements[0][0], shadow_mvp, sizeof(vulkan->shadow_mvp.Elements));
  vulkan->shadow_enabled = enabled;
  vulkan->shadow_bias = bias;
  update_light_buffer_3d(vulkan);
}

void VULKAN_RendererBeginShadowPass(VULKAN *vulkan) {
  if (!vulkan || !vulkan->shadow_enabled)
    return;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkClearValue clear = {.depthStencil = {1.0f, 0}};
  VkRenderPassBeginInfo render = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                  .renderPass = vulkan->shadow_render_pass,
                                  .framebuffer = vulkan->shadow_framebuffers[vulkan->current_frame],
                                  .renderArea = {.offset = {0, 0}, .extent = {VULKAN_SHADOW_MAP_SIZE, VULKAN_SHADOW_MAP_SIZE}},
                                  .clearValueCount = 1,
                                  .pClearValues = &clear};

  vkCmdBeginRenderPass(command, &render, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport = {
      .x = 0.0f, .y = 0.0f, .width = (float)VULKAN_SHADOW_MAP_SIZE, .height = (float)VULKAN_SHADOW_MAP_SIZE, .minDepth = 0.0f, .maxDepth = 1.0f};
  VkRect2D scissor = {.offset = {0, 0}, .extent = {VULKAN_SHADOW_MAP_SIZE, VULKAN_SHADOW_MAP_SIZE}};
  vkCmdSetViewport(command, 0, 1, &viewport);
  vkCmdSetScissor(command, 0, 1, &scissor);
}

void VULKAN_RendererEndShadowPass(VULKAN *vulkan) {
  if (!vulkan || !vulkan->shadow_enabled)
    return;
  vkCmdEndRenderPass(vulkan->command_buffers[vulkan->current_frame]);
}

int VULKAN_RendererEndFrame(VULKAN *vulkan) {
  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  vkCmdEndRenderPass(command);

  if (vkEndCommandBuffer(command) != VK_SUCCESS)
    return -1;

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                         .waitSemaphoreCount = 1,
                         .pWaitSemaphores = &vulkan->image_available[vulkan->current_frame],
                         .pWaitDstStageMask = &wait_stage,
                         .commandBufferCount = 1,
                         .pCommandBuffers = &command,
                         .signalSemaphoreCount = 1,
                         .pSignalSemaphores = &vulkan->render_finished[vulkan->current_frame]};

  if (vkQueueSubmit(vulkan->graphics_queue, 1, &submit, vulkan->in_flight[vulkan->current_frame]) != VK_SUCCESS)
    return -1;

  VkPresentInfoKHR present = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = &vulkan->render_finished[vulkan->current_frame],
                              .swapchainCount = 1,
                              .pSwapchains = &vulkan->swapchain,
                              .pImageIndices = &vulkan->current_image};

  VkResult result = vkQueuePresentKHR(vulkan->present_queue, &present);
  vulkan->current_frame = (vulkan->current_frame + 1) % VULKAN_MAX_FRAMES_IN_FLIGHT;

  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    return -2;

  return result == VK_SUCCESS ? 0 : -1;
}

void VULKAN_RendererClear(VULKAN *vulkan, float r, float g, float b, float a) {
  if (!vulkan)
    return;

  VkClearAttachment attachments[2] = {{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .colorAttachment = 0, .clearValue = {.color = {{r, g, b, a}}}},
                                      {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .clearValue = {.depthStencil = {1.0f, 0}}}};

  VkClearRect rect = {.rect = {.offset = {0, 0}, .extent = vulkan->swapchain_extent}, .baseArrayLayer = 0, .layerCount = 1};

  vkCmdClearAttachments(vulkan->command_buffers[vulkan->current_frame], 2, attachments, 1, &rect);
}

int VULKAN_RendererRecreateSwapchain(VULKAN *vulkan) {
  if (!vulkan)
    return -1;

  int width;
  int height;
  glfwGetFramebufferSize(vulkan->window, &width, &height);

  if (width == 0 || height == 0)
    return 1;

  if (vkDeviceWaitIdle(vulkan->device) != VK_SUCCESS)
    return -1;

  if (vulkan->framebuffers) {
    for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
      if (vulkan->framebuffers[i] != VK_NULL_HANDLE)
        vkDestroyFramebuffer(vulkan->device, vulkan->framebuffers[i], NULL);
    }
    free(vulkan->framebuffers);
    vulkan->framebuffers = NULL;
  }

  destroy_depth_resources(vulkan);
  VULKAN_DestroySwapchain(vulkan);

  if (VULKAN_CreateSwapchain(vulkan, false) != 0)
    return -1;
  if (create_depth_resources(vulkan) != 0)
    return -1;
  if (create_framebuffers(vulkan) != 0)
    return -1;

  free(vulkan->images_in_flight);
  vulkan->images_in_flight = calloc(vulkan->swapchain_image_count, sizeof(VkFence));
  return vulkan->images_in_flight ? 0 : -1;
}
