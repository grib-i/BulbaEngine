#include "bulba/graphics/vulkan/vulkan.h"
#include "bulba/graphics/vulkan/renderer.h"
#include "bulba/graphics/vulkan/swapchain.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pick_device(VULKAN *v) {
  if (!v)
    return -1;

  uint32_t n = 0;

  if (vkEnumeratePhysicalDevices(v->instance, &n, NULL) != VK_SUCCESS)
    return -1;

  if (n == 0)
    return -1;

  VkPhysicalDevice *devices = malloc(sizeof(*devices) * n);

  if (!devices)
    return -1;

  if (vkEnumeratePhysicalDevices(v->instance, &n, devices) != VK_SUCCESS) {
    free(devices);
    return -1;
  }

  for (uint32_t i = 0; i < n; i++) {
    uint32_t queue_count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, NULL);

    if (queue_count == 0)
      continue;

    VkQueueFamilyProperties *queues = malloc(sizeof(*queues) * queue_count);

    if (!queues)
      continue;

    vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count, queues);

    for (uint32_t j = 0; j < queue_count; j++) {
      VkBool32 present = VK_FALSE;

      if (vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, v->surface, &present) != VK_SUCCESS)
        continue;

      if ((queues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
        v->physical_device = devices[i];
        v->graphics_queue_family = j;

        free(queues);
        free(devices);

        return 0;
      }
    }

    free(queues);
  }

  free(devices);

  return -1;
}

static int fail(VULKAN *vulkan, const char *message) {
  if (vulkan && message) {
    snprintf(vulkan->last_error, sizeof(vulkan->last_error), "%s", message);
  }

  return -1;
}

static int fail_vk(VULKAN *vulkan, const char *stage, VkResult result) {
  if (vulkan) {
    snprintf(vulkan->last_error, sizeof(vulkan->last_error), "%s failed with Vulkan error %d", stage, result);
  }

  return -1;
}

int VULKAN_Init(VULKAN *v, GLFWwindow *w) {
  if (!v || !w)
    return -1;

  memset(v, 0, sizeof(*v));

  v->window = w;

  uint32_t extension_count = 0;

  const char **extensions = glfwGetRequiredInstanceExtensions(&extension_count);

  if (!extensions || extension_count == 0)
    return fail(v, "GLFW did not provide Vulkan instance extensions");

  VkApplicationInfo app_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "BulbaEngine",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "BulbaEngine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo instance_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app_info,
      .enabledExtensionCount = extension_count,
      .ppEnabledExtensionNames = extensions,
  };

  VkResult result = vkCreateInstance(&instance_info, NULL, &v->instance);

  if (result != VK_SUCCESS)
    return fail_vk(v, "vkCreateInstance", result);

  result = glfwCreateWindowSurface(v->instance, w, NULL, &v->surface);

  if (result != VK_SUCCESS) {
    fail_vk(v, "glfwCreateWindowSurface", result);
    VULKAN_Shutdown(v);
    return -1;
  }

  if (pick_device(v) != 0) {
    fail(v, "No Vulkan physical device with graphics and present support was found");
    VULKAN_Shutdown(v);
    return -1;
  }

  float priority = 1.0f;

  VkDeviceQueueCreateInfo queue_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = v->graphics_queue_family,
      .queueCount = 1,
      .pQueuePriorities = &priority,
  };

  const char *device_extensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  VkPhysicalDeviceFeatures features = {0};

  VkDeviceCreateInfo device_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_info,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = device_extensions,
      .pEnabledFeatures = &features,
  };

  result = vkCreateDevice(v->physical_device, &device_info, NULL, &v->device);

  if (result != VK_SUCCESS) {
    fail_vk(v, "vkCreateDevice", result);
    VULKAN_Shutdown(v);
    return -1;
  }

  vkGetDeviceQueue(v->device, v->graphics_queue_family, 0, &v->graphics_queue);

  v->present_queue = v->graphics_queue;

  if (VULKAN_CreateSwapchain(v, false) != 0) {
    fail(v, "VULKAN_CreateSwapchain failed");
    VULKAN_Shutdown(v);
    return -1;
  }

  v->current_frame = 0;

  return 0;
}

void VULKAN_Shutdown(VULKAN *v) {
  if (!v)
    return;

  if (v->device != VK_NULL_HANDLE)
    vkDeviceWaitIdle(v->device);

  if (v->device != VK_NULL_HANDLE)
    VULKAN_DestroyRenderer(v);

  VULKAN_DestroySwapchain(v);

  if (v->device != VK_NULL_HANDLE) {
    vkDestroyDevice(v->device, NULL);
    v->device = VK_NULL_HANDLE;
  }

  if (v->surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(v->instance, v->surface, NULL);

    v->surface = VK_NULL_HANDLE;
  }

  if (v->instance != VK_NULL_HANDLE) {
    vkDestroyInstance(v->instance, NULL);
    v->instance = VK_NULL_HANDLE;
  }

  v->physical_device = VK_NULL_HANDLE;
  v->graphics_queue = VK_NULL_HANDLE;
  v->present_queue = VK_NULL_HANDLE;
  v->graphics_queue_family = 0;
  v->current_frame = 0;
  v->current_image = 0;
}

const char *VULKAN_GetLastError(const VULKAN *vulkan) {
  if (!vulkan || vulkan->last_error[0] == '\0')
    return "No Vulkan error information available";

  return vulkan->last_error;
}

