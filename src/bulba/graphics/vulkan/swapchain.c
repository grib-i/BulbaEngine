#include "bulba/graphics/vulkan/swapchain.h"

#include <limits.h>
#include <stdlib.h>

static uint32_t choose_format_count(VkSurfaceFormatKHR *formats, uint32_t count, VkSurfaceFormatKHR *out) {
  for (uint32_t i = 0; i < count; i++)
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      *out = formats[i];
      return 0;
    }
  *out = formats[0];
  return 0;
}

static VkPresentModeKHR present_mode(VULKAN *v, VkPresentModeKHR *list, uint32_t n, bool vsync) {
  (void)v;

  if (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;

  for (uint32_t i = 0; i < n; i++)
    if (list[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
      return VK_PRESENT_MODE_IMMEDIATE_KHR;

  return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D extent(VULKAN *v, const VkSurfaceCapabilitiesKHR *c) {
  if (c->currentExtent.width != UINT32_MAX)
    return c->currentExtent;
  int w = 1, h = 1;
  glfwGetFramebufferSize(v->window, &w, &h);
  VkExtent2D e = {(uint32_t)w, (uint32_t)h};
  if (e.width < c->minImageExtent.width)
    e.width = c->minImageExtent.width;
  if (e.width > c->maxImageExtent.width)
    e.width = c->maxImageExtent.width;
  if (e.height < c->minImageExtent.height)
    e.height = c->minImageExtent.height;
  if (e.height > c->maxImageExtent.height)
    e.height = c->maxImageExtent.height;
  return e;
}

int VULKAN_CreateSwapchain(VULKAN *v, bool vsync) {
  VkSurfaceCapabilitiesKHR c;
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(v->physical_device, v->surface, &c) != VK_SUCCESS)
    return -1;
  uint32_t nf = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(v->physical_device, v->surface, &nf, NULL) != VK_SUCCESS || !nf)
    return -1;
  VkSurfaceFormatKHR *fs = malloc(sizeof(*fs) * nf);
  if (!fs)
    return -1;
  vkGetPhysicalDeviceSurfaceFormatsKHR(v->physical_device, v->surface, &nf, fs);
  VkSurfaceFormatKHR sf;
  choose_format_count(fs, nf, &sf);
  free(fs);
  uint32_t np = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(v->physical_device, v->surface, &np, NULL);
  VkPresentModeKHR *ps = malloc(sizeof(*ps) * np);
  if (!ps)
    return -1;
  vkGetPhysicalDeviceSurfacePresentModesKHR(v->physical_device, v->surface, &np, ps);
  VkPresentModeKHR pm = present_mode(v, ps, np, vsync);
  free(ps);
  VkExtent2D ex = extent(v, &c);
  uint32_t count = c.minImageCount + 1;
  if (c.maxImageCount && count > c.maxImageCount)
    count = c.maxImageCount;
  VkSwapchainCreateInfoKHR info = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                   .surface = v->surface,
                                   .minImageCount = count,
                                   .imageFormat = sf.format,
                                   .imageColorSpace = sf.colorSpace,
                                   .imageExtent = ex,
                                   .imageArrayLayers = 1,
                                   .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                   .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                   .preTransform = c.currentTransform,
                                   .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                   .presentMode = pm,
                                   .clipped = VK_TRUE};
  if (vkCreateSwapchainKHR(v->device, &info, NULL, &v->swapchain) != VK_SUCCESS)
    return -1;
  v->swapchain_format = sf.format;
  v->swapchain_extent = ex;
  vkGetSwapchainImagesKHR(v->device, v->swapchain, &count, NULL);
  v->swapchain_image_count = count;
  v->swapchain_images = calloc(count, sizeof(VkImage));
  v->swapchain_image_views = calloc(count, sizeof(VkImageView));
  if (!v->swapchain_images || !v->swapchain_image_views)
    return -1;

  vkGetSwapchainImagesKHR(v->device, v->swapchain, &count, v->swapchain_images);
  for (uint32_t i = 0; i < count; i++) {
    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = v->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = v->swapchain_format,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
    if (vkCreateImageView(v->device, &view, NULL, &v->swapchain_image_views[i]) != VK_SUCCESS)
      return -1;
  }
  return 0;
}

void VULKAN_DestroySwapchain(VULKAN *v) {
  if (!v || v->device == VK_NULL_HANDLE)
    return;
  for (uint32_t i = 0; i < v->swapchain_image_count; i++)
    if (v->swapchain_image_views && v->swapchain_image_views[i])
      vkDestroyImageView(v->device, v->swapchain_image_views[i], NULL);
  free(v->swapchain_image_views);
  free(v->swapchain_images);
  v->swapchain_image_views = NULL;
  v->swapchain_images = NULL;
  v->swapchain_image_count = 0;
  if (v->swapchain) {
    vkDestroySwapchainKHR(v->device, v->swapchain, NULL);
    v->swapchain = VK_NULL_HANDLE;
  }
}
