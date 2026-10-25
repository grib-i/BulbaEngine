#include "bulba/graphics/vulkan/renderer_backend.h"
#include "bulba/graphics/vulkan/renderer.h"

#include <stdlib.h>
#include <string.h>

static int vk_create(void **backend, struct GLFWwindow *window) {
  if (!backend || !window)
    return -1;
  VULKAN *vulkan = calloc(1, sizeof(*vulkan));

  if (!vulkan)
    return -1;

  if (VULKAN_Init(vulkan, window) != 0) {
    free(vulkan);

    return -1;
  }

  if (VULKAN_CreateRenderer(vulkan) != 0) {
    VULKAN_Shutdown(vulkan);
    free(vulkan);

    return -1;
  }
  *backend = vulkan;
  return 0;
}

static void vk_destroy(void *backend) {
  VULKAN *vulkan = backend;

  if (!vulkan)
    return;
  VULKAN_Shutdown(vulkan);
  free(vulkan);
}

static int vk_begin_frame(void *backend) { return VULKAN_RendererBeginFrame((VULKAN *)backend); }

static int vk_end_frame(void *backend) { return VULKAN_RendererEndFrame((VULKAN *)backend); }

static void vk_set_clear_color(void *backend, float r, float g, float b, float a) { VULKAN_RendererSetClearColor((VULKAN *)backend, r, g, b, a); }

static void vk_begin_main_pass(void *backend) { VULKAN_RendererBeginMainPass((VULKAN *)backend); }

static void vk_set_camera_position(void *backend, BLB_Vec3 position) { VULKAN_RendererSetCameraPosition((VULKAN *)backend, position); }

static void vk_set_lights3d(void *backend, BLB_Light3D **lights, size_t light_count) {
  VULKAN_RendererSetLights3D((VULKAN *)backend, lights, light_count);
}

static void vk_set_lights2d(void *backend, BLB_Light2D **lights, size_t light_count) {
  VULKAN_RendererSetLights2D((VULKAN *)backend, lights, light_count);
}

static void vk_set_lighting_objects3d(void *backend, BLB_Object3D **objects, size_t object_count) {
  VULKAN_RendererSetLightingObjects3D((VULKAN *)backend, objects, object_count);
}

static void vk_set_lighting_objects2d(void *backend, BLB_Object2D **objects, size_t object_count) {
  VULKAN_RendererSetLightingObjects2D((VULKAN *)backend, objects, object_count);
}

static void vk_update_light_buffer3d(void *backend) { update_light_buffer_3d((VULKAN *)backend); }

static void vk_update_light_buffer2d(void *backend) { update_light_buffer_2d((VULKAN *)backend); }

static void vk_set_shadow(void *backend, const float *shadow_mvp, bool enabled, float bias) {
  VULKAN_RendererSetShadow((VULKAN *)backend, shadow_mvp, enabled, bias);
}

static void vk_set_point_shadow(void *backend, const BLB_Mat4 *shadow_mvp, bool enabled, float bias) {
  VULKAN_RendererSetPointShadow((VULKAN *)backend, shadow_mvp, enabled, bias);
}

static void vk_set_shadow_light3d(void *backend, BLB_Light3D *light) { ((VULKAN *)backend)->shadow_light3d = light; }

static uint32_t vk_get_shadow_mode(const void *backend) { return backend ? ((const VULKAN *)backend)->shadow_mode : 0u; }

static bool vk_get_shadow_mvp(const void *backend, uint32_t index, BLB_Mat4 *mvp) {
  const VULKAN *vulkan = backend;

  if (!vulkan || !mvp || index >= VULKAN_SHADOW_MAP_COUNT)
    return false;
  *mvp = vulkan->shadow_mvp[index];
  return true;
}

static void vk_begin_shadow_pass(void *backend, uint32_t shadow_map_index) { VULKAN_RendererBeginShadowPass((VULKAN *)backend, shadow_map_index); }

static void vk_end_shadow_pass(void *backend) { VULKAN_RendererEndShadowPass((VULKAN *)backend); }

static void vk_draw_shadow_polygon3d(void *backend, const BLB_Polygon3D *polygon, const float *model_mvp) {
  VULKAN_RendererDrawShadowPolygon3D((VULKAN *)backend, polygon, model_mvp);
}

static void vk_draw_cached_shadow_mesh(void *backend, const Mesh *mesh, const float *model_mvp) {
  VULKAN_RendererDrawCachedShadowMesh((VULKAN *)backend, mesh, model_mvp);
}

static void vk_draw_instanced_shadow_mesh(void *backend, const Mesh *mesh, const float *mvp, const BLB_RenderInstanceData *instances,
                                          size_t instance_count) {
  VULKAN_RendererDrawInstancedShadowMesh((VULKAN *)backend, mesh, mvp, instances, instance_count);
}

static void vk_draw_instanced_shadow_polygon3d(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const BLB_RenderInstanceData *instances,
                                               size_t instance_count) {
  VULKAN_RendererDrawInstancedShadowPolygon3D((VULKAN *)backend, polygon, mvp, instances, instance_count);
}

static void vk_draw_triangle(void *backend, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color,
                             BLB_Vec3 camera_position) {
  VULKAN_RendererDrawTriangle((VULKAN *)backend, a, b, c, mvp, r, g, b_color, a_color, camera_position);
}

static void vk_draw_triangle2d(void *backend, BLB_Vec3 a, BLB_Vec3 b, BLB_Vec3 c, const float *mvp, float r, float g, float b_color, float a_color,
                               BLB_Vec3 camera_position, int lighting_enabled) {
  VULKAN_RendererDrawTriangle2D((VULKAN *)backend, a, b, c, mvp, r, g, b_color, a_color, camera_position, lighting_enabled);
}

static void vk_draw_mesh(void *backend, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, BLB_Vec3 camera_position) {
  VULKAN_RendererDrawMesh((VULKAN *)backend, mesh, mvp, r, g, b, a, camera_position);
}

static void vk_draw_cached_mesh(void *backend, const Mesh *mesh, const float *mvp, float r, float g, float b, float a, BLB_Vec3 camera_position) {
  VULKAN_RendererDrawCachedMesh((VULKAN *)backend, mesh, mvp, r, g, b, a, camera_position);
}

static void vk_draw_cached_mesh_material(void *backend, const Mesh *mesh, const float *mvp, const float *model_rows, float r, float g, float b,
                                         float a, const BLB_RenderMaterial *material, BLB_Texture *texture, BLB_Vec3 camera_position) {
  VULKAN_RendererDrawCachedMeshMaterial((VULKAN *)backend, mesh, mvp, model_rows, r, g, b, a, material, texture, camera_position);
}

static void vk_draw_instanced_mesh(void *backend, const Mesh *mesh, const float *mvp, const BLB_RenderMaterial *material, BLB_Texture *texture,
                                   const BLB_RenderInstanceData *instances, size_t instance_count, BLB_Vec3 camera_position) {
  VULKAN_RendererDrawInstancedMesh((VULKAN *)backend, mesh, mvp, material, texture, instances, instance_count, camera_position);
}

static void vk_draw_instanced_polygon3d(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const BLB_RenderMaterial *material,
                                        BLB_Texture *texture, const BLB_RenderInstanceData *instances, size_t instance_count,
                                        BLB_Vec3 camera_position) {
  VULKAN_RendererDrawInstancedPolygon3D((VULKAN *)backend, polygon, mvp, material, texture, instances, instance_count, camera_position);
}

static void vk_draw_polygon3d(void *backend, const BLB_Polygon3D *polygon, const float *mvp, const float *model_rows, float r, float g, float b,
                              float a, const BLB_RenderMaterial *material, BLB_Texture *texture) {
  VULKAN_RendererDrawPolygon3D((VULKAN *)backend, polygon, mvp, model_rows, r, g, b, a, material, texture);
}

static void vk_draw_polygon2d(void *backend, const BLB_Polygon2D *polygon, const BLB_Vec2 *world_positions, float viewport_width,
                              float viewport_height, float r, float g, float b, float a, const BLB_RenderMaterial *material, BLB_Texture *texture) {
  VULKAN_RendererDrawPolygon2D((VULKAN *)backend, polygon, world_positions, viewport_width, viewport_height, r, g, b, a, material, texture);
}

static void vk_draw_batched_polygon2d(void *backend, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                      const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                      size_t instance_count, bool camera_attached) {
  VULKAN_RendererDrawBatchedPolygon2D((VULKAN *)backend, polygon, viewport_width, viewport_height, material, texture, instances, instance_count,
                                      camera_attached);
}

static void vk_draw_instanced_polygon2d(void *backend, const BLB_Polygon2D *polygon, float viewport_width, float viewport_height,
                                        const BLB_RenderMaterial *material, BLB_Texture *texture, const BLB_RenderInstanceData *instances,
                                        size_t instance_count, bool camera_attached) {
  VULKAN_RendererDrawInstancedPolygon2D((VULKAN *)backend, polygon, viewport_width, viewport_height, material, texture, instances, instance_count,
                                        camera_attached);
}

static int vk_ensure_instance_capacity(void *backend, size_t required_instances) {
  return VULKAN_RendererEnsureInstanceCapacity((VULKAN *)backend, required_instances);
}

static void vk_clear_cached_meshes(void *backend) { VULKAN_RendererClearCachedMeshes((VULKAN *)backend); }

static int vk_load_font(void *backend, const Font *font) { return VULKAN_RendererLoadFont((VULKAN *)backend, font); }

static void vk_unload_font(void *backend) { VULKAN_RendererUnloadFont((VULKAN *)backend); }

static void vk_draw_text(void *backend, const Font *font, const char *text, float x, float y, float glyph_scale, BLB_Vec2 transform_scale,
                         float rotation, float r, float g, float b, float a, float emission, float glow, float roundness,
                         BLB_RenderMode render_mode) {
  VULKAN_RendererDrawText((VULKAN *)backend, font, text, x, y, glyph_scale, transform_scale, rotation, r, g, b, a, emission, glow, roundness,
                          render_mode);
}

static void vk_draw_text_transformed(void *backend, const Font *font, const char *text, float x, float y, float glyph_scale, BLB_Vec2 transform_scale,
                                     float rotation, float r, float g, float b, float a, float emission, float glow, float roundness,
                                     BLB_RenderMode render_mode, const float *mvp) {
  VULKAN_RendererDrawTextTransformed((VULKAN *)backend, font, text, x, y, glyph_scale, transform_scale, rotation, r, g, b, a, emission, glow,
                                     roundness, render_mode, mvp);
}

static int vk_recreate_swapchain(void *backend, bool vsync) { return VULKAN_RendererRecreateSwapchain((VULKAN *)backend, vsync); }

static bool vk_get_vsync(const void *backend) { return backend ? ((const VULKAN *)backend)->vsync_enabled : false; }

static void vk_get_viewport(const void *backend, uint32_t *width, uint32_t *height) {
  const VULKAN *vulkan = backend;

  if (width)
    *width = vulkan ? vulkan->swapchain_extent.width : 0;
  if (height)
    *height = vulkan ? vulkan->swapchain_extent.height : 0;
}

static void vk_set_view_projection(void *backend, const float *view_projection) {
  if (!backend || !view_projection)
    return;
  memcpy(((VULKAN *)backend)->view_projection, view_projection, sizeof(((VULKAN *)backend)->view_projection));
}

static const BLB_RenderBackendOps blb_vulkan_ops = {
    .name = "Vulkan",
    .create = vk_create,
    .destroy = vk_destroy,
    .begin_frame = vk_begin_frame,
    .end_frame = vk_end_frame,
    .set_clear_color = vk_set_clear_color,
    .begin_main_pass = vk_begin_main_pass,
    .set_camera_position = vk_set_camera_position,
    .set_lights3d = vk_set_lights3d,
    .set_lights2d = vk_set_lights2d,
    .set_lighting_objects3d = vk_set_lighting_objects3d,
    .set_lighting_objects2d = vk_set_lighting_objects2d,
    .update_light_buffer3d = vk_update_light_buffer3d,
    .update_light_buffer2d = vk_update_light_buffer2d,
    .set_shadow = vk_set_shadow,
    .set_point_shadow = vk_set_point_shadow,
    .set_shadow_light3d = vk_set_shadow_light3d,
    .get_shadow_mode = vk_get_shadow_mode,
    .get_shadow_mvp = vk_get_shadow_mvp,
    .begin_shadow_pass = vk_begin_shadow_pass,
    .end_shadow_pass = vk_end_shadow_pass,
    .draw_shadow_polygon3d = vk_draw_shadow_polygon3d,
    .draw_cached_shadow_mesh = vk_draw_cached_shadow_mesh,
    .draw_instanced_shadow_mesh = vk_draw_instanced_shadow_mesh,
    .draw_instanced_shadow_polygon3d = vk_draw_instanced_shadow_polygon3d,
    .draw_triangle = vk_draw_triangle,
    .draw_triangle2d = vk_draw_triangle2d,
    .draw_mesh = vk_draw_mesh,
    .draw_cached_mesh = vk_draw_cached_mesh,
    .draw_cached_mesh_material = vk_draw_cached_mesh_material,
    .draw_instanced_mesh = vk_draw_instanced_mesh,
    .draw_instanced_polygon3d = vk_draw_instanced_polygon3d,
    .draw_polygon3d = vk_draw_polygon3d,
    .draw_polygon2d = vk_draw_polygon2d,
    .draw_batched_polygon2d = vk_draw_batched_polygon2d,
    .draw_instanced_polygon2d = vk_draw_instanced_polygon2d,
    .ensure_instance_capacity = vk_ensure_instance_capacity,
    .clear_cached_meshes = vk_clear_cached_meshes,
    .load_font = vk_load_font,
    .unload_font = vk_unload_font,
    .draw_text = vk_draw_text,
    .draw_text_transformed = vk_draw_text_transformed,
    .recreate_swapchain = vk_recreate_swapchain,
    .get_vsync = vk_get_vsync,
    .get_viewport = vk_get_viewport,
    .set_view_projection = vk_set_view_projection,
};

const BLB_RenderBackendOps *BLB_VulkanRendererBackendOps(void) { return &blb_vulkan_ops; }

int BLB_VulkanRendererBackendAttach(BLB_Renderer *renderer, VULKAN *vulkan) {
  return BLB_Renderer_Attach(renderer, BLB_RENDER_API_VULKAN, vulkan, &blb_vulkan_ops);
}
