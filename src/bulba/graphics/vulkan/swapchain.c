#include "bulba/graphics/vulkan/swapchain.h"

#include <limits.h>
#include <stdlib.h>

static int choose_format_count(VkSurfaceFormatKHR *formats, uint32_t count, VkSurfaceFormatKHR *out) {

  if (!formats || count == 0 || !out)
    return -1;

  for (uint32_t i = 0; i < count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      *out = formats[i];
      return 0;
    }
  }

  *out = formats[0];
  return 0;
}

static VkPresentModeKHR present_mode(VULKAN *v, VkPresentModeKHR *list, uint32_t n, bool vsync) {

  (void)v;

  if (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;

  for (uint32_t i = 0; i < n; i++) {
    if (list[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D extent(VULKAN *v, const VkSurfaceCapabilitiesKHR *c) {

  if (c->currentExtent.width != UINT32_MAX)
    return c->currentExtent;

  int w = 1;
  int h = 1;

  glfwGetFramebufferSize(v->window, &w, &h);

  VkExtent2D e = {
      (uint32_t)w,
      (uint32_t)h,
  };

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

static void destroy_swapchain_partial(VULKAN *vulkan) {
  if (!vulkan || !vulkan->device)
    return;

  for (uint32_t i = 0; i < vulkan->swapchain_image_count; i++) {
    if (vulkan->swapchain_image_views && vulkan->swapchain_image_views[i]) {

      vkDestroyImageView(vulkan->device, vulkan->swapchain_image_views[i], NULL);

      vulkan->swapchain_image_views[i] = VK_NULL_HANDLE;
    }
  }

  free(vulkan->swapchain_image_views);
  free(vulkan->swapchain_images);

  vulkan->swapchain_image_views = NULL;
  vulkan->swapchain_images = NULL;
  vulkan->swapchain_image_count = 0;

  if (vulkan->swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(vulkan->device, vulkan->swapchain, NULL);

    vulkan->swapchain = VK_NULL_HANDLE;
  }
}

int VULKAN_CreateSwapchain(VULKAN *v, bool vsync) {
  if (!v || !v->device || !v->surface)
    return -1;

  VkSurfaceCapabilitiesKHR c;

  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(v->physical_device, v->surface, &c) != VK_SUCCESS)
    return -1;

  uint32_t nf = 0;

  if (vkGetPhysicalDeviceSurfaceFormatsKHR(v->physical_device, v->surface, &nf, NULL) != VK_SUCCESS || nf == 0)
    return -1;

  VkSurfaceFormatKHR *fs = malloc(sizeof(*fs) * nf);

  if (!fs)
    return -1;

  if (vkGetPhysicalDeviceSurfaceFormatsKHR(v->physical_device, v->surface, &nf, fs) != VK_SUCCESS) {

    free(fs);
    return -1;
  }

  VkSurfaceFormatKHR sf;

  if (choose_format_count(fs, nf, &sf) != 0) {
    free(fs);
    return -1;
  }

  free(fs);

  uint32_t np = 0;

  if (vkGetPhysicalDeviceSurfacePresentModesKHR(v->physical_device, v->surface, &np, NULL) != VK_SUCCESS || np == 0)
    return -1;

  VkPresentModeKHR *ps = malloc(sizeof(*ps) * np);

  if (!ps)
    return -1;

  if (vkGetPhysicalDeviceSurfacePresentModesKHR(v->physical_device, v->surface, &np, ps) != VK_SUCCESS) {

    free(ps);
    return -1;
  }

  VkPresentModeKHR pm = present_mode(v, ps, np, vsync);

  free(ps);

  VkExtent2D ex = extent(v, &c);

  uint32_t count = c.minImageCount + 1;

  if (c.maxImageCount && count > c.maxImageCount)
    count = c.maxImageCount;

  VkSwapchainCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
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
      .clipped = VK_TRUE,
  };

  if (vkCreateSwapchainKHR(v->device, &info, NULL, &v->swapchain) != VK_SUCCESS)
    return -1;

  v->swapchain_format = sf.format;
  v->swapchain_extent = ex;

  uint32_t image_count = 0;

  if (vkGetSwapchainImagesKHR(v->device, v->swapchain, &image_count, NULL) != VK_SUCCESS || image_count == 0) {

    destroy_swapchain_partial(v);
    return -1;
  }

  v->swapchain_image_count = image_count;

  v->swapchain_images = calloc(image_count, sizeof(VkImage));

  v->swapchain_image_views = calloc(image_count, sizeof(VkImageView));

  if (!v->swapchain_images || !v->swapchain_image_views) {

    destroy_swapchain_partial(v);
    return -1;
  }

  if (vkGetSwapchainImagesKHR(v->device, v->swapchain, &image_count, v->swapchain_images) != VK_SUCCESS) {

    destroy_swapchain_partial(v);
    return -1;
  }

  v->swapchain_image_count = image_count;

  for (uint32_t i = 0; i < image_count; i++) {
    VkImageViewCreateInfo view = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = v->swapchain_images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = v->swapchain_format,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    if (vkCreateImageView(v->device, &view, NULL, &v->swapchain_image_views[i]) != VK_SUCCESS) {

      destroy_swapchain_partial(v);
      return -1;
    }
  }

  return 0;
}

void VULKAN_DestroySwapchain(VULKAN *v) {
  if (!v || v->device == VK_NULL_HANDLE)
    return;

  for (uint32_t i = 0; i < v->swapchain_image_count; i++) {
    if (v->swapchain_image_views && v->swapchain_image_views[i] != VK_NULL_HANDLE) {

      vkDestroyImageView(v->device, v->swapchain_image_views[i], NULL);
    }
  }

  free(v->swapchain_image_views);
  free(v->swapchain_images);

  v->swapchain_image_views = NULL;
  v->swapchain_images = NULL;
  v->swapchain_image_count = 0;

  if (v->swapchain != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(v->device, v->swapchain, NULL);

    v->swapchain = VK_NULL_HANDLE;
  }
}
