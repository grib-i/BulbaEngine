#include "bulba/graphics/vulkan/renderer.h"

#include <stddef.h>
#include <stdint.h>

#include "basic2d_frag.h"
#include "basic2d_vert.h"
#include "basic3d_frag.h"
#include "basic3d_vert.h"
#include "shadow_vert.h"
#include "text2d_frag.h"
#include "text2d_vert.h"
#include "text3d_frag.h"
#include "text3d_vert.h"

int create_render_pass(VULKAN *vulkan) {
  VkAttachmentDescription color = {.format = vulkan->swapchain_format,
                                   .samples = VK_SAMPLE_COUNT_1_BIT,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                   .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

  VkAttachmentDescription depth = {.format = VK_FORMAT_D32_SFLOAT,
                                   .samples = VK_SAMPLE_COUNT_1_BIT,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                   .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkAttachmentDescription attachments[] = {color, depth};

  VkAttachmentReference color_ref = {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

  VkAttachmentReference depth_ref = {.attachment = 1, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass = {.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &color_ref,
                                  .pDepthStencilAttachment = &depth_ref};

  VkSubpassDependency dependency = {.srcSubpass = VK_SUBPASS_EXTERNAL,
                                    .dstSubpass = 0,
                                    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};

  VkRenderPassCreateInfo info = {.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                 .attachmentCount = 2,
                                 .pAttachments = attachments,
                                 .subpassCount = 1,
                                 .pSubpasses = &subpass,
                                 .dependencyCount = 1,
                                 .pDependencies = &dependency};

  return vkCreateRenderPass(vulkan->device, &info, NULL, &vulkan->render_pass) == VK_SUCCESS ? 0 : -1;
}

VkShaderModule BLB_LoadShaderFromMemory(VULKAN *vulkan, const void *code, size_t size) {
  if (!vulkan || !code || size == 0)
    return VK_NULL_HANDLE;

  if (size % sizeof(uint32_t) != 0)
    return VK_NULL_HANDLE;

  VkShaderModuleCreateInfo info = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = size, .pCode = (const uint32_t *)code};

  VkShaderModule module = VK_NULL_HANDLE;

  if (vkCreateShaderModule(vulkan->device, &info, NULL, &module) != VK_SUCCESS)
    return VK_NULL_HANDLE;

  return module;
}

static int create_basic_pipeline(VULKAN *vulkan, const void *vertex_code, size_t vertex_size, const void *fragment_code, size_t fragment_size,
                                 VkDescriptorSetLayout light_layout, VkPipelineLayout *pipeline_layout, VkPipeline *pipeline, bool is_2d,
                                 BLB_RenderMode mode) {
  VkShaderModule vert = BLB_LoadShaderFromMemory(vulkan, vertex_code, vertex_size);

  VkShaderModule frag = BLB_LoadShaderFromMemory(vulkan, fragment_code, fragment_size);

  if (!vert || !frag) {
    if (vert)
      vkDestroyShaderModule(vulkan->device, vert, NULL);

    if (frag)
      vkDestroyShaderModule(vulkan->device, frag, NULL);

    return -1;
  }

  VkPipelineShaderStageCreateInfo stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"}};

  VkVertexInputBindingDescription binding = {.binding = 0, .stride = sizeof(VulkanVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

  VkVertexInputAttributeDescription attributes[] = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 3},
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = sizeof(float) * 7},
      {.location = 3, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 10}};

  VkPipelineVertexInputStateCreateInfo vertex_input = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                       .vertexBindingDescriptionCount = 1,
                                                       .pVertexBindingDescriptions = &binding,
                                                       .vertexAttributeDescriptionCount = 4,
                                                       .pVertexAttributeDescriptions = attributes};

  VkPipelineInputAssemblyStateCreateInfo assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                     .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                     .primitiveRestartEnable = VK_FALSE};

  VkPipelineViewportStateCreateInfo viewport = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo raster = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .depthClampEnable = VK_FALSE,
                                                   .rasterizerDiscardEnable = VK_FALSE,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_NONE,
                                                   .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                   .depthBiasEnable = VK_FALSE,
                                                   .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo multisample = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkPipelineDepthStencilStateCreateInfo depth = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                 .depthTestEnable = is_2d ? VK_FALSE : VK_TRUE,
                                                 .depthWriteEnable = is_2d || mode != BLB_RENDER_OPAQUE ? VK_FALSE : VK_TRUE,
                                                 .depthCompareOp = is_2d ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_LESS,
                                                 .depthBoundsTestEnable = VK_FALSE,
                                                 .stencilTestEnable = VK_FALSE};

  VkPipelineColorBlendAttachmentState blend_attachment = {
      .blendEnable = mode == BLB_RENDER_OPAQUE ? VK_FALSE : VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = mode == BLB_RENDER_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = mode == BLB_RENDER_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = mode == BLB_RENDER_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo blend = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &blend_attachment};

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamic_states};

  VkPushConstantRange push_constant = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       .offset = 0,
                                       .size = is_2d ? sizeof(Vulkan2DPushConstants) : sizeof(VulkanLightingPushConstants)};

  VkDescriptorSetLayout set_layouts[] = {light_layout, vulkan->texture_descriptor_set_layout};

  VkPipelineLayoutCreateInfo layout = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                       .setLayoutCount = 2,
                                       .pSetLayouts = set_layouts,
                                       .pushConstantRangeCount = 1,
                                       .pPushConstantRanges = &push_constant};

  if (vkCreatePipelineLayout(vulkan->device, &layout, NULL, pipeline_layout) != VK_SUCCESS) {
    vkDestroyShaderModule(vulkan->device, vert, NULL);

    vkDestroyShaderModule(vulkan->device, frag, NULL);

    return -1;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                .stageCount = 2,
                                                .pStages = stages,
                                                .pVertexInputState = &vertex_input,
                                                .pInputAssemblyState = &assembly,
                                                .pViewportState = &viewport,
                                                .pRasterizationState = &raster,
                                                .pMultisampleState = &multisample,
                                                .pDepthStencilState = &depth,
                                                .pColorBlendState = &blend,
                                                .pDynamicState = &dynamic,
                                                .layout = *pipeline_layout,
                                                .renderPass = vulkan->render_pass,
                                                .subpass = 0};

  VkResult result = vkCreateGraphicsPipelines(vulkan->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, pipeline);

  vkDestroyShaderModule(vulkan->device, vert, NULL);

  vkDestroyShaderModule(vulkan->device, frag, NULL);

  if (result != VK_SUCCESS) {
    vkDestroyPipelineLayout(vulkan->device, *pipeline_layout, NULL);

    *pipeline_layout = VK_NULL_HANDLE;

    return -1;
  }

  return 0;
}

static int create_text_pipeline(VULKAN *vulkan, const void *vertex_code, size_t vertex_size, const void *fragment_code, size_t fragment_size,
                                VkPipelineLayout *pipeline_layout, VkPipeline *pipeline, BLB_RenderMode mode) {
  VkShaderModule vert = BLB_LoadShaderFromMemory(vulkan, vertex_code, vertex_size);

  VkShaderModule frag = BLB_LoadShaderFromMemory(vulkan, fragment_code, fragment_size);

  if (!vert || !frag) {
    if (vert)
      vkDestroyShaderModule(vulkan->device, vert, NULL);

    if (frag)
      vkDestroyShaderModule(vulkan->device, frag, NULL);

    return -1;
  }

  VkPipelineShaderStageCreateInfo stages[] = {
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"},
      {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main"}};

  VkVertexInputBindingDescription binding = {.binding = 0, .stride = sizeof(VulkanTextVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

  VkVertexInputAttributeDescription attributes[] = {
      {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0},
      {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT, .offset = sizeof(float) * 3},
      {.location = 2, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = sizeof(float) * 7}};

  VkPipelineVertexInputStateCreateInfo vertex_input = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                       .vertexBindingDescriptionCount = 1,
                                                       .pVertexBindingDescriptions = &binding,
                                                       .vertexAttributeDescriptionCount = 3,
                                                       .pVertexAttributeDescriptions = attributes};

  VkPipelineInputAssemblyStateCreateInfo assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                     .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                     .primitiveRestartEnable = VK_FALSE};

  VkPipelineViewportStateCreateInfo viewport = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo raster = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .depthClampEnable = VK_FALSE,
                                                   .rasterizerDiscardEnable = VK_FALSE,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_NONE,
                                                   .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                                                   .depthBiasEnable = VK_FALSE,
                                                   .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo multisample = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkPipelineDepthStencilStateCreateInfo depth = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                 .depthTestEnable = VK_FALSE,
                                                 .depthWriteEnable = VK_FALSE,
                                                 .depthCompareOp = VK_COMPARE_OP_ALWAYS};

  VkPipelineColorBlendAttachmentState blend_attachment = {
      .blendEnable = mode == BLB_RENDER_OPAQUE ? VK_FALSE : VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = mode == BLB_RENDER_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = mode == BLB_RENDER_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = mode == BLB_RENDER_ADDITIVE ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

  VkPipelineColorBlendStateCreateInfo blend = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1, .pAttachments = &blend_attachment};

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamic_states};

  VkPushConstantRange push_constant = {
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(VulkanTextPushConstants)};

  VkPipelineLayoutCreateInfo layout = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                       .setLayoutCount = 1,
                                       .pSetLayouts = &vulkan->text_descriptor_set_layout,
                                       .pushConstantRangeCount = 1,
                                       .pPushConstantRanges = &push_constant};

  if (vkCreatePipelineLayout(vulkan->device, &layout, NULL, pipeline_layout) != VK_SUCCESS) {
    vkDestroyShaderModule(vulkan->device, vert, NULL);

    vkDestroyShaderModule(vulkan->device, frag, NULL);

    return -1;
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                .stageCount = 2,
                                                .pStages = stages,
                                                .pVertexInputState = &vertex_input,
                                                .pInputAssemblyState = &assembly,
                                                .pViewportState = &viewport,
                                                .pRasterizationState = &raster,
                                                .pMultisampleState = &multisample,
                                                .pDepthStencilState = &depth,
                                                .pColorBlendState = &blend,
                                                .pDynamicState = &dynamic,
                                                .layout = *pipeline_layout,
                                                .renderPass = vulkan->render_pass,
                                                .subpass = 0};

  VkResult result = vkCreateGraphicsPipelines(vulkan->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, pipeline);

  vkDestroyShaderModule(vulkan->device, vert, NULL);

  vkDestroyShaderModule(vulkan->device, frag, NULL);

  if (result != VK_SUCCESS) {
    vkDestroyPipelineLayout(vulkan->device, *pipeline_layout, NULL);

    *pipeline_layout = VK_NULL_HANDLE;
    return -1;
  }

  return 0;
}

int create_shadow_pipeline(VULKAN *vulkan) {
  VkShaderModule vert = BLB_LoadShaderFromMemory(vulkan, blb_shader_shadow_vert, blb_shader_shadow_vert_size);

  if (vert == VK_NULL_HANDLE)
    return -1;

  VkPipelineShaderStageCreateInfo stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main"};

  VkVertexInputBindingDescription binding = {.binding = 0, .stride = sizeof(VulkanShadowVertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};

  VkVertexInputAttributeDescription attribute = {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = 0};

  VkPipelineVertexInputStateCreateInfo vertex_input = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                       .vertexBindingDescriptionCount = 1,
                                                       .pVertexBindingDescriptions = &binding,
                                                       .vertexAttributeDescriptionCount = 1,
                                                       .pVertexAttributeDescriptions = &attribute};

  VkPipelineInputAssemblyStateCreateInfo assembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                     .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkPipelineViewportStateCreateInfo viewport = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo raster = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_BACK_BIT,
                                                   .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                                   .lineWidth = 1.0f};

  VkPipelineMultisampleStateCreateInfo multisample = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

  VkPipelineDepthStencilStateCreateInfo depth = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                 .depthTestEnable = VK_TRUE,
                                                 .depthWriteEnable = VK_TRUE,
                                                 .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2, .pDynamicStates = dynamic_states};

  VkPushConstantRange push_constant = {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .size = sizeof(VulkanShadowPushConstants)};

  VkPipelineLayoutCreateInfo layout = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pushConstantRangeCount = 1, .pPushConstantRanges = &push_constant};

  if (vkCreatePipelineLayout(vulkan->device, &layout, NULL, &vulkan->shadow_pipeline_layout) != VK_SUCCESS) {
    vkDestroyShaderModule(vulkan->device, vert, NULL);

    return -1;
  }

  VkGraphicsPipelineCreateInfo info = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                       .stageCount = 1,
                                       .pStages = &stage,
                                       .pVertexInputState = &vertex_input,
                                       .pInputAssemblyState = &assembly,
                                       .pViewportState = &viewport,
                                       .pRasterizationState = &raster,
                                       .pMultisampleState = &multisample,
                                       .pDepthStencilState = &depth,
                                       .layout = vulkan->shadow_pipeline_layout,
                                       .renderPass = vulkan->shadow_render_pass,
                                       .subpass = 0};

  VkResult result = vkCreateGraphicsPipelines(vulkan->device, VK_NULL_HANDLE, 1, &info, NULL, &vulkan->shadow_pipeline);

  vkDestroyShaderModule(vulkan->device, vert, NULL);

  if (result != VK_SUCCESS) {
    vkDestroyPipelineLayout(vulkan->device, vulkan->shadow_pipeline_layout, NULL);

    vulkan->shadow_pipeline_layout = VK_NULL_HANDLE;

    return -1;
  }

  return 0;
}

int create_pipelines(VULKAN *vulkan) {
  if (create_shadow_pipeline(vulkan) != 0)
    return -1;

  for (int mode = 0; mode < BLB_RENDER_MODE_COUNT; mode++) {
    if (create_basic_pipeline(vulkan, blb_shader_basic3d_vert, blb_shader_basic3d_vert_size, blb_shader_basic3d_frag, blb_shader_basic3d_frag_size,
                              vulkan->light_descriptor_set_layout_3d, &vulkan->pipeline_layout_3d[mode], &vulkan->pipeline_3d[mode], false,
                              (BLB_RenderMode)mode) != 0)
      return -1;

    if (create_basic_pipeline(vulkan, blb_shader_basic2d_vert, blb_shader_basic2d_vert_size, blb_shader_basic2d_frag, blb_shader_basic2d_frag_size,
                              vulkan->light_descriptor_set_layout_2d, &vulkan->pipeline_layout_2d[mode], &vulkan->pipeline_2d[mode], true,
                              (BLB_RenderMode)mode) != 0)
      return -1;

    if (create_text_pipeline(vulkan, blb_shader_text3d_vert, blb_shader_text3d_vert_size, blb_shader_text3d_frag, blb_shader_text3d_frag_size,
                             &vulkan->text_pipeline_layout_3d[mode], &vulkan->text_pipeline_3d[mode], (BLB_RenderMode)mode) != 0)
      return -1;

    if (create_text_pipeline(vulkan, blb_shader_text2d_vert, blb_shader_text2d_vert_size, blb_shader_text2d_frag, blb_shader_text2d_frag_size,
                             &vulkan->text_pipeline_layout_2d[mode], &vulkan->text_pipeline_2d[mode], (BLB_RenderMode)mode) != 0)
      return -1;
  }

  return 0;
}

int create_text_descriptor_layout(VULKAN *vulkan) {
  VkDescriptorSetLayoutBinding binding = {
      .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};

  VkDescriptorSetLayoutCreateInfo info = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding};

  if (vkCreateDescriptorSetLayout(vulkan->device, &info, NULL, &vulkan->text_descriptor_set_layout) != VK_SUCCESS)
    return -1;

  return 0;
}
