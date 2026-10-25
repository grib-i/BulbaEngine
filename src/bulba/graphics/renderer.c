#include "bulba/graphics/renderer.h"
#include "bulba/core/window.h"
#include "bulba/graphics/vulkan/renderer_backend.h"

#include <string.h>

static const BLB_RenderBackendOps *backend_ops(BLB_RenderAPI api) {
  switch (api) {
  case BLB_RENDER_API_VULKAN:
    return BLB_VulkanRendererBackendOps();
  default:
    return NULL;
  }
}

int BLB_Renderer_Create(BLB_Renderer *renderer, BLB_RenderAPI api, struct GLFWwindow *window) {
  if (!renderer)
    return -1;

  memset(renderer, 0, sizeof(*renderer));
  const BLB_RenderBackendOps *ops = backend_ops(api);

  if (!ops || !ops->create)
    return -1;

  void *backend = NULL;

  if (ops->create(&backend, window) != 0 || !backend)
    return -1;

  renderer->api = api;
  renderer->ops = ops;
  renderer->backend = backend;
  renderer->owns_backend = true;
  renderer->requested_vsync = ops->get_vsync && ops->get_vsync(backend);

  return 0;
}

int BLB_Renderer_Attach(BLB_Renderer *renderer, BLB_RenderAPI api, void *backend, const BLB_RenderBackendOps *ops) {
  if (!renderer || !backend || !ops)
    return -1;

  memset(renderer, 0, sizeof(*renderer));

  renderer->api = api;
  renderer->ops = ops;
  renderer->backend = backend;
  renderer->owns_backend = false;
  renderer->requested_vsync = ops->get_vsync && ops->get_vsync(backend);

  return 0;
}

void BLB_Renderer_Destroy(BLB_Renderer *renderer) {
  if (!renderer)
    return;

  if (renderer->owns_backend && renderer->backend && renderer->ops && renderer->ops->destroy)
    renderer->ops->destroy(renderer->backend);

  memset(renderer, 0, sizeof(*renderer));
}

BLB_RenderAPI BLB_Renderer_GetAPI(const BLB_Renderer *renderer) { return renderer ? renderer->api : BLB_RENDER_API_NONE; }

const char *BLB_Renderer_GetAPIName(const BLB_Renderer *renderer) {
  return renderer && renderer->ops && renderer->ops->name ? renderer->ops->name : "none";
}

int BLB_RendererBeginFrame(BLB_Renderer *renderer) {
  if (!renderer || !renderer->ops || !renderer->ops->begin_frame || renderer->frame_ready)
    return -1;

  int result = renderer->ops->begin_frame(renderer->backend);

  if (result == -2) {
    renderer->swapchain_pending = true;

    return 1;
  }

  if (result != 0)
    return -1;

  renderer->frame_ready = true;

  return 0;
}

int BLB_RendererEndFrame(BLB_Renderer *renderer) {
  if (!renderer || !renderer->ops || !renderer->ops->end_frame || !renderer->frame_ready)
    return -1;

  int result = renderer->ops->end_frame(renderer->backend);
  renderer->frame_ready = false;

  if (result == -2) {
    renderer->swapchain_pending = true;

    return 0;
  }

  return result == 0 ? 0 : -1;
}

int BLB_RendererPrepareFrame(BLB_Renderer *renderer, BLB_Window *window) {
  if (!renderer || !renderer->backend || !renderer->ops || !window)
    return -1;

  if (renderer->frame_ready)
    return -1;

  if (window->width <= 0 || window->height <= 0)
    return 1;

  if (window->resized || renderer->swapchain_pending) {
    if (!renderer->ops->recreate_swapchain)
      return -1;

    uint64_t resize_serial = window->resize_serial;
    int result = renderer->ops->recreate_swapchain(renderer->backend, renderer->requested_vsync);

    if (result > 0)
      return 1;

    if (result < 0)
      return -1;

    renderer->swapchain_pending = false;

    if (window->resize_serial == resize_serial)
      window->resized = false;
  }

  return BLB_RendererBeginFrame(renderer);
}

bool BLB_RendererIsFrameReady(const BLB_Renderer *renderer) { return renderer && renderer->frame_ready; }

int BLB_RendererSetVSync(BLB_Renderer *renderer, bool vsync) {
  if (!renderer || !renderer->backend || !renderer->ops || !renderer->ops->recreate_swapchain)
    return -1;

  if (renderer->requested_vsync != vsync) {
    renderer->requested_vsync = vsync;
    renderer->swapchain_pending = true;
  }

  return 0;
}

void BLB_RendererSetClearColor(BLB_Renderer *renderer, float r, float g, float b, float a) {
  if (renderer && renderer->ops && renderer->ops->set_clear_color)
    renderer->ops->set_clear_color(renderer->backend, r, g, b, a);
}

void BLB_RendererBeginMainPass(BLB_Renderer *renderer) {
  if (renderer && renderer->ops && renderer->ops->begin_main_pass)
    renderer->ops->begin_main_pass(renderer->backend);
}

void BLB_RendererSetCameraPosition(BLB_Renderer *renderer, BLB_Vec3 position) {
  if (renderer && renderer->ops && renderer->ops->set_camera_position)
    renderer->ops->set_camera_position(renderer->backend, position);
}

void BLB_RendererSetLights3D(BLB_Renderer *renderer, BLB_Light3D **lights, size_t light_count) {
  if (renderer && renderer->ops && renderer->ops->set_lights3d)
    renderer->ops->set_lights3d(renderer->backend, lights, light_count);
}

void BLB_RendererSetLights2D(BLB_Renderer *renderer, BLB_Light2D **lights, size_t light_count) {
  if (renderer && renderer->ops && renderer->ops->set_lights2d)
    renderer->ops->set_lights2d(renderer->backend, lights, light_count);
}

void BLB_RendererSetLightingObjects3D(BLB_Renderer *renderer, BLB_Object3D **objects, size_t object_count) {
  if (renderer && renderer->ops && renderer->ops->set_lighting_objects3d)
    renderer->ops->set_lighting_objects3d(renderer->backend, objects, object_count);
}

void BLB_RendererSetLightingObjects2D(BLB_Renderer *renderer, BLB_Object2D **objects, size_t object_count) {
  if (renderer && renderer->ops && renderer->ops->set_lighting_objects2d)
    renderer->ops->set_lighting_objects2d(renderer->backend, objects, object_count);
}

void BLB_RendererUpdateLightBuffer3D(BLB_Renderer *renderer) {
  if (renderer && renderer->ops && renderer->ops->update_light_buffer3d)
    renderer->ops->update_light_buffer3d(renderer->backend);
}

void BLB_RendererUpdateLightBuffer2D(BLB_Renderer *renderer) {
  if (renderer && renderer->ops && renderer->ops->update_light_buffer2d)
    renderer->ops->update_light_buffer2d(renderer->backend);
}

void BLB_RendererSetShadow(BLB_Renderer *renderer, const float *shadow_mvp, bool enabled, float bias) {
  if (renderer && renderer->ops && renderer->ops->set_shadow)
    renderer->ops->set_shadow(renderer->backend, shadow_mvp, enabled, bias);
}

void BLB_RendererSetPointShadow(BLB_Renderer *renderer, const BLB_Mat4 *shadow_mvp, bool enabled, float bias) {
  if (renderer && renderer->ops && renderer->ops->set_point_shadow)
    renderer->ops->set_point_shadow(renderer->backend, shadow_mvp, enabled, bias);
}

void BLB_RendererSetShadowLight3D(BLB_Renderer *renderer, BLB_Light3D *light) {
  if (renderer && renderer->ops && renderer->ops->set_shadow_light3d)
    renderer->ops->set_shadow_light3d(renderer->backend, light);
}

uint32_t BLB_RendererGetShadowMode(const BLB_Renderer *renderer) {
  return renderer && renderer->ops && renderer->ops->get_shadow_mode ? renderer->ops->get_shadow_mode(renderer->backend) : 0u;
}

bool BLB_RendererGetShadowMVP(const BLB_Renderer *renderer, uint32_t index, BLB_Mat4 *mvp) {
  return renderer && renderer->ops && renderer->ops->get_shadow_mvp && renderer->ops->get_shadow_mvp(renderer->backend, index, mvp);
}

void BLB_RendererBeginShadowPass(BLB_Renderer *renderer, uint32_t shadow_map_index) {
  if (renderer && renderer->ops && renderer->ops->begin_shadow_pass)
    renderer->ops->begin_shadow_pass(renderer->backend, shadow_map_index);
}

void BLB_RendererEndShadowPass(BLB_Renderer *renderer) {
  if (renderer && renderer->ops && renderer->ops->end_shadow_pass)
    renderer->ops->end_shadow_pass(renderer->backend);
}

void BLB_RendererDrawShadowPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *model_mvp) {
  if (renderer && renderer->ops && renderer->ops->draw_shadow_polygon3d)
    renderer->ops->draw_shadow_polygon3d(renderer->backend, polygon, model_mvp);
}

void BLB_RendererDrawCachedShadowMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *model_mvp) {
  if (renderer && renderer->ops && renderer->ops->draw_cached_shadow_mesh)
    renderer->ops->draw_cached_shadow_mesh(renderer->backend, mesh, model_mvp);
}

void BLB_RendererDrawInstancedShadowMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, const BLB_RenderInstanceData *instances,
                                         size_t instance_count) {
  if (renderer && renderer->ops && renderer->ops->draw_instanced_shadow_mesh)
    renderer->ops->draw_instanced_shadow_mesh(renderer->backend, mesh, mvp, instances, instance_count);
}

void BLB_RendererDrawInstancedShadowPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *mvp,
                                              const BLB_RenderInstanceData *instances, size_t instance_count) {
  if (renderer && renderer->ops && renderer->ops->draw_instanced_shadow_polygon3d)
    renderer->ops->draw_instanced_shadow_polygon3d(renderer->backend, polygon, mvp, instances, instance_count);
}

void BLB_RendererDrawTriangle(BLB_Renderer *renderer, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color,
                              float a_color, BLB_Vec3 camera_position) {
  if (renderer && renderer->ops && renderer->ops->draw_triangle)
    renderer->ops->draw_triangle(renderer->backend, a, b, c, mvp, r, g, b_color, a_color, camera_position);
}

void BLB_RendererDrawTriangle2D(BLB_Renderer *renderer, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color,
                                float a_color, BLB_Vec3 camera_position, int lighting_enabled) {
  if (renderer && renderer->ops && renderer->ops->draw_triangle2d)
    renderer->ops->draw_triangle2d(renderer->backend, a, b, c, mvp, r, g, b_color, a_color, camera_position, lighting_enabled);
}

void BLB_RendererDrawMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, BLB_Vec3 camera_position) {
  if (renderer && renderer->ops && renderer->ops->draw_mesh)
    renderer->ops->draw_mesh(renderer->backend, mesh, mvp, r, g, b, a, camera_position);
}

void BLB_RendererDrawCachedMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, float r, float g, float b, float a,
                                BLB_Vec3 camera_position) {
  if (renderer && renderer->ops && renderer->ops->draw_cached_mesh)
    renderer->ops->draw_cached_mesh(renderer->backend, mesh, mvp, r, g, b, a, camera_position);
}

void BLB_RendererDrawCachedMeshMaterial(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, const float *model_rows, float r, float g,
                                        float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture, BLB_Vec3 camera_position) {
  if (renderer && renderer->ops && renderer->ops->draw_cached_mesh_material)
    renderer->ops->draw_cached_mesh_material(renderer->backend, mesh, mvp, model_rows, r, g, b, a, material, texture, camera_position);
}

void BLB_RendererDrawInstancedMesh(BLB_Renderer *renderer, const Mesh *mesh, const float *mvp, const BLB_RenderMaterial *material,
                                   BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t instance_count, BLB_Vec3 camera_position) {
  if (renderer && renderer->ops && renderer->ops->draw_instanced_mesh)
    renderer->ops->draw_instanced_mesh(renderer->backend, mesh, mvp, material, texture, instances, instance_count, camera_position);
}

void BLB_RendererDrawInstancedPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *mvp, const BLB_RenderMaterial *material,
                                        BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t instance_count,
                                        BLB_Vec3 camera_position) {
  if (renderer && renderer->ops && renderer->ops->draw_instanced_polygon3d)
    renderer->ops->draw_instanced_polygon3d(renderer->backend, polygon, mvp, material, texture, instances, instance_count, camera_position);
}

void BLB_RendererDrawPolygon3D(BLB_Renderer *renderer, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g,
                               float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture) {
  if (renderer && renderer->ops && renderer->ops->draw_polygon3d)
    renderer->ops->draw_polygon3d(renderer->backend, polygon, mvp, model_rows, r, g, b, a, material, texture);
}

void BLB_RendererDrawPolygon2D(BLB_Renderer *renderer, const BLB_Polygon2D *polygon, const BLB_Vec2 *world_positions, float viewport_width,
                               float viewport_height, float r, float g, float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture) {
  if (renderer && renderer->ops && renderer->ops->draw_polygon2d)
    renderer->ops->draw_polygon2d(renderer->backend, polygon, world_positions, viewport_width, viewport_height, r, g, b, a, material, texture);
}

void BLB_RendererDrawBatchedPolygon2D(BLB_Renderer *renderer, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                      const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                      size_t instance_count, bool camera_attached) {
  if (renderer && renderer->ops && renderer->ops->draw_batched_polygon2d)
    renderer->ops->draw_batched_polygon2d(renderer->backend, polygon, viewport_width, viewport_height, material, texture, instances, instance_count,
                                          camera_attached);
}

void BLB_RendererDrawInstancedPolygon2D(BLB_Renderer *renderer, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                        const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                        size_t instance_count, bool camera_attached) {
  if (renderer && renderer->ops && renderer->ops->draw_instanced_polygon2d)
    renderer->ops->draw_instanced_polygon2d(renderer->backend, polygon, viewport_width, viewport_height, material, texture, instances, instance_count,
                                            camera_attached);
}

int BLB_RendererEnsureInstanceCapacity(BLB_Renderer *renderer, size_t required_instances) {
  return renderer && renderer->ops && renderer->ops->ensure_instance_capacity
             ? renderer->ops->ensure_instance_capacity(renderer->backend, required_instances)
             : -1;
}

void BLB_RendererClearCachedMeshes(BLB_Renderer *renderer) {
  if (renderer && renderer->ops && renderer->ops->clear_cached_meshes)
    renderer->ops->clear_cached_meshes(renderer->backend);
}

int BLB_RendererLoadFont(BLB_Renderer *renderer, const Font *font) {
  return renderer && renderer->ops && renderer->ops->load_font ? renderer->ops->load_font(renderer->backend, font) : -1;
}

void BLB_RendererUnloadFont(BLB_Renderer *renderer) {
  if (renderer && renderer->ops && renderer->ops->unload_font)
    renderer->ops->unload_font(renderer->backend);
}

void BLB_RendererDrawText(BLB_Renderer *renderer, const Font *font, const char *text, float x, float y, float glyph_scale, BLB_Vec2 transform_scale,
                          float rotation, float r, float g, float b, float a, float emission, float glow, float roundness,
                          BLB_RenderMode render_mode) {
  if (renderer && renderer->ops && renderer->ops->draw_text)
    renderer->ops->draw_text(renderer->backend, font, text, x, y, glyph_scale, transform_scale, rotation, r, g, b, a, emission, glow, roundness,
                             render_mode);
}

void BLB_RendererDrawTextTransformed(BLB_Renderer *renderer, const Font *font, const char *text, float x, float y, float glyph_scale,
                                     BLB_Vec2 transform_scale, float rotation, float r, float g, float b, float a, float emission, float glow,
                                     float roundness, BLB_RenderMode render_mode, const float *mvp) {
  if (renderer && renderer->ops && renderer->ops->draw_text_transformed)
    renderer->ops->draw_text_transformed(renderer->backend, font, text, x, y, glyph_scale, transform_scale, rotation, r, g, b, a, emission, glow,
                                         roundness, render_mode, mvp);
}

int BLB_RendererRecreateSwapchain(BLB_Renderer *renderer, bool vsync) {
  if (!renderer || !renderer->ops || !renderer->ops->recreate_swapchain || renderer->frame_ready)
    return -1;

  int result = renderer->ops->recreate_swapchain(renderer->backend, vsync);

  if (result == 0) {
    renderer->requested_vsync = vsync;
    renderer->swapchain_pending = false;
  } else {
    renderer->swapchain_pending = true;
  }

  return result;
}

bool BLB_RendererGetVSync(const BLB_Renderer *renderer) {
  return renderer && renderer->ops && renderer->ops->get_vsync ? renderer->ops->get_vsync(renderer->backend) : false;
}

void BLB_RendererGetViewport(const BLB_Renderer *renderer, uint32_t *width, uint32_t *height) {
  if (width)
    *width = 0;

  if (height)
    *height = 0;

  if (renderer && renderer->ops && renderer->ops->get_viewport)
    renderer->ops->get_viewport(renderer->backend, width, height);
}

void BLB_RendererSetViewProjection(BLB_Renderer *renderer, const float *view_projection) {
  if (renderer && renderer->ops && renderer->ops->set_view_projection)
    renderer->ops->set_view_projection(renderer->backend, view_projection);
}
