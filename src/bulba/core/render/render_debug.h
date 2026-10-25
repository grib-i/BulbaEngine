#ifndef BLB_RENDER_DEBUG_INTERNAL_H
#define BLB_RENDER_DEBUG_INTERNAL_H
#include "bulba/core/render.h"
#include "bulba/graphics/renderer.h"
BLB_Mat4 blb_render_object3d_model_matrix(const BLB_Object3D *object, const BLB_CameraCache *camera_cache, float scale_mul);
void blb_render_draw_physics_debug_colliders3d(BLB_Scene *scene, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache);
void blb_render_draw_physics_debug_colliders2d(BLB_Scene *scene, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache);
void blb_render_draw_handler_debug_hitboxes(BLB_Scene *scene, BLB_Renderer *renderer, const BLB_CameraCache *camera_cache);
#endif
