#include "bulba/graphics/vulkan/renderer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  VULKAN *vulkan;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkSampler sampler;
  VkDescriptorSet descriptor_set;
} VulkanTextureBackend;

static int begin_single_time_commands(VULKAN *vulkan, VkCommandBuffer *command) {
  VkCommandBufferAllocateInfo allocate_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                               .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                               .commandPool = vulkan->command_pool,
                                               .commandBufferCount = 1};

  if (vkAllocateCommandBuffers(vulkan->device, &allocate_info, command) != VK_SUCCESS)
    return -1;

  VkCommandBufferBeginInfo begin_info = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  if (vkBeginCommandBuffer(*command, &begin_info) != VK_SUCCESS) {
    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, command);

    return -1;
  }

  return 0;
}

static int end_single_time_commands(VULKAN *vulkan, VkCommandBuffer command) {
  if (vkEndCommandBuffer(command) != VK_SUCCESS) {
    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);

    return -1;
  }

  VkSubmitInfo submit_info = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &command};

  VkResult result = vkQueueSubmit(vulkan->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);

  if (result == VK_SUCCESS)
    result = vkQueueWaitIdle(vulkan->graphics_queue);

  vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);

  return result == VK_SUCCESS ? 0 : -1;
}

static int transition_image(VULKAN *vulkan, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
  VkCommandBuffer command = VK_NULL_HANDLE;

  if (begin_single_time_commands(vulkan, &command) != 0)
    return -1;

  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

  VkPipelineStageFlags source_stage;
  VkPipelineStageFlags destination_stage;

  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    vkEndCommandBuffer(command);

    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);

    return -1;
  }

  vkCmdPipelineBarrier(command, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

  return end_single_time_commands(vulkan, command);
}

static int copy_buffer_to_image(VULKAN *vulkan, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
  VkCommandBuffer command = VK_NULL_HANDLE;

  if (begin_single_time_commands(vulkan, &command) != 0)
    return -1;

  VkBufferImageCopy region = {.bufferOffset = 0,
                              .bufferRowLength = 0,
                              .bufferImageHeight = 0,
                              .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
                              .imageOffset = {0, 0, 0},
                              .imageExtent = {width, height, 1}};

  vkCmdCopyBufferToImage(command, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  return end_single_time_commands(vulkan, command);
}

static void destroy_texture_backend(VulkanTextureBackend *backend) {
  if (!backend)
    return;

  VULKAN *vulkan = backend->vulkan;

  if (!vulkan || vulkan->device == VK_NULL_HANDLE) {
    free(backend);
    return;
  }

  vkDeviceWaitIdle(vulkan->device);

  if (backend->descriptor_set != VK_NULL_HANDLE && vulkan->texture_descriptor_pool != VK_NULL_HANDLE) {
    vkFreeDescriptorSets(vulkan->device, vulkan->texture_descriptor_pool, 1, &backend->descriptor_set);
  }

  if (backend->sampler != VK_NULL_HANDLE) {
    vkDestroySampler(vulkan->device, backend->sampler, NULL);
  }

  if (backend->view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan->device, backend->view, NULL);
  }

  if (backend->image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan->device, backend->image, NULL);
  }

  if (backend->memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan->device, backend->memory, NULL);
  }

  free(backend);
}

void VULKAN_TextureDestroyBackend(void *backend_data) { destroy_texture_backend((VulkanTextureBackend *)backend_data); }

int create_texture_descriptor_resources(VULKAN *vulkan) {
  VkDescriptorSetLayoutBinding binding = {
      .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding};

  if (vkCreateDescriptorSetLayout(vulkan->device, &layout_info, NULL, &vulkan->texture_descriptor_set_layout) != VK_SUCCESS)
    return -1;

  VkDescriptorPoolSize pool_size = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = VULKAN_MAX_TEXTURES};

  VkDescriptorPoolCreateInfo pool_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                          .maxSets = VULKAN_MAX_TEXTURES,
                                          .poolSizeCount = 1,
                                          .pPoolSizes = &pool_size};

  if (vkCreateDescriptorPool(vulkan->device, &pool_info, NULL, &vulkan->texture_descriptor_pool) != VK_SUCCESS) {
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->texture_descriptor_set_layout, NULL);

    vulkan->texture_descriptor_set_layout = VK_NULL_HANDLE;

    return -1;
  }

  return 0;
}

void destroy_texture_descriptor_resources(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE)
    return;

  if (vulkan->texture_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan->device, vulkan->texture_descriptor_pool, NULL);

    vulkan->texture_descriptor_pool = VK_NULL_HANDLE;
  }

  if (vulkan->texture_descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->texture_descriptor_set_layout, NULL);

    vulkan->texture_descriptor_set_layout = VK_NULL_HANDLE;
  }
}

int VULKAN_TextureEnsureUploaded(VULKAN *vulkan, BLB_Texture *texture) {
  if (!vulkan || !texture || !texture->pixels)
    return -1;

  if (texture->type != BLB_TEXTURE_2D)
    return -1;

  if (texture->width == 0 || texture->height == 0)
    return -1;

  if (texture->pixel_size != (size_t)texture->width * (size_t)texture->height * 4)
    return -1;

  if (texture->backend_data)
    return 0;

  VulkanTextureBackend *backend = calloc(1, sizeof(*backend));

  if (!backend)
    return -1;

  backend->vulkan = vulkan;

  VkDeviceSize image_size = (VkDeviceSize)texture->pixel_size;

  VULKAN_Buffer staging = {0};

  if (create_buffer(vulkan, image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging) != 0) {
    free(backend);
    return -1;
  }

  memcpy(staging.mapped, texture->pixels, texture->pixel_size);

  if (create_image(vulkan, texture->width, texture->height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   &backend->image, &backend->memory) != 0) {
    destroy_buffer(vulkan, &staging);

    free(backend);
    return -1;
  }

  if (transition_image(vulkan, backend->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) != 0) {
    destroy_buffer(vulkan, &staging);

    destroy_texture_backend(backend);

    return -1;
  }

  if (copy_buffer_to_image(vulkan, staging.buffer, backend->image, texture->width, texture->height) != 0) {
    destroy_buffer(vulkan, &staging);

    destroy_texture_backend(backend);

    return -1;
  }

  destroy_buffer(vulkan, &staging);

  if (transition_image(vulkan, backend->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) != 0) {
    destroy_texture_backend(backend);

    return -1;
  }

  VkImageViewCreateInfo view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = backend->image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_SRGB,
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

  if (vkCreateImageView(vulkan->device, &view_info, NULL, &backend->view) != VK_SUCCESS) {
    destroy_texture_backend(backend);

    return -1;
  }

  VkSamplerCreateInfo sampler_info = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                      .magFilter = VK_FILTER_LINEAR,
                                      .minFilter = VK_FILTER_LINEAR,
                                      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                      .mipLodBias = 0.0f,
                                      .anisotropyEnable = VK_FALSE,
                                      .maxAnisotropy = 1.0f,
                                      .compareEnable = VK_FALSE,
                                      .compareOp = VK_COMPARE_OP_ALWAYS,
                                      .minLod = 0.0f,
                                      .maxLod = 0.0f,
                                      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                                      .unnormalizedCoordinates = VK_FALSE};

  if (vkCreateSampler(vulkan->device, &sampler_info, NULL, &backend->sampler) != VK_SUCCESS) {
    destroy_texture_backend(backend);

    return -1;
  }

  VkDescriptorSetAllocateInfo allocate_info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                               .descriptorPool = vulkan->texture_descriptor_pool,
                                               .descriptorSetCount = 1,
                                               .pSetLayouts = &vulkan->texture_descriptor_set_layout};

  if (vkAllocateDescriptorSets(vulkan->device, &allocate_info, &backend->descriptor_set) != VK_SUCCESS) {
    destroy_texture_backend(backend);

    return -1;
  }

  VkDescriptorImageInfo image_info = {
      .sampler = backend->sampler, .imageView = backend->view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = backend->descriptor_set,
                                .dstBinding = 0,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                .pImageInfo = &image_info};

  vkUpdateDescriptorSets(vulkan->device, 1, &write, 0, NULL);

  texture->backend_data = backend;

  texture->backend_destroy = VULKAN_TextureDestroyBackend;

  return 0;
}

static BLB_Texture *create_default_texture(void) {
  const unsigned char pixel[4] = {255, 255, 255, 255};

  return BLB_Texture_Create2D(1, 1, pixel, sizeof(pixel));
}

void VULKAN_RendererBindTexture(VULKAN *vulkan, VkPipelineLayout layout, BLB_Texture *texture) {
  if (!vulkan)
    return;

  if (!texture) {
    if (!vulkan->default_texture)
      vulkan->default_texture = create_default_texture();

    texture = vulkan->default_texture;
  }

  if (!texture)
    return;

  if (VULKAN_TextureEnsureUploaded(vulkan, texture) != 0)
    return;

  VulkanTextureBackend *backend = (VulkanTextureBackend *)texture->backend_data;

  if (!backend || backend->descriptor_set == VK_NULL_HANDLE)
    return;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];

  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &backend->descriptor_set, 0, NULL);
}
