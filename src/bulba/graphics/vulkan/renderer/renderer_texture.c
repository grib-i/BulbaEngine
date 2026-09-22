#include "bulba/graphics/vulkan/renderer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
  VULKAN *vulkan;
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkSampler sampler;
  VkDescriptorSet descriptor_set;
} VulkanTextureBackend;

typedef struct {
  VULKAN *vulkan;
  VkDescriptorSet descriptor_set;
  VkDescriptorSetLayout layout;
  uint32_t slot_index;
  uint64_t revision;
  uint64_t texture_revision;
  BLB_Texture *base_override;
} VulkanMaterialBackend;

static uint16_t float_to_half(float value) {
  union { float f; uint32_t u; } bits = {value};
  uint32_t sign = (bits.u >> 16) & 0x8000u;
  uint32_t mantissa = bits.u & 0x007fffffu;
  int exponent = (int)((bits.u >> 23) & 0xffu) - 127 + 15;

  if (exponent <= 0) {
    if (exponent < -10)
      return (uint16_t)sign;
    mantissa |= 0x00800000u;
    uint32_t shifted = mantissa >> (1 - exponent);
    if (shifted & 0x00001000u)
      shifted += 0x00002000u;
    return (uint16_t)(sign | (shifted >> 13));
  }

  if (exponent >= 31) {
    if (mantissa == 0)
      return (uint16_t)(sign | 0x7c00u);
    return (uint16_t)(sign | 0x7c00u | (mantissa >> 13));
  }

  if (mantissa & 0x00001000u)
    mantissa += 0x00002000u;
  if (mantissa & 0x00800000u) {
    mantissa = 0;
    exponent++;
    if (exponent >= 31)
      return (uint16_t)(sign | 0x7c00u);
  }

  return (uint16_t)(sign | ((uint32_t)exponent << 10) | (mantissa >> 13));
}

static uint32_t pack_half2(float x, float y) {
  return (uint32_t)float_to_half(x) | ((uint32_t)float_to_half(y) << 16);
}

static uint32_t pack_rotation_uv_set(float rotation, int uv_set) {
  uint32_t uv = (uint32_t)(uv_set < 0 ? 0 : uv_set);
  if (uv > 65535u)
    uv = 65535u;
  return (uint32_t)float_to_half(rotation) | (uv << 16u);
}

static uint32_t material_sampler_index(BLB_TextureWrap wrap_u, BLB_TextureWrap wrap_v, BLB_TextureFilter min_filter, BLB_TextureFilter mag_filter) {
  uint32_t wu = (uint32_t)wrap_u % 3u;
  uint32_t wv = (uint32_t)wrap_v % 3u;
  uint32_t mi = (uint32_t)min_filter % 2u;
  uint32_t ma = (uint32_t)mag_filter % 2u;
  return (((wu * 3u + wv) * 2u + mi) * 2u + ma);
}

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

int create_material_descriptor_resources(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE)
    return -1;

  VkDescriptorSetLayoutBinding bindings[2] = {
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = VULKAN_MATERIAL_TEXTURE_SLOTS,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };

  VkDescriptorSetLayoutCreateInfo layout_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = bindings,
  };

  if (vkCreateDescriptorSetLayout(vulkan->device, &layout_info, NULL, &vulkan->material_descriptor_set_layout) != VK_SUCCESS)
    return -1;

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(vulkan->physical_device, &properties);
  VkDeviceSize alignment = properties.limits.minStorageBufferOffsetAlignment;
  if (alignment == 0)
    alignment = 1;

  VkDeviceSize record_size = sizeof(VulkanMaterialGPURecord);
  vulkan->material_params_stride = (record_size + alignment - 1) / alignment * alignment;

  VkDeviceSize buffer_size = vulkan->material_params_stride * VULKAN_MAX_MATERIALS;
  if (create_buffer(vulkan, buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &vulkan->material_params_buffer) != 0) {
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->material_descriptor_set_layout, NULL);
    vulkan->material_descriptor_set_layout = VK_NULL_HANDLE;
    return -1;
  }

  memset(vulkan->material_params_buffer.mapped, 0, (size_t)buffer_size);
  VulkanMaterialGPURecord *default_record = (VulkanMaterialGPURecord *)vulkan->material_params_buffer.mapped;
  for (uint32_t i = 0; i < VULKAN_MATERIAL_TEXTURE_SLOTS; ++i) {
    default_record->slots[i].packed[0] = pack_half2(0.0f, 0.0f);
    default_record->slots[i].packed[1] = pack_half2(1.0f, 1.0f);
    default_record->slots[i].packed[2] = pack_rotation_uv_set(0.0f, 0);
  }

  for (uint32_t i = 0; i < VULKAN_MAX_MATERIAL_SAMPLERS; ++i) {
    uint32_t code = i;
    uint32_t mag = code % 2u; code /= 2u;
    uint32_t min = code % 2u; code /= 2u;
    uint32_t wrap_v = code % 3u; code /= 3u;
    uint32_t wrap_u = code % 3u;

    VkSamplerCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = mag ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .minFilter = min ? VK_FILTER_LINEAR : VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .addressModeU = wrap_u == BLB_TEXTURE_WRAP_CLAMP_TO_EDGE ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                        : wrap_u == BLB_TEXTURE_WRAP_MIRRORED_REPEAT ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                        : VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = wrap_v == BLB_TEXTURE_WRAP_CLAMP_TO_EDGE ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                        : wrap_v == BLB_TEXTURE_WRAP_MIRRORED_REPEAT ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                        : VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    if (vkCreateSampler(vulkan->device, &info, NULL, &vulkan->material_samplers[i]) != VK_SUCCESS) {
      for (uint32_t j = 0; j < i; ++j)
        vkDestroySampler(vulkan->device, vulkan->material_samplers[j], NULL);
      vulkan->material_params_stride = 0;
      destroy_buffer(vulkan, &vulkan->material_params_buffer);
      vkDestroyDescriptorSetLayout(vulkan->device, vulkan->material_descriptor_set_layout, NULL);
      vulkan->material_descriptor_set_layout = VK_NULL_HANDLE;
      return -1;
    }
  }

  VkDescriptorPoolSize pool_sizes[2] = {
      {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = (VULKAN_MAX_MATERIALS + 1u) * VULKAN_MATERIAL_TEXTURE_SLOTS},
      {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .descriptorCount = VULKAN_MAX_MATERIALS + 1u},
  };

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = VULKAN_MAX_MATERIALS + 1u,
      .poolSizeCount = 2,
      .pPoolSizes = pool_sizes,
  };

  if (vkCreateDescriptorPool(vulkan->device, &pool_info, NULL, &vulkan->material_descriptor_pool) != VK_SUCCESS) {
    for (uint32_t i = 0; i < VULKAN_MAX_MATERIAL_SAMPLERS; ++i)
      if (vulkan->material_samplers[i] != VK_NULL_HANDLE)
        vkDestroySampler(vulkan->device, vulkan->material_samplers[i], NULL);
    destroy_buffer(vulkan, &vulkan->material_params_buffer);
    vulkan->material_params_stride = 0;
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->material_descriptor_set_layout, NULL);
    vulkan->material_descriptor_set_layout = VK_NULL_HANDLE;
    return -1;
  }

  memset(vulkan->material_slot_used, 0, sizeof(vulkan->material_slot_used));
  vulkan->material_slot_used[0] = true;
  vulkan->default_material_descriptor_set = VK_NULL_HANDLE;

  return 0;
}

void destroy_material_descriptor_resources(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE)
    return;

  if (vulkan->material_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan->device, vulkan->material_descriptor_pool, NULL);
    vulkan->material_descriptor_pool = VK_NULL_HANDLE;
  }

  for (uint32_t i = 0; i < VULKAN_MAX_MATERIAL_SAMPLERS; ++i) {
    if (vulkan->material_samplers[i] != VK_NULL_HANDLE) {
      vkDestroySampler(vulkan->device, vulkan->material_samplers[i], NULL);
      vulkan->material_samplers[i] = VK_NULL_HANDLE;
    }
  }

  destroy_buffer(vulkan, &vulkan->material_params_buffer);
  vulkan->material_params_stride = 0;
  vulkan->default_material_descriptor_set = VK_NULL_HANDLE;
  memset(vulkan->material_slot_used, 0, sizeof(vulkan->material_slot_used));

  if (vulkan->material_descriptor_set_layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan->device, vulkan->material_descriptor_set_layout, NULL);
    vulkan->material_descriptor_set_layout = VK_NULL_HANDLE;
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
                                      .addressModeU = texture->clamp_to_edge ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                      .addressModeV = texture->clamp_to_edge ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                      .addressModeW = texture->clamp_to_edge ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT,
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


static void destroy_material_backend(void *backend_data) {
  VulkanMaterialBackend *backend = (VulkanMaterialBackend *)backend_data;
  if (!backend)
    return;

  VULKAN *vulkan = backend->vulkan;
  if (vulkan) {
    if (backend->slot_index < VULKAN_MAX_MATERIALS && backend->slot_index != 0)
      vulkan->material_slot_used[backend->slot_index] = false;

    if (vulkan->device != VK_NULL_HANDLE && vulkan->material_descriptor_pool != VK_NULL_HANDLE &&
        backend->descriptor_set != VK_NULL_HANDLE && backend->layout == vulkan->material_descriptor_set_layout) {
      vkFreeDescriptorSets(vulkan->device, vulkan->material_descriptor_pool, 1, &backend->descriptor_set);
    }
  }

  free(backend);
}

static BLB_MaterialTexture *material_slot(BLB_Material *material, uint32_t slot) {
  if (!material || slot >= VULKAN_MATERIAL_TEXTURE_SLOTS)
    return NULL;

  BLB_MaterialTexture *slots[] = {
      &material->base_color_texture, &material->metallic_roughness_texture, &material->normal_texture,
      &material->occlusion_texture, &material->emission_texture, &material->specular_texture,
      &material->specular_color_texture, &material->clearcoat_texture, &material->clearcoat_roughness_texture,
      &material->clearcoat_normal_texture, &material->transmission_texture, &material->thickness_texture,
      &material->sheen_color_texture, &material->sheen_roughness_texture, &material->iridescence_texture,
      &material->iridescence_thickness_texture, &material->anisotropy_texture,
  };

  return slots[slot];
}

static const BLB_MaterialTexture *material_slot_const(const BLB_Material *material, uint32_t slot) {
  return material_slot((BLB_Material *)material, slot);
}

static BLB_Texture *material_slot_texture(const BLB_Material *material, uint32_t slot, BLB_Texture *base_override) {
  const BLB_MaterialTexture *slot_data = material_slot_const(material, slot);
  if (slot_data && slot_data->texture)
    return slot_data->texture;
  return slot == 0u ? base_override : NULL;
}

static uint32_t find_material_slot(VULKAN *vulkan) {
  for (uint32_t i = 1; i < VULKAN_MAX_MATERIALS; ++i) {
    if (!vulkan->material_slot_used[i]) {
      vulkan->material_slot_used[i] = true;
      return i;
    }
  }
  return UINT32_MAX;
}

static void write_material_transforms(VULKAN *vulkan, uint32_t slot_index, const BLB_Material *material) {
  if (!vulkan || !material || !vulkan->material_params_buffer.mapped || slot_index >= VULKAN_MAX_MATERIALS)
    return;

  unsigned char *base = (unsigned char *)vulkan->material_params_buffer.mapped + vulkan->material_params_stride * slot_index;
  VulkanMaterialGPURecord *record = (VulkanMaterialGPURecord *)base;

  for (uint32_t i = 0; i < VULKAN_MATERIAL_TEXTURE_SLOTS; ++i) {
    const BLB_MaterialTexture *texture = material_slot_const(material, i);
    if (!texture)
      continue;

    record->slots[i].packed[0] = pack_half2(texture->offset[0], texture->offset[1]);
    record->slots[i].packed[1] = pack_half2(texture->scale[0], texture->scale[1]);
    record->slots[i].packed[2] = pack_rotation_uv_set(texture->rotation, texture->uv_set);
  }
}

static int ensure_default_material_set(VULKAN *vulkan, BLB_Texture *default_texture, VkDescriptorSet *out_set) {
  if (vulkan->default_material_descriptor_set != VK_NULL_HANDLE) {
    *out_set = vulkan->default_material_descriptor_set;
    return 0;
  }

  VkDescriptorSetAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = vulkan->material_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &vulkan->material_descriptor_set_layout,
  };
  if (vkAllocateDescriptorSets(vulkan->device, &allocate_info, &vulkan->default_material_descriptor_set) != VK_SUCCESS)
    return -1;

  VulkanTextureBackend *tb = (VulkanTextureBackend *)default_texture->backend_data;
  VkDescriptorImageInfo infos[VULKAN_MATERIAL_TEXTURE_SLOTS];
  for (uint32_t i = 0; i < VULKAN_MATERIAL_TEXTURE_SLOTS; ++i) {
    infos[i] = (VkDescriptorImageInfo){.sampler = vulkan->material_samplers[material_sampler_index(BLB_TEXTURE_WRAP_REPEAT, BLB_TEXTURE_WRAP_REPEAT, BLB_TEXTURE_FILTER_LINEAR, BLB_TEXTURE_FILTER_LINEAR)],
                                      .imageView = tb->view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  }

  VkDescriptorBufferInfo params = {.buffer = vulkan->material_params_buffer.buffer, .offset = 0, .range = sizeof(VulkanMaterialGPURecord)};
  VkWriteDescriptorSet writes[2] = {
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vulkan->default_material_descriptor_set, .dstBinding = 0,
       .descriptorCount = VULKAN_MATERIAL_TEXTURE_SLOTS, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = infos},
      {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vulkan->default_material_descriptor_set, .dstBinding = 1,
       .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .pBufferInfo = &params},
  };
  vkUpdateDescriptorSets(vulkan->device, 2, writes, 0, NULL);
  *out_set = vulkan->default_material_descriptor_set;
  return 0;
}

static int ensure_material_descriptor_set(VULKAN *vulkan, const VulkanMaterial *material, VkDescriptorSet *out_set, uint32_t *out_dynamic_offset) {
  if (!vulkan || !material || !out_set || !out_dynamic_offset || vulkan->device == VK_NULL_HANDLE ||
      vulkan->material_descriptor_set_layout == VK_NULL_HANDLE || vulkan->material_descriptor_pool == VK_NULL_HANDLE)
    return -1;

  BLB_Texture *default_texture = vulkan->default_texture;
  if (!default_texture) {
    default_texture = create_default_texture();
    if (!default_texture)
      return -1;
    vulkan->default_texture = default_texture;
  }
  if (VULKAN_TextureEnsureUploaded(vulkan, default_texture) != 0)
    return -1;

  const BLB_Material *source = material->source_material;
  if (!source) {
    if (ensure_default_material_set(vulkan, default_texture, out_set) != 0)
      return -1;
    *out_dynamic_offset = 0;
    return 0;
  }

  VulkanMaterialBackend *backend = (VulkanMaterialBackend *)source->backend_data;
  bool stale_layout = backend && backend->layout != vulkan->material_descriptor_set_layout;
  if (stale_layout) {
    backend->descriptor_set = VK_NULL_HANDLE;
    backend->layout = vulkan->material_descriptor_set_layout;
    backend->vulkan = vulkan;
    backend->revision = 0;
    backend->texture_revision = 0;
    backend->base_override = NULL;
    if (backend->slot_index < VULKAN_MAX_MATERIALS)
      vulkan->material_slot_used[backend->slot_index] = false;
    backend->slot_index = UINT32_MAX;
  }

  if (!backend) {
    backend = calloc(1, sizeof(*backend));
    if (!backend)
      return -1;
    backend->vulkan = vulkan;
    backend->layout = vulkan->material_descriptor_set_layout;
    backend->slot_index = find_material_slot(vulkan);
    if (backend->slot_index == UINT32_MAX) {
      free(backend);
      return -1;
    }
    ((BLB_Material *)source)->backend_data = backend;
    ((BLB_Material *)source)->backend_destroy = destroy_material_backend;
  }

  if (backend->slot_index == UINT32_MAX) {
    backend->slot_index = find_material_slot(vulkan);
    if (backend->slot_index == UINT32_MAX)
      return -1;
  }

  if (backend->descriptor_set == VK_NULL_HANDLE) {
    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vulkan->material_descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &vulkan->material_descriptor_set_layout,
    };
    if (vkAllocateDescriptorSets(vulkan->device, &allocate_info, &backend->descriptor_set) != VK_SUCCESS)
      return -1;
  }

  BLB_Texture *base_override = material->base_texture_override;
  uint64_t texture_revision = BLB_Material_TextureStateRevision();
  if (backend->revision != source->revision || backend->texture_revision != texture_revision || backend->base_override != base_override) {
    VkDescriptorImageInfo infos[VULKAN_MATERIAL_TEXTURE_SLOTS];
    for (uint32_t i = 0; i < VULKAN_MATERIAL_TEXTURE_SLOTS; ++i) {
      BLB_Texture *texture = material_slot_texture(source, i, base_override);
      if (!texture)
        texture = default_texture;
      if (VULKAN_TextureEnsureUploaded(vulkan, texture) != 0)
        return -1;
      VulkanTextureBackend *tb = (VulkanTextureBackend *)texture->backend_data;
      if (!tb || tb->view == VK_NULL_HANDLE)
        return -1;

      const BLB_MaterialTexture *slot = material_slot_const(source, i);
      BLB_TextureWrap wrap_u = slot ? slot->wrap_u : BLB_TEXTURE_WRAP_REPEAT;
      BLB_TextureWrap wrap_v = slot ? slot->wrap_v : BLB_TEXTURE_WRAP_REPEAT;
      BLB_TextureFilter min_filter = slot ? slot->min_filter : BLB_TEXTURE_FILTER_LINEAR;
      BLB_TextureFilter mag_filter = slot ? slot->mag_filter : BLB_TEXTURE_FILTER_LINEAR;
      if (texture->clamp_to_edge) {
        wrap_u = BLB_TEXTURE_WRAP_CLAMP_TO_EDGE;
        wrap_v = BLB_TEXTURE_WRAP_CLAMP_TO_EDGE;
      }
      uint32_t sampler_index = material_sampler_index(wrap_u, wrap_v, min_filter, mag_filter);
      infos[i] = (VkDescriptorImageInfo){.sampler = vulkan->material_samplers[sampler_index], .imageView = tb->view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    }

    VkDescriptorBufferInfo params = {.buffer = vulkan->material_params_buffer.buffer, .offset = 0, .range = sizeof(VulkanMaterialGPURecord)};
    VkWriteDescriptorSet writes[2] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = backend->descriptor_set, .dstBinding = 0,
         .descriptorCount = VULKAN_MATERIAL_TEXTURE_SLOTS, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = infos},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = backend->descriptor_set, .dstBinding = 1,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, .pBufferInfo = &params},
    };
    vkUpdateDescriptorSets(vulkan->device, 2, writes, 0, NULL);
    write_material_transforms(vulkan, backend->slot_index, source);
    backend->revision = source->revision;
    backend->texture_revision = texture_revision;
    backend->base_override = base_override;
  }

  *out_set = backend->descriptor_set;
  *out_dynamic_offset = (uint32_t)(vulkan->material_params_stride * backend->slot_index);
  return 0;
}

void VULKAN_RendererBindMaterial(VULKAN *vulkan, VkPipelineLayout layout, const VulkanMaterial *material) {
  if (!vulkan || !layout || !material || vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;

  VkDescriptorSet set = VK_NULL_HANDLE;
  uint32_t dynamic_offset = 0;
  if (ensure_material_descriptor_set(vulkan, material, &set, &dynamic_offset) != 0 || set == VK_NULL_HANDLE)
    return;

  if (vulkan->bound_material_set == set && vulkan->bound_material_layout == layout && vulkan->bound_material_offset == dynamic_offset)
    return;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &set, 1, &dynamic_offset);
  vulkan->bound_material_set = set;
  vulkan->bound_material_layout = layout;
  vulkan->bound_material_offset = dynamic_offset;
}

void VULKAN_RendererInvalidateMaterialCache(VULKAN *vulkan, BLB_Material *material) {
  (void)vulkan;
  (void)material;
  /* Material revisioning is checked lazily on the next draw. */
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

