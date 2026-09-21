#include "bulba/graphics/vulkan/renderer.h"

#include "bulba/core/math3v/lights.h"

#include <math.h>
#include <string.h>

static void write_light_3d(VulkanGPULight *destination, BLB_Light3D *source) {
  if (!destination || !source || !source->object)
    return;

  HMM_Vec3 direction = BLB_GetLightDirection3D(source);

  destination->direction[0] = direction.x;
  destination->direction[1] = direction.y;
  destination->direction[2] = direction.z;
  destination->direction[3] = 0.0f;

  destination->position[0] = source->object->position.x;

  destination->position[1] = source->object->position.y;

  destination->position[2] = source->object->position.z;

  destination->position[3] = 1.0f;

  destination->color[0] = source->object->color[0] / 255.0f;

  destination->color[1] = source->object->color[1] / 255.0f;

  destination->color[2] = source->object->color[2] / 255.0f;

  destination->color[3] = source->object->color[3] / 255.0f;

  destination->parameters[0] = source->intensity;

  destination->parameters[1] = source->ambient;

  destination->parameters[2] = source->specular;

  destination->parameters[3] = 0.0f;

  destination->cone[0] = source->range;

  destination->cone[1] = source->inner_cone;

  destination->cone[2] = source->outer_cone;

  destination->cone[3] = (float)source->type;
}

static HMM_Vec3 emission_temperature_color(float kelvin) {
  if (kelvin <= 0.0f)
    return HMM_V3(1.0f, 1.0f, 1.0f);

  float temperature = fminf(fmaxf(kelvin, 1000.0f), 40000.0f) / 100.0f;
  float red;
  float green;
  float blue;

  if (temperature <= 66.0f)
    red = 1.0f;
  else
    red = fminf(fmaxf(1.29293618662f * powf(temperature - 60.0f, -0.1332047592f), 0.0f), 1.0f);

  if (temperature <= 66.0f)
    green = fminf(fmaxf(0.390081578769f * logf(fmaxf(temperature, 1.0f)) - 0.63184144377f, 0.0f), 1.0f);
  else
    green = fminf(fmaxf(1.129890860895f * powf(temperature - 60.0f, -0.0755148492f), 0.0f), 1.0f);

  if (temperature >= 66.0f)
    blue = 1.0f;
  else if (temperature <= 19.0f)
    blue = 0.0f;
  else
    blue = fminf(fmaxf(0.54320678911f * logf(temperature - 10.0f) - 1.19625408914f, 0.0f), 1.0f);

  return HMM_V3(red, green, blue);
}

static HMM_Vec3 transform_emissive_point_3d(const BLB_Object3D *object, HMM_Vec3 local) {
  HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));
  HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));
  HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));
  HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));
  HMM_Vec4 local_position = HMM_V4(local.x * object->scale.x, local.y * object->scale.y, local.z * object->scale.z, 1.0f);
  HMM_Vec4 rotated = HMM_MulM4V4(rotation, local_position);
  return HMM_AddV3(object->position, HMM_V3(rotated.x, rotated.y, rotated.z));
}

static size_t write_emissive_lights_3d(VulkanGPULight *destination, size_t capacity, BLB_Object3D *object) {
  if (!destination || !object || capacity == 0 || !object->visible || !object->material || !object->polygon || object->polygon->vertex_count == 0)
    return 0;

  BLB_Material *material = object->material;
  float total_strength = fmaxf(material->emission_strength, 0.0f) * 4.0f + fminf(fmaxf(material->glow_strength, 0.0f) * 0.02f, 1.5f);

  if (total_strength <= 0.0001f)
    return 0;

  HMM_Vec3 temperature = emission_temperature_color(material->temperature);
  size_t sample_count = object->polygon->vertex_count < 6 ? object->polygon->vertex_count : 6;
  if (sample_count == 0)
    return 0;

  float sample_strength = total_strength / (float)sample_count;
  float range = fmaxf(material->glow_radius, 6.0f + sqrtf(total_strength) * 4.0f);
  size_t written = 0;

  for (size_t sample = 0; sample < sample_count && written < capacity; sample++) {
    size_t index = sample_count == 1 ? 0 : (sample * (object->polygon->vertex_count - 1)) / (sample_count - 1);
    HMM_Vec3 world = transform_emissive_point_3d(object, object->polygon->vertices[index]);
    VulkanGPULight *light = &destination[written];

    light->direction[0] = 0.0f;
    light->direction[1] = 0.0f;
    light->direction[2] = -1.0f;
    light->direction[3] = 0.0f;
    light->position[0] = world.x;
    light->position[1] = world.y;
    light->position[2] = world.z;
    light->position[3] = 1.0f;
    light->color[0] = material->emission_color[0] * temperature.x;
    light->color[1] = material->emission_color[1] * temperature.y;
    light->color[2] = material->emission_color[2] * temperature.z;
    light->color[3] = 1.0f;
    light->parameters[0] = sample_strength;
    light->parameters[1] = 0.0f;
    light->parameters[2] = material->specular_factor;
    light->parameters[3] = (float)object->entity_id;
    light->cone[0] = range;
    light->cone[1] = 0.0f;
    light->cone[2] = 0.0f;
    light->cone[3] = (float)BLB_LIGHT_POINT;
    written++;
  }

  return written;
}

static HMM_Vec2 transform_emissive_point_2d(const BLB_Object2D *object, HMM_Vec2 local) {
  float angle = HMM_AngleDeg(object->rotation);
  float c = cosf(angle);
  float s = sinf(angle);
  float x = local.x * object->scale.x;
  float y = local.y * object->scale.y;
  return HMM_V2(object->position.x + x * c - y * s, object->position.y + x * s + y * c);
}

static size_t write_emissive_lights_2d(VulkanGPULight *destination, size_t capacity, BLB_Object2D *object) {
  if (!destination || !object || capacity == 0 || !object->visible || !object->material || !object->polygon || object->polygon->vertex_count == 0)
    return 0;

  BLB_Material *material = object->material;
  float total_strength = fmaxf(material->emission_strength, 0.0f) * 4.0f + fminf(fmaxf(material->glow_strength, 0.0f) * 0.02f, 1.5f);

  if (total_strength <= 0.0001f)
    return 0;

  HMM_Vec3 temperature = emission_temperature_color(material->temperature);
  size_t sample_count = object->polygon->vertex_count < 6 ? object->polygon->vertex_count : 6;
  float sample_strength = total_strength / (float)sample_count;
  float range = fmaxf(material->glow_radius, 6.0f + sqrtf(total_strength) * 4.0f);
  size_t written = 0;

  for (size_t sample = 0; sample < sample_count && written < capacity; sample++) {
    size_t index = sample_count == 1 ? 0 : (sample * (object->polygon->vertex_count - 1)) / (sample_count - 1);
    HMM_Vec2 world = transform_emissive_point_2d(object, object->polygon->vertices[index]);
    VulkanGPULight *light = &destination[written];

    light->direction[0] = 1.0f;
    light->direction[1] = 0.0f;
    light->direction[2] = 0.0f;
    light->direction[3] = 0.0f;
    light->position[0] = world.x;
    light->position[1] = world.y;
    light->position[2] = 0.0f;
    light->position[3] = 1.0f;
    light->color[0] = material->emission_color[0] * temperature.x;
    light->color[1] = material->emission_color[1] * temperature.y;
    light->color[2] = material->emission_color[2] * temperature.z;
    light->color[3] = 1.0f;
    light->parameters[0] = sample_strength;
    light->parameters[1] = 0.0f;
    light->parameters[2] = material->specular_factor;
    light->parameters[3] = (float)object->entity_id;
    light->cone[0] = range;
    light->cone[1] = 0.0f;
    light->cone[2] = 0.0f;
    light->cone[3] = (float)BLB_LIGHT_POINT;
    written++;
  }

  return written;
}

static void write_light_2d(VulkanGPULight *destination, BLB_Light2D *source) {
  if (!destination || !source || !source->object)
    return;

  HMM_Vec2 direction = BLB_GetLightDirection2D(source);

  destination->direction[0] = direction.x;
  destination->direction[1] = direction.y;
  destination->direction[2] = 0.0f;
  destination->direction[3] = 0.0f;

  destination->position[0] = source->object->position.x;

  destination->position[1] = source->object->position.y;

  destination->position[2] = 0.0f;
  destination->position[3] = 1.0f;

  destination->color[0] = source->object->color[0] / 255.0f;

  destination->color[1] = source->object->color[1] / 255.0f;

  destination->color[2] = source->object->color[2] / 255.0f;

  destination->color[3] = source->object->color[3] / 255.0f;

  destination->parameters[0] = source->intensity;

  destination->parameters[1] = source->ambient;

  destination->parameters[2] = source->specular;

  destination->parameters[3] = 0.0f;

  destination->cone[0] = source->range;

  destination->cone[1] = source->inner_cone;

  destination->cone[2] = source->outer_cone;

  destination->cone[3] = (float)source->type;
}

void update_light_buffer_3d(VULKAN *vulkan) {
  if (!vulkan)
    return;

  if (vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;

  VULKAN_Buffer *source_buffer = &vulkan->light_buffers_3d[vulkan->current_frame];

  if (!source_buffer->mapped)
    return;

  VulkanLightBufferData *buffer = (VulkanLightBufferData *)source_buffer->mapped;

  memset(buffer, 0, sizeof(*buffer));

  buffer->camera_position[0] = vulkan->camera_position.x;

  buffer->camera_position[1] = vulkan->camera_position.y;

  buffer->camera_position[2] = vulkan->camera_position.z;

  buffer->camera_position[3] = 1.0f;

  memcpy(buffer->shadow_mvp, &vulkan->shadow_mvp[0].Elements[0][0], sizeof(buffer->shadow_mvp));

  size_t count = vulkan->light3d_count;

  if (count > VULKAN_MAX_LIGHTS)
    count = VULKAN_MAX_LIGHTS;

  int shadow_light_index = -1;

  for (size_t i = 0; i < count; i++) {

    BLB_Light3D *source = vulkan->lights3d[i];

    if (!source || !source->enabled || !source->object)
      continue;

    if (buffer->light_count >= VULKAN_MAX_LIGHTS)
      break;

    size_t destination_index = buffer->light_count;
    write_light_3d(&buffer->lights[destination_index], source);

    if (vulkan->shadow_mode == 2u && source == vulkan->shadow_light3d)
      shadow_light_index = (int)destination_index;

    buffer->light_count++;
  }

  for (size_t i = 0; i < vulkan->lighting_object3d_count && buffer->light_count < VULKAN_MAX_LIGHTS; i++) {
    BLB_Object3D *object = vulkan->lighting_objects3d[i];

    if (!object || !object->visible || !object->material)
      continue;

    if (object->material->emission_strength <= 0.0001f && object->material->glow_strength <= 0.0001f)
      continue;

    size_t capacity = VULKAN_MAX_LIGHTS - buffer->light_count;
    size_t written = write_emissive_lights_3d(&buffer->lights[buffer->light_count], capacity, object);
    buffer->light_count += written;
  }

  if (vulkan->shadow_enabled) {
    buffer->shadow_params[0] = (float)vulkan->shadow_mode;
    buffer->shadow_params[1] = vulkan->shadow_bias;
    buffer->shadow_params[2] = -1.0f;
    buffer->shadow_params[3] = 0.0f;

    if (vulkan->shadow_mode == 2u && shadow_light_index >= 0)
      buffer->shadow_params[2] = (float)shadow_light_index;
  } else {
    vulkan->shadow_mode = 0;
    buffer->shadow_params[0] = 0.0f;
    buffer->shadow_params[1] = vulkan->shadow_bias;
    buffer->shadow_params[2] = -1.0f;
    buffer->shadow_params[3] = 0.0f;
  }
}

void update_light_buffer_2d(VULKAN *vulkan) {
  if (!vulkan)
    return;

  if (vulkan->current_frame >= VULKAN_MAX_FRAMES_IN_FLIGHT)
    return;

  VULKAN_Buffer *source_buffer = &vulkan->light_buffers_2d[vulkan->current_frame];

  if (!source_buffer->mapped)
    return;

  VulkanLightBufferData *buffer = (VulkanLightBufferData *)source_buffer->mapped;

  memset(buffer, 0, sizeof(*buffer));

  buffer->camera_position[0] = vulkan->camera_position.x;

  buffer->camera_position[1] = vulkan->camera_position.y;

  buffer->camera_position[2] = vulkan->camera_position.z;

  buffer->camera_position[3] = 1.0f;

  size_t count = vulkan->light2d_count;

  if (count > VULKAN_MAX_LIGHTS)
    count = VULKAN_MAX_LIGHTS;

  for (size_t i = 0; i < count; i++) {

    BLB_Light2D *source = vulkan->lights2d[i];

    if (!source || !source->enabled || !source->object)
      continue;

    if (buffer->light_count >= VULKAN_MAX_LIGHTS)
      break;

    write_light_2d(&buffer->lights[buffer->light_count], source);

    buffer->light_count++;
  }

  for (size_t i = 0; i < vulkan->lighting_object2d_count && buffer->light_count < VULKAN_MAX_LIGHTS; i++) {
    BLB_Object2D *object = vulkan->lighting_objects2d[i];

    if (!object || !object->visible || !object->material)
      continue;

    if (object->material->emission_strength <= 0.0001f && object->material->glow_strength <= 0.0001f)
      continue;

    size_t capacity = VULKAN_MAX_LIGHTS - buffer->light_count;
    size_t written = write_emissive_lights_2d(&buffer->lights[buffer->light_count], capacity, object);
    buffer->light_count += written;
  }
}

int create_light_descriptor_layout(VULKAN *vulkan, VkDescriptorSetLayout *layout) {
  if (!vulkan || !layout)
    return -1;

  VkDescriptorSetLayoutBinding bindings[8] = {
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
      {
          .binding = 2,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 3,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 4,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 5,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 6,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
      {
          .binding = 7,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
      },
  };

  VkDescriptorSetLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 8,
      .pBindings = bindings,
  };

  return vkCreateDescriptorSetLayout(vulkan->device, &info, NULL, layout) == VK_SUCCESS ? 0 : -1;
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

  VkDescriptorPoolSize pool_sizes[2] = {
      {
          .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = VULKAN_MAX_FRAMES_IN_FLIGHT * VULKAN_SHADOW_MAP_COUNT,
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

  for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++)
    layouts[i] = layout;

  VkDescriptorSetAllocateInfo allocate = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = *pool,
      .descriptorSetCount = VULKAN_MAX_FRAMES_IN_FLIGHT,
      .pSetLayouts = layouts,
  };

  if (vkAllocateDescriptorSets(vulkan->device, &allocate, sets) != VK_SUCCESS) {

    vkDestroyDescriptorPool(vulkan->device, *pool, NULL);

    *pool = VK_NULL_HANDLE;

    return -1;
  }

  for (uint32_t frame = 0; frame < VULKAN_MAX_FRAMES_IN_FLIGHT; frame++) {

    VkDescriptorBufferInfo buffer_info = {
        .buffer = buffers[frame].buffer,
        .offset = 0,
        .range = sizeof(VulkanLightBufferData),
    };

    VkDescriptorImageInfo images[VULKAN_SHADOW_MAP_COUNT];

    for (uint32_t shadow = 0; shadow < VULKAN_SHADOW_MAP_COUNT; shadow++) {

      images[shadow] = (VkDescriptorImageInfo){
          .sampler = vulkan->shadow_sampler,
          .imageView = vulkan->shadow_image_views[frame][shadow],
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      };
    }

    VkWriteDescriptorSet writes[8];

    memset(writes, 0, sizeof(writes));

    writes[0] = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = sets[frame],
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_info,
    };

    for (uint32_t shadow = 0; shadow < VULKAN_SHADOW_MAP_COUNT; shadow++) {

      uint32_t binding = shadow + 1;

      writes[binding] = (VkWriteDescriptorSet){
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = sets[frame],
          .dstBinding = binding,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &images[shadow],
      };
    }

    vkUpdateDescriptorSets(vulkan->device, 8, writes, 0, NULL);
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

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lights3d[i] = NULL;

  for (size_t i = 0; i < light_count; i++)
    vulkan->lights3d[i] = lights ? lights[i] : NULL;

  update_light_buffer_3d(vulkan);
}

void VULKAN_RendererSetLightingObjects3D(VULKAN *vulkan, BLB_Object3D **objects, size_t object_count) {
  if (!vulkan)
    return;

  if (object_count > VULKAN_MAX_LIGHTS)
    object_count = VULKAN_MAX_LIGHTS;

  vulkan->lighting_object3d_count = object_count;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lighting_objects3d[i] = NULL;

  for (size_t i = 0; i < object_count; i++)
    vulkan->lighting_objects3d[i] = objects ? objects[i] : NULL;

  update_light_buffer_3d(vulkan);
}

void VULKAN_RendererClearLights3D(VULKAN *vulkan) {
  if (!vulkan)
    return;

  vulkan->light3d_count = 0;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lights3d[i] = NULL;

  vulkan->lighting_object3d_count = 0;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lighting_objects3d[i] = NULL;

  update_light_buffer_3d(vulkan);
}

void VULKAN_RendererSetLights2D(VULKAN *vulkan, BLB_Light2D **lights, size_t light_count) {
  if (!vulkan)
    return;

  if (light_count > VULKAN_MAX_LIGHTS)
    light_count = VULKAN_MAX_LIGHTS;

  vulkan->light2d_count = light_count;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lights2d[i] = NULL;

  for (size_t i = 0; i < light_count; i++)
    vulkan->lights2d[i] = lights ? lights[i] : NULL;

  update_light_buffer_2d(vulkan);
}

void VULKAN_RendererSetLightingObjects2D(VULKAN *vulkan, BLB_Object2D **objects, size_t object_count) {
  if (!vulkan)
    return;

  if (object_count > VULKAN_MAX_LIGHTS)
    object_count = VULKAN_MAX_LIGHTS;

  vulkan->lighting_object2d_count = object_count;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lighting_objects2d[i] = NULL;

  for (size_t i = 0; i < object_count; i++)
    vulkan->lighting_objects2d[i] = objects ? objects[i] : NULL;

  update_light_buffer_2d(vulkan);
}

void VULKAN_RendererClearLights2D(VULKAN *vulkan) {
  if (!vulkan)
    return;

  vulkan->light2d_count = 0;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lights2d[i] = NULL;

  vulkan->lighting_object2d_count = 0;

  for (size_t i = 0; i < VULKAN_MAX_LIGHTS; i++)
    vulkan->lighting_objects2d[i] = NULL;

  update_light_buffer_2d(vulkan);
}

