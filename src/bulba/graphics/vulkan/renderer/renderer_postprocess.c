#include "bulba/core/utils/config.h"
#include "bulba/graphics/vulkan/renderer.h"
#include "postprocess_frag.h"
#include "postprocess_vert.h"
#include <math.h>
#include <stdlib.h>

typedef struct {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkFramebuffer framebuffer;
  VkDescriptorSet descriptor;
} PostImage;

typedef struct {
  VkRenderPass pass;
  VkSampler sampler;
  VkDescriptorPool pool;
  VkDescriptorSetLayout descriptor_layout;
  VkPipelineLayout layout;
  VkPipeline pipeline;
  uint32_t image_count;
  PostImage *images;
} Postprocess;

void VULKAN_PostprocessDestroy(VULKAN *v) {
  Postprocess *post = v ? v->postprocess : NULL;

  if (!post)
    return;

  for (uint32_t i = 0; post->images && i < post->image_count; ++i) {
    PostImage *image = &post->images[i];

    if (image->framebuffer)
      vkDestroyFramebuffer(v->device, image->framebuffer, NULL);

    if (image->view)
      vkDestroyImageView(v->device, image->view, NULL);

    if (image->image)
      vkDestroyImage(v->device, image->image, NULL);

    if (image->memory)
      vkFreeMemory(v->device, image->memory, NULL);
  }

  if (post->pipeline)
    vkDestroyPipeline(v->device, post->pipeline, NULL);

  if (post->layout)
    vkDestroyPipelineLayout(v->device, post->layout, NULL);

  if (post->pool)
    vkDestroyDescriptorPool(v->device, post->pool, NULL);

  if (post->descriptor_layout)
    vkDestroyDescriptorSetLayout(v->device, post->descriptor_layout, NULL);

  if (post->sampler)
    vkDestroySampler(v->device, post->sampler, NULL);

  if (post->pass)
    vkDestroyRenderPass(v->device, post->pass, NULL);
  free(post->images);
  free(post);
  v->postprocess = NULL;
}

VkImageView VULKAN_PostprocessView(VULKAN *v, uint32_t image) {
  Postprocess *post = v->postprocess;

  return post && image < post->image_count ? post->images[image].view : v->swapchain_image_views[image];
}

static int post_pipeline(VULKAN *v, Postprocess *post) {
  VkShaderModule vert = BLB_LoadShaderFromMemory(v, blb_shader_postprocess_vert, blb_shader_postprocess_vert_size);
  VkShaderModule frag = BLB_LoadShaderFromMemory(v, blb_shader_postprocess_frag, blb_shader_postprocess_frag_size);

  if (!vert || !frag) {
    if (vert)
      vkDestroyShaderModule(v->device, vert, NULL);

    if (frag)
      vkDestroyShaderModule(v->device, frag, NULL);

    return -1;
  }
  VkPipelineShaderStageCreateInfo stages[2] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"}};
  VkPipelineVertexInputStateCreateInfo vertex = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
  VkPipelineInputAssemblyStateCreateInfo assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                     .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
  VkPipelineViewportStateCreateInfo viewport = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
  VkPipelineRasterizationStateCreateInfo raster = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_NONE,
                                                   .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                   .lineWidth = 1.0f};
  VkPipelineMultisampleStateCreateInfo samples = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  VkPipelineColorBlendAttachmentState attachment = {.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                                                      VK_COLOR_COMPONENT_A_BIT};
  VkPipelineColorBlendStateCreateInfo blend = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &attachment};
  VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = states};
  VkPushConstantRange push = {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .size = 64};
  VkPipelineLayoutCreateInfo layout = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                       .setLayoutCount = 1,
                                       .pSetLayouts = &post->descriptor_layout,
                                       .pushConstantRangeCount = 1,
                                       .pPushConstantRanges = &push};
  int result = -1;

  if (vkCreatePipelineLayout(v->device, &layout, NULL, &post->layout) == VK_SUCCESS) {
    VkGraphicsPipelineCreateInfo info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                         .stageCount = 2,
                                         .pStages = stages,
                                         .pVertexInputState = &vertex,
                                         .pInputAssemblyState = &assembly,
                                         .pViewportState = &viewport,
                                         .pRasterizationState = &raster,
                                         .pMultisampleState = &samples,
                                         .pColorBlendState = &blend,
                                         .pDynamicState = &dynamic,
                                         .layout = post->layout,
                                         .renderPass = post->pass};
    result = vkCreateGraphicsPipelines(v->device, VK_NULL_HANDLE, 1, &info, NULL, &post->pipeline) == VK_SUCCESS ? 0 : -1;
  }
  vkDestroyShaderModule(v->device, vert, NULL);
  vkDestroyShaderModule(v->device, frag, NULL);

  return result;
}

int VULKAN_PostprocessCreate(VULKAN *v) {
  if (!v || !v->device || !v->swapchain_image_count)
    return -1;
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(v->physical_device, VK_FORMAT_R16G16B16A16_SFLOAT, &properties);
  VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT |
                                  VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

  if ((properties.optimalTilingFeatures & required) != required)
    return -1;
  Postprocess *post = calloc(1, sizeof(*post));

  if (!post)
    return -1;
  v->postprocess = post;
  post->image_count = v->swapchain_image_count;
  post->images = calloc(post->image_count, sizeof(*post->images));

  if (!post->images)
    goto fail;
  VkAttachmentDescription color = {.format = v->swapchain_format,
                                   .samples = VK_SAMPLE_COUNT_1_BIT,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                   .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};
  VkAttachmentReference reference = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription subpass = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &reference};
  VkSubpassDependency dependency = {.srcSubpass = VK_SUBPASS_EXTERNAL,
                                    .dstSubpass = 0,
                                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
  VkRenderPassCreateInfo pass = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 1,
                                 .pAttachments = &color,
                                 .subpassCount = 1,
                                 .pSubpasses = &subpass,
                                 .dependencyCount = 1,
                                 .pDependencies = &dependency};

  if (vkCreateRenderPass(v->device, &pass, NULL, &post->pass) != VK_SUCCESS)
    goto fail;
  VkSamplerCreateInfo sampler = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                 .magFilter = VK_FILTER_LINEAR,
                                 .minFilter = VK_FILTER_LINEAR,
                                 .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                                 .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                 .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                 .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};

  if (vkCreateSampler(v->device, &sampler, NULL, &post->sampler) != VK_SUCCESS)
    goto fail;
  VkDescriptorSetLayoutBinding binding = {
      .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
  VkDescriptorSetLayoutCreateInfo descriptor_layout = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding};

  if (vkCreateDescriptorSetLayout(v->device, &descriptor_layout, NULL, &post->descriptor_layout) != VK_SUCCESS)
    goto fail;
  VkDescriptorPoolSize pool_size = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = post->image_count};
  VkDescriptorPoolCreateInfo pool = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = post->image_count, .poolSizeCount = 1, .pPoolSizes = &pool_size};

  if (vkCreateDescriptorPool(v->device, &pool, NULL, &post->pool) != VK_SUCCESS)
    goto fail;

  for (uint32_t i = 0; i < post->image_count; ++i) {
    PostImage *image = &post->images[i];

    if (create_image(v, v->swapchain_extent.width, v->swapchain_extent.height, VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &image->image, &image->memory) != 0)
      goto fail;
    VkImageViewCreateInfo view = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                  .image = image->image,
                                  .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                  .format = VK_FORMAT_R16G16B16A16_SFLOAT,
                                  .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};

    if (vkCreateImageView(v->device, &view, NULL, &image->view) != VK_SUCCESS)
      goto fail;
    VkFramebufferCreateInfo framebuffer = {.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                           .renderPass = post->pass,
                                           .attachmentCount = 1,
                                           .pAttachments = &v->swapchain_image_views[i],
                                           .width = v->swapchain_extent.width,
                                           .height = v->swapchain_extent.height,
                                           .layers = 1};

    if (vkCreateFramebuffer(v->device, &framebuffer, NULL, &image->framebuffer) != VK_SUCCESS)
      goto fail;
    VkDescriptorSetAllocateInfo allocate = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                            .descriptorPool = post->pool,
                                            .descriptorSetCount = 1,
                                            .pSetLayouts = &post->descriptor_layout};

    if (vkAllocateDescriptorSets(v->device, &allocate, &image->descriptor) != VK_SUCCESS)
      goto fail;
    VkDescriptorImageInfo source = {.sampler = post->sampler, .imageView = image->view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                  .dstSet = image->descriptor,
                                  .dstBinding = 0,
                                  .descriptorCount = 1,
                                  .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                  .pImageInfo = &source};
    vkUpdateDescriptorSets(v->device, 1, &write, 0, NULL);
  }

  if (post_pipeline(v, post) != 0)
    goto fail;

  return 0;
fail:
  VULKAN_PostprocessDestroy(v);

  return -1;
}

static float post_value(float value, float fallback, float low, float high) {
  return isfinite(value) ? fminf(fmaxf(value, low), high) : fallback;
}

void VULKAN_PostprocessDraw(VULKAN *v) {
  Postprocess *post = v ? v->postprocess : NULL;

  if (!post || v->current_image >= post->image_count)
    return;
  PostImage *image = &post->images[v->current_image];
  VkCommandBuffer command = v->command_buffers[v->current_frame];
  VkRenderPassBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                 .renderPass = post->pass,
                                 .framebuffer = image->framebuffer,
                                 .renderArea = {.extent = v->swapchain_extent}};
  vkCmdBeginRenderPass(command, &begin, VK_SUBPASS_CONTENTS_INLINE);
  VkViewport viewport = {.width = (float)v->swapchain_extent.width, .height = (float)v->swapchain_extent.height, .maxDepth = 1.0f};
  VkRect2D scissor = {.extent = v->swapchain_extent};
  vkCmdSetViewport(command, 0, 1, &viewport);
  vkCmdSetScissor(command, 0, 1, &scissor);
  bool correction = BLB_POSTPROCESS_COLOR_CORRECTION;
  bool srgb = v->swapchain_format == VK_FORMAT_B8G8R8A8_SRGB || v->swapchain_format == VK_FORMAT_R8G8B8A8_SRGB;
  float settings[16] = {BLB_POSTPROCESS ? 1.0f : 0.0f,
                        correction ? exp2f(post_value(BLB_POSTPROCESS_EXPOSURE, 0, -16, 16)) : 1.0f,
                        correction ? post_value(BLB_POSTPROCESS_CONTRAST, 1, 0, 4) : 1.0f,
                        correction ? post_value(BLB_POSTPROCESS_SATURATION, 1, 0, 4) : 1.0f,
                        correction ? post_value(BLB_POSTPROCESS_GAMMA, 1, 0.1f, 4) : 1.0f,
                        (float)(BLB_POSTPROCESS_TONEMAP >= 0 && BLB_POSTPROCESS_TONEMAP <= 2 ? BLB_POSTPROCESS_TONEMAP : 0),
                        0,
                        post_value(BLB_POSTPROCESS_VIGNETTE, 0, 0, 2),
                        BLB_POSTPROCESS_BLOOM ? post_value(BLB_POSTPROCESS_BLOOM_STRENGTH, 0.15f, 0, 4) : 0,
                        post_value(BLB_POSTPROCESS_BLOOM_THRESHOLD, 1, 0, 64),
                        post_value(BLB_POSTPROCESS_BLOOM_RADIUS, 2, 0, 32),
                        BLB_POSTPROCESS_FXAA ? 1.0f : 0.0f,
                        1.0f / viewport.width,
                        1.0f / viewport.height,
                        srgb ? 0.0f : 1.0f,
                        0};
  vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, post->pipeline);
  vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, post->layout, 0, 1, &image->descriptor, 0, NULL);
  vkCmdPushConstants(command, post->layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(settings), settings);
  vkCmdDraw(command, 3, 1, 0, 0);
  vkCmdEndRenderPass(command);
  v->bound_pipeline = VK_NULL_HANDLE;
}
