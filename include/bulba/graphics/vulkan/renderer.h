#ifndef BULBA_GRAPHICS_VULKAN_RENDERER_H
#define BULBA_GRAPHICS_VULKAN_RENDERER_H

#include "bulba/graphics/vulkan/vulkan.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  float position[3];
  float color[4];
  float normal[3];
  float uv[2];
} VulkanVertex;

typedef struct {
  float position[3];
  float color[4];
  float uv[2];
} VulkanTextVertex;

typedef struct {
  float direction[4];
  float position[4];
  float color[4];
  float parameters[4];
  float cone[4];
} VulkanGPULight;

typedef struct {
  uint32_t light_count;
  uint32_t padding0;
  uint32_t padding1;
  uint32_t padding2;
  float camera_position[4];
  VulkanGPULight lights[VULKAN_MAX_LIGHTS];
  float shadow_mvp[VULKAN_SHADOW_MAP_COUNT][16];
  float shadow_params[4];
} VulkanLightBufferData;

typedef struct {
  float mvp[16];
  float model_rows[12];
  float material[4];
  float pbr[4];
  float emission[4];
  uint32_t material_ext[4];
  uint32_t surface[8];
  uint32_t meta[4];
} VulkanLightingPushConstants;

typedef struct {
  float mvp[16];
} VulkanShadowPushConstants;

typedef struct {
  float position[3];
} VulkanShadowVertex;

typedef struct {
  float viewport[4];
  float material[4];
  float pbr[4];
  float emission[4];
  uint32_t material_ext[4];
  uint32_t surface[8];
  uint32_t meta[4];
} Vulkan2DPushConstants;

typedef struct {
  float viewport[4];
  float material[4];
} VulkanTextPushConstants;

typedef struct {
  float mvp[16];
  float material[4];
} VulkanText3DPushConstants;

typedef struct {
  uint32_t packed[3];
} VulkanMaterialTextureGPU;

typedef struct {
  VulkanMaterialTextureGPU slots[VULKAN_MATERIAL_TEXTURE_SLOTS];
} VulkanMaterialGPURecord;

_Static_assert(sizeof(VulkanMaterialTextureGPU) == 12, "VulkanMaterialTextureGPU must stay 12 bytes");
_Static_assert(sizeof(VulkanMaterialGPURecord) == VULKAN_MATERIAL_TEXTURE_SLOTS * 12,
               "VulkanMaterialGPURecord layout changed; update GLSL std430 packing");

typedef struct {
  bool lighting_enabled;
  bool double_sided;
  bool unlit;
  bool depth_enabled;
  bool depth_write;
  BLB_AlphaMode alpha_mode;
  float alpha_cutoff;

  float emission;
  float glow;
  float roundness;
  float glow_radius;
  float glow_falloff;
  float metallic;
  float roughness;
  float normal_scale;
  float occlusion;
  float specular;
  float specular_color[3];
  float emission_color[4];
  float temperature;
  float entity_id;

  float ior;
  float transmission;
  float volume_thickness;
  float attenuation_color[3];
  float attenuation_distance;
  float clearcoat_factor;
  float clearcoat_roughness;
  float clearcoat_normal_scale;
  float sheen_color[3];
  float sheen_roughness;
  float iridescence_factor;
  float iridescence_ior;
  float iridescence_thickness_min;
  float iridescence_thickness_max;
  float anisotropy_strength;
  float anisotropy_rotation;
  float dispersion;

  const BLB_Material *source_material;
  BLB_Texture *base_texture_override;
  BLB_RenderMode render_mode;
} VulkanMaterial;

int VULKAN_CreateRenderer(VULKAN *vulkan);
void VULKAN_DestroyRenderer(VULKAN *vulkan);
int VULKAN_RendererBeginFrame(VULKAN *vulkan);
int VULKAN_RendererEndFrame(VULKAN *vulkan);
void VULKAN_RendererClear(VULKAN *vulkan, float r, float g, float b, float a);
void VULKAN_RendererBeginMainPass(VULKAN *vulkan);
void VULKAN_RendererSetClearColor(VULKAN *vulkan, float r, float g, float b, float a);
void VULKAN_RendererSetShadow(VULKAN *vulkan, const float *shadow_mvp, bool enabled, float bias);
void VULKAN_RendererSetPointShadow(VULKAN *vulkan, const HMM_Mat4 *shadow_mvp, bool enabled, float bias);
void VULKAN_RendererBeginShadowPass(VULKAN *vulkan, uint32_t shadow_map_index);
void VULKAN_RendererEndShadowPass(VULKAN *vulkan);
void VULKAN_RendererDrawShadowPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *model_mvp);
void VULKAN_RendererSetCameraPosition(VULKAN *vulkan, HMM_Vec3 position);
void VULKAN_RendererSetLights3D(VULKAN *vulkan, BLB_Light3D **lights, size_t light_count);
void VULKAN_RendererClearLights3D(VULKAN *vulkan);
void VULKAN_RendererSetLights2D(VULKAN *vulkan, BLB_Light2D **lights, size_t light_count);
void VULKAN_RendererClearLights2D(VULKAN *vulkan);
void VULKAN_RendererSetLightingObjects3D(VULKAN *vulkan, BLB_Object3D **objects, size_t object_count);
void VULKAN_RendererSetLightingObjects2D(VULKAN *vulkan, BLB_Object2D **objects, size_t object_count);
void VULKAN_RendererDrawTriangle(VULKAN *vulkan, HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color, HMM_Vec3 camera_position);
void VULKAN_RendererDrawTriangle2D(VULKAN *vulkan, HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color, HMM_Vec3 camera_position, int lighting_enabled);
void VULKAN_RendererDrawMesh(VULKAN *vulkan, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, HMM_Vec3 camera_position);
void VULKAN_RendererDrawPolygon3D(VULKAN *vulkan, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g, float b, float a, const VulkanMaterial *material, BLB_Texture *texture);
void VULKAN_RendererDrawPolygon2D(VULKAN *vulkan, const BLB_Polygon2D *polygon, const HMM_Vec2 *world_positions, float viewport_width, float viewport_height, float r, float g, float b, float a, const VulkanMaterial *material, BLB_Texture *texture);
int VULKAN_RendererLoadFont(VULKAN *vulkan, const Font *font);
void VULKAN_RendererUnloadFont(VULKAN *vulkan);
void VULKAN_RendererDrawText(VULKAN *vulkan, const Font *font, const char *text, float x, float y, float glyph_scale, HMM_Vec2 transform_scale, float rotation, float r, float g, float b, float a, float emission, float glow, float roundness, BLB_RenderMode render_mode);
int VULKAN_RendererRecreateSwapchain(VULKAN *vulkan, bool vsync);
uint32_t find_memory_type(VULKAN *vulkan, uint32_t type_filter, VkMemoryPropertyFlags properties);
int create_buffer(VULKAN *vulkan, VkDeviceSize size, VkBufferUsageFlags usage, VULKAN_Buffer *buffer);
void destroy_buffer(VULKAN *vulkan, VULKAN_Buffer *buffer);
int create_image(VULKAN *vulkan, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage *image, VkDeviceMemory *memory);
int create_render_pass(VULKAN *vulkan);
int create_depth_resources(VULKAN *vulkan);
void destroy_depth_resources(VULKAN *vulkan);
int create_framebuffers(VULKAN *vulkan);
int create_command_resources(VULKAN *vulkan);
int create_sync(VULKAN *vulkan);
int create_light_descriptor_layout(VULKAN *vulkan, VkDescriptorSetLayout *layout);
int create_light_buffers(VULKAN *vulkan, VULKAN_Buffer *buffers);
void destroy_light_buffers(VULKAN *vulkan, VULKAN_Buffer *buffers);
int create_light_descriptors(VULKAN *vulkan, VkDescriptorSetLayout layout, VkDescriptorPool *pool, VkDescriptorSet *sets, VULKAN_Buffer *buffers);
void destroy_light_descriptors(VULKAN *vulkan, VkDescriptorPool *pool, VkDescriptorSetLayout *layout);
int create_texture_descriptor_resources(VULKAN *vulkan);
void destroy_texture_descriptor_resources(VULKAN *vulkan);
int create_pipelines(VULKAN *vulkan);
int create_shadow_pipeline(VULKAN *vulkan);
int create_text_descriptor_layout(VULKAN *vulkan);
int create_text_buffers(VULKAN *vulkan);
void destroy_font_resources(VULKAN *vulkan);
VkShaderModule BLB_LoadShaderFromMemory(VULKAN *vulkan, const void *code, size_t size);
void push_lighting(VULKAN *vulkan, VkPipelineLayout layout, const float *mvp, const float *model_rows, const VulkanMaterial *material, int is_2d);
void update_light_buffer_3d(VULKAN *vulkan);
void update_light_buffer_2d(VULKAN *vulkan);
int VULKAN_TextureEnsureUploaded(VULKAN *vulkan, BLB_Texture *texture);
void VULKAN_RendererBindTexture(VULKAN *vulkan, VkPipelineLayout layout, BLB_Texture *texture);
void VULKAN_RendererBindMaterial(VULKAN *vulkan, VkPipelineLayout layout, const VulkanMaterial *material);
void VULKAN_RendererInvalidateMaterialCache(VULKAN *vulkan, BLB_Material *material);
void VULKAN_TextureDestroyBackend(void *backend_data);
char *read_binary(const char *path, size_t *size);
int create_shadow_resources(VULKAN *vulkan);
void destroy_shadow_resources(VULKAN *vulkan);

#endif
