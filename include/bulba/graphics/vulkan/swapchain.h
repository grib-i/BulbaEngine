#ifndef BULBA_GRAPHICS_VULKAN_SWAPCHAIN_H
#define BULBA_GRAPHICS_VULKAN_SWAPCHAIN_H

#include "bulba/graphics/vulkan/vulkan.h"

int VULKAN_CreateSwapchain(VULKAN *vulkan, bool vsync);

void VULKAN_DestroySwapchain(VULKAN *vulkan);

#endif
