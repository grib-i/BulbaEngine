#include "bulba/graphics/vulkan/renderer.h"

#include "bulba/core/math3v/lights.h"

#include <string.h>

static void write_light_3d(VulkanGPULight *destination, BLB_Light3D *source) {
  if (!destination || !source)
    return;

  HMM_Vec3 direction = BLB_GetLightDirection3D(source);

  destination->direction[0] = direction.x;
  destination->direction[1] = direction.y;
  destination->direction[2] = direction.z;
  destination->direction[3] = 0.0f;

  destination->position[0] = source->object.position.x;
  destination->position[1] = source->object.position.y;
  destination->position[2] = source->object.position.z;
  destination->position[3] = 0.0f;

  destination->color[0] = source->object.color[0] / 255.0f;

  destination->color[1] = source->object.color[1] / 255.0f;

  destination->color[2] = source->object.color[2] / 255.0f;

  destination->color[3] = source->object.color[3] / 255.0f;

  destination->parameters[0] = source->intensity;
  destination->parameters[1] = source->ambient;
  destination->parameters[2] = source->specular;
  destination->parameters[3] = source->shininess;

  destination->cone[0] = source->range;
  destination->cone[1] = source->inner_cone;
  destination->cone[2] = source->outer_cone;
  destination->cone[3] = (float)source->type;
}

static void write_light_2d(VulkanGPULight *destination, BLB_Light2D *source) {
  if (!destination || !source)
    return;

  HMM_Vec2 direction = BLB_GetLightDirection2D(source);

  destination->direction[0] = direction.x;
  destination->direction[1] = direction.y;
  destination->direction[2] = 0.0f;
  destination->direction[3] = 0.0f;

  destination->position[0] = source->object.position.x;
  destination->position[1] = source->object.position.y;
  destination->position[2] = 0.0f;
  destination->position[3] = 0.0f;

  destination->color[0] = source->object.color[0] / 255.0f;

  destination->color[1] = source->object.color[1] / 255.0f;

  destination->color[2] = source->object.color[2] / 255.0f;

  destination->color[3] = source->object.color[3] / 255.0f;

  destination->parameters[0] = source->intensity;
  destination->parameters[1] = source->ambient;
  destination->parameters[2] = source->specular;
  destination->parameters[3] = source->shininess;

  destination->cone[0] = source->range;
  destination->cone[1] = source->inner_cone;
  destination->cone[2] = source->outer_cone;
  destination->cone[3] = (float)source->type;
}

void update_light_buffer_3d(VULKAN *vulkan) {
  if (!vulkan)
    return;

  size_t count = vulkan->light3d_count;

  if (count > VULKAN_MAX_LIGHTS)
    count = VULKAN_MAX_LIGHTS;

  VulkanLightBufferData *buffer = (VulkanLightBufferData *)vulkan->light_buffers_3d[vulkan->current_frame].mapped;

  memset(buffer, 0, sizeof(*buffer));

  buffer->camera_position[0] = vulkan->camera_position.x;

  buffer->camera_position[1] = vulkan->camera_position.y;

  buffer->camera_position[2] = vulkan->camera_position.z;

  buffer->camera_position[3] = 1.0f;

  memcpy(buffer->shadow_mvp, &vulkan->shadow_mvp.Elements[0][0], sizeof(buffer->shadow_mvp));

  buffer->shadow_params[0] = vulkan->shadow_enabled ? 1.0f : 0.0f;

  buffer->shadow_params[1] = vulkan->shadow_bias;

  for (size_t i = 0; i < count; i++) {
    BLB_Light3D *source = vulkan->lights3d[i];

    if (!source || !source->enabled)
      continue;

    if (buffer->light_count >= VULKAN_MAX_LIGHTS)
      break;

    write_light_3d(&buffer->lights[buffer->light_count], source);

    buffer->light_count++;
  }
}

void update_light_buffer_2d(VULKAN *vulkan) {
  if (!vulkan)
    return;

  size_t count = vulkan->light2d_count;

  if (count > VULKAN_MAX_LIGHTS)
    count = VULKAN_MAX_LIGHTS;

  VulkanLightBufferData *buffer = (VulkanLightBufferData *)vulkan->light_buffers_2d[vulkan->current_frame].mapped;

  memset(buffer, 0, sizeof(*buffer));

  buffer->camera_position[0] = vulkan->camera_position.x;

  buffer->camera_position[1] = vulkan->camera_position.y;

  buffer->camera_position[2] = vulkan->camera_position.z;

  buffer->camera_position[3] = 1.0f;

  for (size_t i = 0; i < count; i++) {
    BLB_Light2D *source = vulkan->lights2d[i];

    if (!source || !source->enabled)
      continue;

    if (buffer->light_count >= VULKAN_MAX_LIGHTS)
      break;

    write_light_2d(&buffer->lights[buffer->light_count], source);

    buffer->light_count++;
  }
}

int create_light_descriptor_layout(VULKAN *vulkan, VkDescriptorSetLayout *layout) {
  VkDescriptorSetLayoutBinding bindings[] = {
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };

  VkDescriptorSetLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = bindings,
  };

  if (vkCreateDescriptorSetLayout(vulkan->device, &info, NULL, layout) != VK_SUCCESS)
    return -1;

  return 0;
}

int create_light_buffers(VULKAN *vulkan, VULKAN_Buffer *buffers) {
  if (!vulkan || !buffers)
    return -1;

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    if (create_buffer(vulkan, sizeof(VulkanLightBufferData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &buffers[i]) != 0) {
      for (uint32_t j = 0; j < i; j++)
        destroy_buffer(vulkan, &buffers[j]);

      return -1;
    }
  }

  return 0;
}

void destroy_light_buffers(VULKAN *vulkan, VULKAN_Buffer *buffers) {
  if (!vulkan || !buffers)
    return;

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    destroy_buffer(vulkan, &buffers[i]);
  }
}

int create_light_descriptors(VULKAN *vulkan, VkDescriptorSetLayout layout, VkDescriptorPool *pool, VkDescriptorSet *sets, VULKAN_Buffer *buffers) {
  if (!vulkan || !pool || !sets || !buffers)
    return -1;

  VkDescriptorPoolSize pool_sizes[] = {
      {
          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT,
      },
  };

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = VULKAN_MAX_FRAMES_IN_FLIGHT,
      .poolSizeCount = 2,
      .pPoolSizes = pool_sizes,
  };

  if (vkCreateDescriptorPool(vulkan->device, &pool_info, NULL, pool) != VK_SUCCESS)
    return -1;

  VkDescriptorSetLayout layouts[VULKAN_MAX_FRAMES_IN_FLIGHT];

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    layouts[i] = layout;
  }

  VkDescriptorSetAllocateInfo alloc = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = *pool,
      .descriptorSetCount = VULKAN_MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = layouts,
  };

  if (vkAllocateDescriptorSets(vulkan->device, &alloc, sets) != VK_SUCCESS) {
    vkDestroyDescriptorPool(vulkan->device, *pool, NULL);

    *pool = VK_NULL_HANDLE;

    return -1;
  }

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffers[i].buffer,
        .offset = 0,
        .range = sizeof(VulkanLightBufferData),
    };

    VkDescriptorImageInfo shadow_image = {
        .sampler = vulkan->shadow_sampler,
        .imageView = vulkan->shadow_image_views[i],
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet writes[2] = {0};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = sets[i];
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo = &buffer_info;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = sets[i];
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &shadow_image;

    vkUpdateDescriptorSets(vulkan->device, 2, writes, 0, NULL);
  }

  return 0;
}

void destroy_light_descriptors(VULKAN *vulkan, VkDescriptorPool *pool, VkDescriptorSetLayout *layout) {
  if (!vulkan)
    return;

  if (pool && *pool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(vulkan->device, *pool, NULL);

    *pool = VK_NULL_HANDLE;
  }

  if (layout && *layout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(vulkan->device, *layout, NULL);

    *layout = VK_NULL_HANDLE;
  }
}

void VULKAN_RendererSetCameraPosition(VULKAN *vulkan, HMM_Vec3 position) {
  if (!vulkan)
    return;

  vulkan->camera_position = position;

  update_light_buffer_3d(vulkan);
  update_light_buffer_2d(vulkan);
}

void VULKAN_RendererSetLights3D(VULKAN *vulkan, BLB_Light3D **lights, size_t light_count) {
  if (!vulkan)
    return;

  if (light_count > VULKAN_MAX_LIGHTS)
    light_count = VULKAN_MAX_LIGHTS;

  vulkan->light3d_count = light_count;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++) {
    vulkan->lights3d[i] = NULL;
  }

  for (size_t i = 0; i < light_count; i++) {
    vulkan->lights3d[i] = lights ? lights[i] : NULL;
  }

  update_light_buffer_3d(vulkan);
}

void VULKAN_RendererClearLights3D(VULKAN *vulkan) {
  if (!vulkan)
    return;

  vulkan->light3d_count = 0;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++) {
    vulkan->lights3d[i] = NULL;
  }

  update_light_buffer_3d(vulkan);
}

void VULKAN_RendererSetLights2D(VULKAN *vulkan, BLB_Light2D **lights, size_t light_count) {
  if (!vulkan)
    return;

  if (light_count > VULKAN_MAX_LIGHTS)
    light_count = VULKAN_MAX_LIGHTS;

  vulkan->light2d_count = light_count;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++) {
    vulkan->lights2d[i] = NULL;
  }

  for (size_t i = 0; i < light_count; i++) {
    vulkan->lights2d[i] = lights ? lights[i] : NULL;
  }

  update_light_buffer_2d(vulkan);
}

void VULKAN_RendererClearLights2D(VULKAN *vulkan) {
  if (!vulkan)
    return;

  vulkan->light2d_count = 0;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++) {
    vulkan->lights2d[i] = NULL;
  }

  update_light_buffer_2d(vulkan);
}
