#include "bulba/graphics/vulkan/vulkan.h"
#include "bulba/graphics/vulkan/renderer.h"
#include "bulba/graphics/vulkan/swapchain.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int pick_device(VULKAN *v) {
  uint32_t n = 0;
  vkEnumeratePhysicalDevices(v->instance, &n, NULL);
  if (!n)
    return -1;
  VkPhysicalDevice *ds = malloc(sizeof(*ds) * n);
  if (!ds)
    return -1;
  vkEnumeratePhysicalDevices(v->instance, &n, ds);
  for (uint32_t i = 0; i < n; i++) {
    uint32_t qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ds[i], &qn, NULL);
    VkQueueFamilyProperties *q = malloc(sizeof(*q) * qn);
    vkGetPhysicalDeviceQueueFamilyProperties(ds[i], &qn, q);
    for (uint32_t j = 0; j < qn; j++) {
      VkBool32 present = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(ds[i], j, v->surface, &present);
      if ((q[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
        v->physical_device = ds[i];
        v->graphics_queue_family = j;
        free(q);
        free(ds);
        return 0;
      }
    }
    free(q);
  }
  free(ds);
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

  uint32_t count = 0;
  const char **ext = glfwGetRequiredInstanceExtensions(&count);
  if (!ext || count == 0)
    return fail(v, "GLFW did not provide Vulkan instance extensions");

  VkApplicationInfo ai = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                          .pApplicationName = "BulbaEngine",
                          .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                          .pEngineName = "BulbaEngine",
                          .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                          .apiVersion = VK_API_VERSION_1_0};

  VkInstanceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &ai, .enabledExtensionCount = count, .ppEnabledExtensionNames = ext};

  VkResult result = vkCreateInstance(&ci, NULL, &v->instance);
  if (result != VK_SUCCESS)
    return fail_vk(v, "vkCreateInstance", result);

  result = glfwCreateWindowSurface(v->instance, w, NULL, &v->surface);
  if (result != VK_SUCCESS)
    return fail_vk(v, "glfwCreateWindowSurface", result);

  if (pick_device(v) != 0)
    return fail(v, "No Vulkan physical device with graphics and present support was found");

  float priority = 1.0f;
  VkDeviceQueueCreateInfo q = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                               .queueFamilyIndex = v->graphics_queue_family,
                               .queueCount = 1,
                               .pQueuePriorities = &priority};

  const char *devext[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkPhysicalDeviceFeatures feat = {0};
  VkDeviceCreateInfo di = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                           .queueCreateInfoCount = 1,
                           .pQueueCreateInfos = &q,
                           .enabledExtensionCount = 1,
                           .ppEnabledExtensionNames = devext,
                           .pEnabledFeatures = &feat};

  result = vkCreateDevice(v->physical_device, &di, NULL, &v->device);
  if (result != VK_SUCCESS)
    return fail_vk(v, "vkCreateDevice", result);

  vkGetDeviceQueue(v->device, v->graphics_queue_family, 0, &v->graphics_queue);
  v->present_queue = v->graphics_queue;

  if (VULKAN_CreateSwapchain(v, false) != 0)
    return fail(v, "VULKAN_CreateSwapchain failed");

  v->current_frame = 0;
  return 0;
}

void VULKAN_Shutdown(VULKAN *v) {
  if (!v)
    return;

  if (v->device)
    vkDeviceWaitIdle(v->device);

  if (v->device)
    VULKAN_DestroyRenderer(v);

  VULKAN_DestroySwapchain(v);

  if (v->device) {
    vkDestroyDevice(v->device, NULL);
    v->device = VK_NULL_HANDLE;
  }

  if (v->surface) {
    vkDestroySurfaceKHR(v->instance, v->surface, NULL);
    v->surface = VK_NULL_HANDLE;
  }

  if (v->instance) {
    vkDestroyInstance(v->instance, NULL);
    v->instance = VK_NULL_HANDLE;
  }
}

const char *VULKAN_GetLastError(const VULKAN *vulkan) {
  if (!vulkan || vulkan->last_error[0] == '\0')
    return "No Vulkan error information available";
  return vulkan->last_error;
}
