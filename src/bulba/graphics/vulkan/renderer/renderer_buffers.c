#include "bulba/graphics/vulkan/renderer.h"

uint32_t find_memory_type(VULKAN *vulkan, uint32_t type_filter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memory;

  vkGetPhysicalDeviceMemoryProperties(vulkan->physical_device, &memory);

  for (uint32_t i = 0; i < memory.memoryTypeCount; i++) {
    if ((type_filter & (1u << i)) && (memory.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  return UINT32_MAX;
}


int create_buffer(VULKAN *vulkan, VkDeviceSize size, VkBufferUsageFlags usage, VULKAN_Buffer *buffer) {
  VkBufferCreateInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

  if (vkCreateBuffer(vulkan->device, &info, NULL, &buffer->buffer) != VK_SUCCESS)
    return -1;

  VkMemoryRequirements requirements;
  vkGetBufferMemoryRequirements(vulkan->device, buffer->buffer, &requirements);

  uint32_t memory_type =
      find_memory_type(vulkan, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (memory_type == UINT32_MAX) {
    vkDestroyBuffer(vulkan->device, buffer->buffer, NULL);
    buffer->buffer = VK_NULL_HANDLE;
    return -1;
  }

  VkMemoryAllocateInfo alloc = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = requirements.size, .memoryTypeIndex = memory_type};

  if (vkAllocateMemory(vulkan->device, &alloc, NULL, &buffer->memory) != VK_SUCCESS) {
    vkDestroyBuffer(vulkan->device, buffer->buffer, NULL);
    buffer->buffer = VK_NULL_HANDLE;
    return -1;
  }

  if (vkBindBufferMemory(vulkan->device, buffer->buffer, buffer->memory, 0) != VK_SUCCESS) {
    vkFreeMemory(vulkan->device, buffer->memory, NULL);
    vkDestroyBuffer(vulkan->device, buffer->buffer, NULL);

    buffer->memory = VK_NULL_HANDLE;
    buffer->buffer = VK_NULL_HANDLE;

    return -1;
  }

  if (vkMapMemory(vulkan->device, buffer->memory, 0, VK_WHOLE_SIZE, 0, &buffer->mapped) != VK_SUCCESS) {
    vkFreeMemory(vulkan->device, buffer->memory, NULL);
    vkDestroyBuffer(vulkan->device, buffer->buffer, NULL);

    buffer->memory = VK_NULL_HANDLE;
    buffer->buffer = VK_NULL_HANDLE;

    return -1;
  }

  return 0;
}


void destroy_buffer(VULKAN *vulkan, VULKAN_Buffer *buffer) {
  if (buffer == NULL)
    return;

  if (buffer->mapped != NULL) {
    vkUnmapMemory(vulkan->device, buffer->memory);
    buffer->mapped = NULL;
  }

  if (buffer->buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(vulkan->device, buffer->buffer, NULL);
    buffer->buffer = VK_NULL_HANDLE;
  }

  if (buffer->memory != VK_NULL_HANDLE) {
    vkFreeMemory(vulkan->device, buffer->memory, NULL);
    buffer->memory = VK_NULL_HANDLE;
  }
}


int create_image(VULKAN *vulkan, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage *image,
                        VkDeviceMemory *memory) {
  VkImageCreateInfo info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                            .imageType = VK_IMAGE_TYPE_2D,
                            .format = format,
                            .extent = {width, height, 1},
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .samples = VK_SAMPLE_COUNT_1_BIT,
                            .tiling = VK_IMAGE_TILING_OPTIMAL,
                            .usage = usage,
                            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};

  if (vkCreateImage(vulkan->device, &info, NULL, image) != VK_SUCCESS)
    return -1;

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(vulkan->device, *image, &requirements);

  uint32_t memory_type = find_memory_type(vulkan, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  if (memory_type == UINT32_MAX) {
    vkDestroyImage(vulkan->device, *image, NULL);
    *image = VK_NULL_HANDLE;
    return -1;
  }

  VkMemoryAllocateInfo alloc = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = requirements.size, .memoryTypeIndex = memory_type};

  if (vkAllocateMemory(vulkan->device, &alloc, NULL, memory) != VK_SUCCESS) {
    vkDestroyImage(vulkan->device, *image, NULL);
    *image = VK_NULL_HANDLE;
    return -1;
  }

  if (vkBindImageMemory(vulkan->device, *image, *memory, 0) != VK_SUCCESS) {
    vkFreeMemory(vulkan->device, *memory, NULL);
    vkDestroyImage(vulkan->device, *image, NULL);
    *memory = VK_NULL_HANDLE;
    *image = VK_NULL_HANDLE;
    return -1;
  }

  return 0;
}


