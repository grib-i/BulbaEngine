#include "bulba/graphics/vulkan/renderer.h"

#include <string.h>

static int create_shadow_render_pass(VULKAN *vulkan) {
  VkAttachmentDescription depth = {.format = VK_FORMAT_D32_SFLOAT,
                                   .samples = VK_SAMPLE_COUNT_1_BIT,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                   .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

  VkAttachmentReference depth_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .pDepthStencilAttachment = &depth_ref};

  VkSubpassDependency dependencies[2] = {{.srcSubpass = VK_SUBPASS_EXTERNAL,
                                          .dstSubpass = 0,
                                          .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                          .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
                                          .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                                         {.srcSubpass = 0,
                                          .dstSubpass = VK_SUBPASS_EXTERNAL,
                                          .srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                          .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT}};

  VkRenderPassCreateInfo info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 1,
                                 .pAttachments = &depth,
                                 .subpassCount = 1,
                                 .pSubpasses = &subpass,
                                 .dependencyCount = 2,
                                 .pDependencies = dependencies};

  if (vkCreateRenderPass(vulkan->device, &info, NULL, &vulkan->shadow_render_pass) != VK_SUCCESS)
    return -1;

  return 0;
}

int create_shadow_resources(VULKAN *vulkan) {
  if (!vulkan)
    return -1;

  if (create_shadow_render_pass(vulkan) != 0)
    return -1;

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (create_image(vulkan, VULKAN_SHADOW_MAP_SIZE, VULKAN_SHADOW_MAP_SIZE, VK_FORMAT_D32_SFLOAT,
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &vulkan->shadow_images[i],
                     &vulkan->shadow_memories[i]) != 0)
      goto fail;

    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = vulkan->shadow_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};

    if (vkCreateImageView(vulkan->device, &view, NULL, &vulkan->shadow_image_views[i]) != VK_SUCCESS)
      goto fail;

    VkFramebufferCreateInfo framebuffer = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                           .renderPass = vulkan->shadow_render_pass,
                                           .attachmentCount = 1,
                                           .pAttachments = &vulkan->shadow_image_views[i],
                                           .width = VULKAN_SHADOW_MAP_SIZE,
                                           .height = VULKAN_SHADOW_MAP_SIZE,
                                           .layers = 1};

    if (vkCreateFramebuffer(vulkan->device, &framebuffer, NULL, &vulkan->shadow_framebuffers[i]) != VK_SUCCESS)
      goto fail;
  }

  VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                 .magFilter = VK_FILTER_LINEAR,
                                 .minFilter = VK_FILTER_LINEAR,
                                 .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                 .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                 .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                 .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                 .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                                 .compareEnable = VK_TRUE,
                                 .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
                                 .minLod = 0.0f,
                                 .maxLod = 0.0f};

  if (vkCreateSampler(vulkan->device, &sampler, NULL, &vulkan->shadow_sampler) != VK_SUCCESS)
    goto fail;

  return 0;

fail:
  destroy_shadow_resources(vulkan);
  return -1;
}

void destroy_shadow_resources(VULKAN *vulkan) {
  if (!vulkan || vulkan->device == VK_NULL_HANDLE)
    return;

  if (vulkan->shadow_sampler != VK_NULL_HANDLE) {
    vkDestroySampler(vulkan->device, vulkan->shadow_sampler, NULL);
    vulkan->shadow_sampler = VK_NULL_HANDLE;
  }

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (vulkan->shadow_framebuffers[i] != VK_NULL_HANDLE) {
      vkDestroyFramebuffer(vulkan->device, vulkan->shadow_framebuffers[i], NULL);
      vulkan->shadow_framebuffers[i] = VK_NULL_HANDLE;
    }

    if (vulkan->shadow_image_views[i] != VK_NULL_HANDLE) {
      vkDestroyImageView(vulkan->device, vulkan->shadow_image_views[i], NULL);
      vulkan->shadow_image_views[i] = VK_NULL_HANDLE;
    }

    if (vulkan->shadow_images[i] != VK_NULL_HANDLE) {
      vkDestroyImage(vulkan->device, vulkan->shadow_images[i], NULL);
      vulkan->shadow_images[i] = VK_NULL_HANDLE;
    }

    if (vulkan->shadow_memories[i] != VK_NULL_HANDLE) {
      vkFreeMemory(vulkan->device, vulkan->shadow_memories[i], NULL);
      vulkan->shadow_memories[i] = VK_NULL_HANDLE;
    }
  }

  if (vulkan->shadow_pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(vulkan->device, vulkan->shadow_pipeline, NULL);
    vulkan->shadow_pipeline = VK_NULL_HANDLE;
  }

  if (vulkan->shadow_pipeline_layout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(vulkan->device, vulkan->shadow_pipeline_layout, NULL);
    vulkan->shadow_pipeline_layout = VK_NULL_HANDLE;
  }

  if (vulkan->shadow_render_pass != VK_NULL_HANDLE) {
    vkDestroyRenderPass(vulkan->device, vulkan->shadow_render_pass, NULL);
    vulkan->shadow_render_pass = VK_NULL_HANDLE;
  }
}
