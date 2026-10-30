#ifndef BULBA_GRAPHICS_RENDERER_H
#define BULBA_GRAPHICS_RENDERER_H

#include "bulba/core/math3v/lights.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/text2d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render/shader.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/utils/colors.h"
#include "bulba/core/utils/font.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLB_RENDER_POINT_SHADOW_FACES 6u

struct GLFWwindow;
typedef struct BLB_Window BLB_Window;

typedef enum {
  BLB_RENDER_API_NONE = 0,
  BLB_RENDER_API_VULKAN = 1,
} BLB_RenderAPI;

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
  BLB_Color specular_color;
  BLB_Color emission_color;
  float temperature;
  float entity_id;

  float ior;
  float transmission;
  float volume_thickness;
  BLB_Color attenuation_color;
  float attenuation_distance;
  float clearcoat_factor;
  float clearcoat_roughness;
  float clearcoat_normal_scale;
  BLB_Color sheen_color;
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
  bool particle;
  bool skybox;
  float uv_bounds_min[3];
  float uv_bounds_max[3];
} BLB_RenderMaterial;

typedef struct {
  float model[16];
  float color[4];
  uint32_t entity_id;
  uint32_t padding[3];
} BLB_RenderInstanceData;

_Static_assert(sizeof(BLB_RenderInstanceData) == 96, "BLB_RenderInstanceData layout changed; update renderer backend layout");

typedef struct BLB_Renderer BLB_Renderer;
typedef struct BLB_RenderBackendOps BLB_RenderBackendOps;

struct BLB_RenderBackendOps {
  const char *name;
  int (*create)(void **backend, struct GLFWwindow *window);
  void (*destroy)(void *backend);

  int (*begin_frame)(void *backend);
  int (*end_frame)(void *backend);
  void (*set_clear_color)(void *backend, float r, float g, float b, float a);
  void (*begin_main_pass)(void *backend);

  void (*set_camera_position)(void *backend, BLB_Vec3 position);
  void (*set_lights3d)(void *backend, BLB_Light3D **lights, size_t light_count);
  void (*set_lights2d)(void *backend, BLB_Light2D **lights, size_t light_count);
  void (*set_lighting_objects3d)(void *backend, BLB_Object3D **objects, size_t object_count);
  void (*set_lighting_objects2d)(void *backend, BLB_Object2D **objects, size_t object_count);
  void (*update_light_buffer3d)(void *backend);
  void (*update_light_buffer2d)(void *backend);

  void (*set_shadow)(void *backend, const float *shadow_mvp, bool enabled, float bias);
  void (*set_point_shadow)(void *backend, const BLB_Mat4 *shadow_mvp, bool enabled, float bias);
  void (*set_shadow_light3d)(void *backend, BLB_Light3D *light);
  uint32_t (*get_shadow_mode)(const void *backend);
  bool (*get_shadow_mvp)(const void *backend, uint32_t index, BLB_Mat4 *mvp);
  void (*begin_shadow_pass)(void *backend, uint32_t shadow_map_index);
  void (*end_shadow_pass)(void *backend);

  void (*draw_shadow_polygon3d)(void *backend, const BLB_Polygon3D *polygon, const float *model_mvp);
  void (*draw_cached_shadow_mesh)(void *backend, const Mesh *mesh, const float *model_mvp);
  void (*draw_instanced_shadow_mesh)(void *backend, const Mesh *mesh, const float *mvp, const BLB_RenderInstanceData *instances,
                                     size_t instance_count);
  void (*draw_instanced_shadow_polygon3d)(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const BLB_RenderInstanceData *instances,
                                          size_t instance_count);

  void (*draw_triangle)(void *backend, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color,
                        BLB_Vec3 camera_position);
  void (*draw_triangle2d)(void *backend, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color,
                          BLB_Vec3 camera_position, int lighting_enabled);
  void (*draw_mesh)(void *backend, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, BLB_Vec3 camera_position);
  void (*draw_cached_mesh)(void *backend, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, BLB_Vec3 camera_position);
  void (*draw_cached_mesh_material)(void *backend, const Mesh *mesh, const float *mvp, const float *model_rows, float r, float g, float b, float a,
                                    const BLB_RenderMaterial *material, BLB_Texture *texture, BLB_Vec3 camera_position);
  void (*draw_instanced_mesh)(void *backend, const Mesh *mesh, const float *mvp, const BLB_RenderMaterial *material, BLB_Texture *texture,
                              const BLB_RenderInstanceData *instances, size_t instance_count, BLB_Vec3 camera_position);
  void (*draw_instanced_polygon3d)(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const BLB_RenderMaterial *material,
                                   BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t instance_count, BLB_Vec3 camera_position);
  void (*draw_polygon3d)(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g, float b, float a,
                         const BLB_RenderMaterial *material, BLB_Texture *texture);
  void (*draw_polygon2d)(void *backend, const BLB_Polygon2D *polygon, const BLB_Vec2 *world_positions, float viewport_width, float viewport_height,
                         float r, float g, float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture);
  void (*draw_batched_polygon2d)(void *backend, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                 const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                 size_t instance_count, bool camera_attached);
  void (*draw_instanced_polygon2d)(void *backend, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                   const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                   size_t instance_count, bool camera_attached);

  int (*ensure_instance_capacity)(void *backend, size_t required_instances);
  void (*clear_cached_meshes)(void *backend);

  int (*load_font)(void *backend, const Font *font);
  void (*unload_font)(void *backend);
  void (*draw_text)(void *backend, const Font *font, const char *text, float x, float y, float glyph_scale, BLB_Vec2 transform_scale, float rotation,
                    float r, float g, float b, float a, float emission, float glow, float roundness, BLB_RenderMode render_mode);
  void (*draw_text_transformed)(void *backend, const Font *font, const char *text, float x, float y, float glyph_scale, BLB_Vec2 transform_scale,
                                float rotation, float r, float g, float b, float a, float emission, float glow, float roundness,
                                BLB_RenderMode render_mode, const float *mvp);

  int (*recreate_swapchain)(void *backend, bool vsync);
  bool (*get_vsync)(const void *backend);
  void (*get_viewport)(const void *backend, uint32_t *width, uint32_t *height);
  void (*set_view_projection)(void *backend, const float *view_projection);
};

struct BLB_Renderer {
  BLB_RenderAPI api;
  const BLB_RenderBackendOps *ops;
  void *backend;
  bool owns_backend;
  bool frame_ready;
  bool swapchain_pending;
  bool requested_vsync;
};

int BLB_Renderer_Create(BLB_Renderer *renderer, BLB_RenderAPI api, struct GLFWwindow *window);
int BLB_Renderer_Attach(BLB_Renderer *renderer, BLB_RenderAPI api, void *backend, const BLB_RenderBackendOps *ops);
void BLB_Renderer_Destroy(BLB_Renderer *renderer);
BLB_RenderAPI BLB_Renderer_GetAPI(const BLB_Renderer *renderer);
const char *BLB_Renderer_GetAPIName(const BLB_Renderer *renderer);

int BLB_RendererBeginFrame(BLB_Renderer *renderer);
int BLB_RendererEndFrame(BLB_Renderer *renderer);
int BLB_RendererPrepareFrame(BLB_Renderer *renderer, BLB_Window *window);
bool BLB_RendererIsFrameReady(const BLB_Renderer *renderer);
int BLB_RendererSetVSync(BLB_Renderer *renderer, bool vsync);
void BLB_RendererSetClearColor(BLB_Renderer *renderer, BLB_Color color);
void BLB_RendererBeginMainPass(BLB_Renderer *renderer);
void BLB_RendererSetCameraPosition(BLB_Renderer *renderer, BLB_Vec3 position);
void BLB_RendererSetLights3D(BLB_Renderer *renderer, BLB_Light3D **lights, size_t light_count);
void BLB_RendererSetLights2D(BLB_Renderer *renderer, BLB_Light2D **lights, size_t light_count);
void BLB_RendererSetLightingObjects3D(BLB_Renderer *renderer, BLB_Object3D **objects, size_t object_count);
void BLB_RendererSetLightingObjects2D(BLB_Renderer *renderer, BLB_Object2D **objects, size_t object_count);
void BLB_RendererUpdateLightBuffer3D(BLB_Renderer *renderer);
void BLB_RendererUpdateLightBuffer2D(BLB_Renderer *renderer);
void BLB_RendererSetShadow(BLB_Renderer *renderer, const float *shadow_mvp, bool enabled, float bias);
void BLB_RendererSetPointShadow(BLB_Renderer *renderer, const BLB_Mat4 *shadow_mvp, bool enabled, float bias);
void BLB_RendererSetShadowLight3D(BLB_Renderer *renderer, BLB_Light3D *light);
uint32_t BLB_RendererGetShadowMode(const BLB_Renderer *renderer);
bool BLB_RendererGetShadowMVP(const BLB_Renderer *renderer, uint32_t index, BLB_Mat4 *mvp);
void BLB_RendererBeginShadowPass(BLB_Renderer *renderer, uint32_t shadow_map_index);
void BLB_RendererEndShadowPass(BLB_Renderer *renderer);
void BLB_RendererDrawShadowPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *model_mvp);
void BLB_RendererDrawCachedShadowMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *model_mvp);
void BLB_RendererDrawInstancedShadowMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, const BLB_RenderInstanceData *instances,
                                         size_t instance_count);
void BLB_RendererDrawInstancedShadowPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *mvp,
                                              const BLB_RenderInstanceData *instances, size_t instance_count);
void BLB_RendererDrawTriangle(BLB_Renderer *renderer, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color,
                              float a_color, BLB_Vec3 camera_position);
void BLB_RendererDrawTriangle2D(BLB_Renderer *renderer, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color,
                                float a_color, BLB_Vec3 camera_position, int lighting_enabled);
void BLB_RendererDrawMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, BLB_Vec3 camera_position);
void BLB_RendererDrawCachedMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, float r, float g, float b, float a,
                                BLB_Vec3 camera_position);
void BLB_RendererDrawCachedMeshMaterial(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, const float *model_rows, float r, float g,
                                        float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture, BLB_Vec3 camera_position);
void BLB_RendererDrawInstancedMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, const BLB_RenderMaterial *material,
                                   BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t instance_count, BLB_Vec3 camera_position);
void BLB_RendererDrawInstancedPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *mvp, const BLB_RenderMaterial *material,
                                        BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t instance_count,
                                        BLB_Vec3 camera_position);
void BLB_RendererDrawPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g,
                               float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture);
void BLB_RendererDrawPolygon2D(BLB_Renderer *renderer, const BLB_Polygon2D *polygon, const BLB_Vec2 *world_positions, float viewport_width,
                               float viewport_height, float r, float g, float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture);
void BLB_RendererDrawBatchedPolygon2D(BLB_Renderer *renderer, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                      const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                      size_t instance_count, bool camera_attached);
void BLB_RendererDrawInstancedPolygon2D(BLB_Renderer *renderer, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                        const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                        size_t instance_count, bool camera_attached);
int BLB_RendererEnsureInstanceCapacity(BLB_Renderer *renderer, size_t required_instances);
void BLB_RendererClearCachedMeshes(BLB_Renderer *renderer);
int BLB_RendererLoadFont(BLB_Renderer *renderer, const Font *font);
void BLB_RendererUnloadFont(BLB_Renderer *renderer);
void BLB_RendererDrawText(BLB_Renderer *renderer, const Font *font, const char *text, float x, float y, float glyph_scale, BLB_Vec2 transform_scale,
                          float rotation, float r, float g, float b, float a, float emission, float glow, float roundness,
                          BLB_RenderMode render_mode);
void BLB_RendererDrawTextTransformed(BLB_Renderer *renderer, const Font *font, const char *text, float x, float y, float glyph_scale,
                                     BLB_Vec2 transform_scale, float rotation, float r, float g, float b, float a, float emission, float glow,
                                     float roundness, BLB_RenderMode render_mode, const float *mvp);
int BLB_RendererRecreateSwapchain(BLB_Renderer *renderer, bool vsync);
bool BLB_RendererGetVSync(const BLB_Renderer *renderer);
void BLB_RendererGetViewport(const BLB_Renderer *renderer, uint32_t *width, uint32_t *height);
void BLB_RendererSetViewProjection(BLB_Renderer *renderer, const float *view_projection);

#endif
