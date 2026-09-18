#ifndef BULBA_GRAPHICS_VULKAN_H
#define BULBA_GRAPHICS_VULKAN_H

#define GLFW_INCLUDE_VULKAN

#include "bulba/core/math3v/lights.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/text2d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/scene.h"
#include "bulba/core/utils/font.h"
#include "bulba/core/window.h"

#include <GLFW/glfw3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VULKAN_MAX_FRAMES_IN_FLIGHT 2
#define VULKAN_MAX_VERTICES 65536
#define VULKAN_MAX_INDICES 131072
#define VULKAN_MAX_TEXT_VERTICES 65536
#define VULKAN_MAX_LIGHTS 64
#define VULKAN_MAX_TEXTURES 1024
#define VULKAN_SHADOW_MAP_SIZE 2048

typedef struct VULKAN_Buffer {
  VkBuffer buffer;
  VkDeviceMemory memory;
  void *mapped;
} VULKAN_Buffer;

typedef struct VULKAN {
  GLFWwindow *window;

  VkInstance instance;
  VkSurfaceKHR surface;

  VkPhysicalDevice physical_device;
  VkDevice device;

  uint32_t graphics_queue_family;

  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSwapchainKHR swapchain;
  VkFormat swapchain_format;
  VkExtent2D swapchain_extent;

  uint32_t swapchain_image_count;

  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;

  VkRenderPass render_pass;
  VkFramebuffer *framebuffers;

  VkImage *depth_images;
  VkDeviceMemory *depth_memories;
  VkImageView *depth_image_views;

  VkCommandPool command_pool;

  VkCommandBuffer command_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkSemaphore image_available[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkSemaphore render_finished[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkFence in_flight[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkFence *images_in_flight;

  uint32_t current_frame;
  uint32_t current_image;

  VkPipelineLayout pipeline_layout_3d[BLB_RENDER_MODE_COUNT];

  VkPipeline pipeline_3d[BLB_RENDER_MODE_COUNT];

  VkPipelineLayout pipeline_layout_2d[BLB_RENDER_MODE_COUNT];

  VkPipeline pipeline_2d[BLB_RENDER_MODE_COUNT];

  VkPipelineLayout text_pipeline_layout_3d[BLB_RENDER_MODE_COUNT];

  VkPipeline text_pipeline_3d[BLB_RENDER_MODE_COUNT];

  VkPipelineLayout text_pipeline_layout_2d[BLB_RENDER_MODE_COUNT];

  VkPipeline text_pipeline_2d[BLB_RENDER_MODE_COUNT];

  VkDescriptorSetLayout light_descriptor_set_layout_3d;
  VkDescriptorSetLayout light_descriptor_set_layout_2d;

  VkDescriptorPool light_descriptor_pool_3d;
  VkDescriptorPool light_descriptor_pool_2d;

  VkDescriptorSet light_descriptor_sets_3d[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkDescriptorSet light_descriptor_sets_2d[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VULKAN_Buffer light_buffers_3d[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VULKAN_Buffer light_buffers_2d[VULKAN_MAX_FRAMES_IN_FLIGHT];

  BLB_Light3D *lights3d[VULKAN_MAX_LIGHTS];

  BLB_Light2D *lights2d[VULKAN_MAX_LIGHTS];

  size_t light3d_count;
  size_t light2d_count;

  HMM_Vec3 camera_position;

  float clear_color[4];

  VkDescriptorSetLayout texture_descriptor_set_layout;
  VkDescriptorPool texture_descriptor_pool;

  BLB_Texture *default_texture;

  VULKAN_Buffer vertex_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VULKAN_Buffer index_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VULKAN_Buffer text_vertex_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VULKAN_Buffer shadow_vertex_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

  size_t vertex_cursor;
  size_t index_cursor;
  size_t text_vertex_cursor;

  VkDescriptorSetLayout text_descriptor_set_layout;
  VkDescriptorPool text_descriptor_pool;
  VkDescriptorSet text_descriptor_set;

  VkImage font_image;
  VkDeviceMemory font_image_memory;
  VkImageView font_image_view;
  VkSampler font_sampler;

  const Font *loaded_font;

  VkRenderPass shadow_render_pass;

  VkFramebuffer shadow_framebuffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkImage shadow_images[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkDeviceMemory shadow_memories[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkImageView shadow_image_views[VULKAN_MAX_FRAMES_IN_FLIGHT];

  VkSampler shadow_sampler;

  VkPipeline shadow_pipeline;
  VkPipelineLayout shadow_pipeline_layout;

  HMM_Mat4 shadow_mvp;

  bool shadow_enabled;
  float shadow_bias;

  bool main_render_pass_begun;

  char last_error[256];
} VULKAN;

int VULKAN_Init(VULKAN *vulkan, GLFWwindow *window);

void VULKAN_Shutdown(VULKAN *vulkan);

const char *VULKAN_GetLastError(const VULKAN *vulkan);

#endif
