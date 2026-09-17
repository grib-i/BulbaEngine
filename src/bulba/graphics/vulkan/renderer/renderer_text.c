#include "bulba/graphics/vulkan/renderer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t utf8_decode(const char **text) {
  const unsigned char *s = (const unsigned char *)*text;
  uint32_t codepoint;

  if (s[0] < 0x80) {
    codepoint = s[0];
    *text += 1;
  } else if ((s[0] & 0xE0) == 0xC0) {
    codepoint = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    *text += 2;
  } else if ((s[0] & 0xF0) == 0xE0) {
    codepoint = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    *text += 3;
  } else if ((s[0] & 0xF8) == 0xF0) {
    codepoint = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    *text += 4;
  } else {
    codepoint = '?';
    *text += 1;
  }

  return codepoint;
}

int create_text_buffers(VULKAN *vulkan) {
  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (create_buffer(vulkan, sizeof(VulkanTextVertex) * VULKAN_MAX_TEXT_VERTICES, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      &vulkan->text_vertex_buffers[i]) != 0)
      return -1;
  }

  return 0;
}

static int create_font_texture(VULKAN *vulkan, const Font *font) {
  if (!vulkan || !font || !font->atlas || !font->atlas_width || !font->atlas_height)
    return -1;

  VkDeviceSize size = (VkDeviceSize)font->atlas_width * (VkDeviceSize)font->atlas_height;
  VULKAN_Buffer staging = {0};

  if (create_buffer(vulkan, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging) != 0)
    return -1;

  memcpy(staging.mapped, font->atlas, (size_t)size);

  if (create_image(vulkan, font->atlas_width, font->atlas_height, VK_FORMAT_R8_UNORM,
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                   &vulkan->font_image, &vulkan->font_image_memory) != 0) {
    destroy_buffer(vulkan, &staging);
    return -1;
  }

  VkCommandBufferAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = vulkan->command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1
  };

  VkCommandBuffer command = VK_NULL_HANDLE;

  if (vkAllocateCommandBuffers(vulkan->device, &alloc, &command) != VK_SUCCESS) {
    destroy_buffer(vulkan, &staging);
    return -1;
  }

  VkCommandBufferBeginInfo begin = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  if (vkBeginCommandBuffer(command, &begin) != VK_SUCCESS) {
    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);
    destroy_buffer(vulkan, &staging);
    return -1;
  }

  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = vulkan->font_image,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1
      }
  };

  vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

  VkBufferImageCopy copy = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1
      },
      .imageOffset = {0, 0, 0},
      .imageExtent = {font->atlas_width, font->atlas_height, 1}
  };

  vkCmdCopyBufferToImage(command, staging.buffer, vulkan->font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

  if (vkEndCommandBuffer(command) != VK_SUCCESS) {
    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);
    destroy_buffer(vulkan, &staging);
    return -1;
  }

  VkSubmitInfo submit = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command
  };

  if (vkQueueSubmit(vulkan->graphics_queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);
    destroy_buffer(vulkan, &staging);
    return -1;
  }

  if (vkQueueWaitIdle(vulkan->graphics_queue) != VK_SUCCESS) {
    vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);
    destroy_buffer(vulkan, &staging);
    return -1;
  }

  vkFreeCommandBuffers(vulkan->device, vulkan->command_pool, 1, &command);
  destroy_buffer(vulkan, &staging);

  VkImageViewCreateInfo view = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vulkan->font_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8_UNORM,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1
      }
  };

  if (vkCreateImageView(vulkan->device, &view, NULL, &vulkan->font_image_view) != VK_SUCCESS)
    return -1;

  VkSamplerCreateInfo sampler = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0f,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = 0.0f,
      .mipLodBias = 0.0f
  };

  if (vkCreateSampler(vulkan->device, &sampler, NULL, &vulkan->font_sampler) != VK_SUCCESS) {
    vkDestroyImageView(vulkan->device, vulkan->font_image_view, NULL);
    vulkan->font_image_view = VK_NULL_HANDLE;
    return -1;
  }

  return 0;
}

static int create_text_descriptors(VULKAN *vulkan) {
  VkDescriptorPoolSize pool_size = {
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = 1
  };

  VkDescriptorPoolCreateInfo pool = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = 1,
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size
  };

  if (vkCreateDescriptorPool(vulkan->device, &pool, NULL, &vulkan->text_descriptor_pool) != VK_SUCCESS)
    return -1;

  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = vulkan->text_descriptor_pool,
      .descriptorSetCount = 1,
      .pSetLayouts = &vulkan->text_descriptor_set_layout
  };

  if (vkAllocateDescriptorSets(vulkan->device, &alloc, &vulkan->text_descriptor_set) != VK_SUCCESS)
    return -1;

  VkDescriptorImageInfo image = {
      .sampler = vulkan->font_sampler,
      .imageView = vulkan->font_image_view,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  };

  VkWriteDescriptorSet write = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = vulkan->text_descriptor_set,
      .dstBinding = 0,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = &image
  };

  vkUpdateDescriptorSets(vulkan->device, 1, &write, 0, NULL);
  return 0;
}

void destroy_font_resources(VULKAN *vulkan) {
  if (!vulkan)
    return;

  if (vulkan->text_descriptor_pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan->device, vulkan->text_descriptor_pool, NULL);
    vulkan->text_descriptor_pool = VK_NULL_HANDLE;
    vulkan->text_descriptor_set = VK_NULL_HANDLE;
  }

  if (vulkan->font_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(vulkan->device, vulkan->font_sampler, NULL);
    vulkan->font_sampler = VK_NULL_HANDLE;
  }

  if (vulkan->font_image_view != VK_NULL_HANDLE) {
    vkDestroyImageView(vulkan->device, vulkan->font_image_view, NULL);
    vulkan->font_image_view = VK_NULL_HANDLE;
  }

  if (vulkan->font_image != VK_NULL_HANDLE) {
    vkDestroyImage(vulkan->device, vulkan->font_image, NULL);
    vulkan->font_image = VK_NULL_HANDLE;
  }

  if (vulkan->font_image_memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan->device, vulkan->font_image_memory, NULL);
    vulkan->font_image_memory = VK_NULL_HANDLE;
  }

  vulkan->loaded_font = NULL;
}

int VULKAN_RendererLoadFont(VULKAN *vulkan, const Font *font) {
  if (!vulkan || !font || !font->atlas || !font->atlas_width || !font->atlas_height)
    return -1;

  if (vulkan->loaded_font == font && vulkan->text_descriptor_set != VK_NULL_HANDLE)
    return 0;

  if (vkDeviceWaitIdle(vulkan->device) != VK_SUCCESS)
    return -1;

  VULKAN_RendererUnloadFont(vulkan);

  if (create_font_texture(vulkan, font) != 0) {
    VULKAN_RendererUnloadFont(vulkan);
    return -1;
  }

  if (create_text_descriptors(vulkan) != 0) {
    VULKAN_RendererUnloadFont(vulkan);
    return -1;
  }

  vulkan->loaded_font = font;
  return 0;
}

void VULKAN_RendererUnloadFont(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE)
    return;

  destroy_font_resources(vulkan);
}

static VkPipeline select_text_pipeline(VULKAN *vulkan, BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;
  return vulkan->text_pipeline_2d[mode];
}

static VkPipelineLayout select_text_layout(VULKAN *vulkan, BLB_RenderMode mode) {
  if (mode < BLB_RENDER_OPAQUE || mode >= BLB_RENDER_MODE_COUNT)
    mode = BLB_RENDER_OPAQUE;
  return vulkan->text_pipeline_layout_2d[mode];
}

void VULKAN_RendererDrawText(VULKAN *vulkan, const Font *font, const char *text, float x, float y, float glyph_scale, HMM_Vec2 transform_scale,
                             float rotation, float r, float g, float b, float a, float emission, float glow, float roundness, BLB_RenderMode render_mode) {
  if (!vulkan || !font || !text || vulkan->font_image_view == VK_NULL_HANDLE || vulkan->font_sampler == VK_NULL_HANDLE ||
      vulkan->text_descriptor_set == VK_NULL_HANDLE)
    return;

  if (glyph_scale <= 0.0f)
    glyph_scale = 1.0f;

  VulkanTextVertex *vertices = (VulkanTextVertex *)vulkan->text_vertex_buffers[vulkan->current_frame].mapped;
  size_t start_cursor = vulkan->text_vertex_cursor;

  float ascent = 0.0f;
  const char *measure = text;
  while (*measure != '\0') {
    uint32_t codepoint = utf8_decode(&measure);
    if (codepoint == '\n')
      break;
    const FontGlyph *glyph = BLB_FontGetGlyph(font, codepoint);
    if (glyph && glyph->height > 0 && (float)glyph->bearing_y > ascent)
      ascent = (float)glyph->bearing_y;
  }

  if (ascent <= 0.0f)
    ascent = (float)font->size * 0.8f;

  float pen_x = 0.0f;
  float baseline_y = ascent * glyph_scale;
  const char *cursor = text;

  while (*cursor != '\0') {
    uint32_t codepoint = utf8_decode(&cursor);

    if (codepoint == '\n') {
      pen_x = 0.0f;
      baseline_y += (float)font->size * 1.2f * glyph_scale;
      continue;
    }

    const FontGlyph *glyph = BLB_FontGetGlyph(font, codepoint);
    if (!glyph)
      continue;

    if (glyph->width == 0 || glyph->height == 0) {
      pen_x += (float)glyph->advance_x * glyph_scale;
      continue;
    }

    if (vulkan->text_vertex_cursor + 6 > VULKAN_MAX_TEXT_VERTICES)
      break;

    float px = pen_x + (float)glyph->bearing_x * glyph_scale;
    float py = baseline_y - (float)glyph->bearing_y * glyph_scale;
    float width = (float)glyph->width * glyph_scale;
    float height = (float)glyph->height * glyph_scale;

    float u0 = ((float)glyph->x + 0.5f) / (float)font->atlas_width;
    float u1 = ((float)(glyph->x + glyph->width) - 0.5f) / (float)font->atlas_width;
    float v0 = ((float)glyph->y + 0.5f) / (float)font->atlas_height;
    float v1 = ((float)(glyph->y + glyph->height) - 0.5f) / (float)font->atlas_height;

    size_t index = vulkan->text_vertex_cursor;
    vertices[index + 0] = (VulkanTextVertex){.position = {px, py, 0.0f}, .color = {r, g, b, a}, .uv = {u0, v0}};
    vertices[index + 1] = (VulkanTextVertex){.position = {px + width, py, 0.0f}, .color = {r, g, b, a}, .uv = {u1, v0}};
    vertices[index + 2] = (VulkanTextVertex){.position = {px + width, py + height, 0.0f}, .color = {r, g, b, a}, .uv = {u1, v1}};
    vertices[index + 3] = (VulkanTextVertex){.position = {px, py, 0.0f}, .color = {r, g, b, a}, .uv = {u0, v0}};
    vertices[index + 4] = (VulkanTextVertex){.position = {px + width, py + height, 0.0f}, .color = {r, g, b, a}, .uv = {u1, v1}};
    vertices[index + 5] = (VulkanTextVertex){.position = {px, py + height, 0.0f}, .color = {r, g, b, a}, .uv = {u0, v1}};

    vulkan->text_vertex_cursor += 6;
    pen_x += (float)glyph->advance_x * glyph_scale;
  }

  size_t vertex_count = vulkan->text_vertex_cursor - start_cursor;
  if (vertex_count == 0)
    return;

  float angle = HMM_AngleDeg(rotation);
  float cosine = cosf(angle);
  float sine = sinf(angle);

  for (size_t i = start_cursor; i < vulkan->text_vertex_cursor; i++) {
    float local_x = vertices[i].position[0];
    float local_y = vertices[i].position[1];
    vertices[i].position[0] = x + local_x * transform_scale.x * cosine - local_y * transform_scale.y * sine;
    vertices[i].position[1] = y + local_x * transform_scale.x * sine + local_y * transform_scale.y * cosine;
  }

  VulkanTextPushConstants data = {0};
  data.viewport[0] = (float)vulkan->swapchain_extent.width;
  data.viewport[1] = (float)vulkan->swapchain_extent.height;
  data.material[0] = emission;
  data.material[1] = glow;
  data.material[2] = roundness;
  data.material[3] = 0.0f;

  VkCommandBuffer command = vulkan->command_buffers[vulkan->current_frame];
  VkPipeline pipeline = select_text_pipeline(vulkan, render_mode);
  VkPipelineLayout layout = select_text_layout(vulkan, render_mode);
  VkBuffer buffer = vulkan->text_vertex_buffers[vulkan->current_frame].buffer;
  VkDeviceSize offset = start_cursor * sizeof(VulkanTextVertex);

  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command, 0, 1, &buffer, &offset);
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &vulkan->text_descriptor_set, 0, NULL);
  vkCmdPushConstants(command, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(data), &data);
  vkCmdDraw(command, (uint32_t)vertex_count, 1, 0, 0);
}
