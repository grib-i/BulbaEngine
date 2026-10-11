#include "bulba/handlers/object.h"

#include "bulba/core/camera.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/scene.h"
#include "bulba/handlers/handlers.h"

#include <GLFW/glfw3.h>
#include <float.h>
#include <math.h>
#include <string.h>

#ifndef BLB_OBJECT2D_CAMERA_SPACE_DEPTH
#define BLB_OBJECT2D_CAMERA_SPACE_DEPTH 1.0f
#endif

static double event_time(void) { return glfwGetTime(); }

void BLB_ObjectHandlerInit(BLB_ObjectHandler *handler) {
  if (!handler)
    return;

  *handler = (BLB_ObjectHandler){0};
  handler->enabled = true;
}

bool BLB_ObjectHandlerIsHovered(const BLB_ObjectHandler *handler) { return handler && handler->hovered; }

bool BLB_ObjectHandlerIsDown(const BLB_ObjectHandler *handler, int button) {
  return handler && button >= 0 && button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT && (handler->down_buttons & (uint16_t)(1u << (unsigned)button)) != 0;
}

bool BLB_ObjectHandlerWasPressed(const BLB_ObjectHandler *handler, int button) {
  return handler && button >= 0 && button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT &&
         (handler->pressed_buttons & (uint16_t)(1u << (unsigned)button)) != 0;
}

bool BLB_ObjectHandlerWasReleased(const BLB_ObjectHandler *handler, int button) {
  return handler && button >= 0 && button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT &&
         (handler->released_buttons & (uint16_t)(1u << (unsigned)button)) != 0;
}

bool BLB_ObjectHandlerIsCaptured(const BLB_ObjectHandler *handler, int button) {
  return handler && button >= 0 && button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT &&
         (handler->captured_buttons & (uint16_t)(1u << (unsigned)button)) != 0;
}

BLB_ObjectEvent BLB_ObjectEventZero(void) {
  BLB_ObjectEvent event = {0};
  event.distance = -1.0f;
  event.t = -1.0f;
  return event;
}

static BLB_ObjectHandlerCallback handler_callback(BLB_ObjectHandlerEventType type, const BLB_ObjectHandler *handler) {
  if (!handler || !handler->enabled)
    return NULL;

  switch (type) {
  case BLB_OBJECT_HANDLER_CLICK:
    return handler->on_click;
  case BLB_OBJECT_HANDLER_HOLD:
    return handler->on_hold;
  case BLB_OBJECT_HANDLER_PRESS:
    return handler->on_press;
  case BLB_OBJECT_HANDLER_RELEASE:
    return handler->on_release;
  case BLB_OBJECT_HANDLER_HOVER_ENTER:
    return handler->on_hover_enter;
  case BLB_OBJECT_HANDLER_HOVER_LEAVE:
    return handler->on_hover_leave;
  case BLB_OBJECT_HANDLER_DRAG_START:
    return handler->on_drag_start;
  case BLB_OBJECT_HANDLER_DRAG:
    return handler->on_drag;
  case BLB_OBJECT_HANDLER_DRAG_END:
    return handler->on_drag_end;
  case BLB_OBJECT_HANDLER_CANCEL:
    return handler->on_cancel;
  default:
    return NULL;
  }
}

static void cache_bounds_2d(BLB_ObjectHandler *handler, const BLB_Polygon2D *polygon) {
  if (!handler || !polygon || !polygon->vertices || polygon->vertex_count == 0)
    return;

  HMM_Vec2 min_v = polygon->vertices[0];
  HMM_Vec2 max_v = polygon->vertices[0];

  for (size_t i = 1; i < polygon->vertex_count; ++i) {
    const HMM_Vec2 p = polygon->vertices[i];
    min_v.x = fminf(min_v.x, p.x);
    min_v.y = fminf(min_v.y, p.y);
    max_v.x = fmaxf(max_v.x, p.x);
    max_v.y = fmaxf(max_v.y, p.y);
  }

  handler->object_geometry = polygon;
  handler->object_bounds_min = HMM_V3(min_v.x, min_v.y, 0.0f);
  handler->object_bounds_max = HMM_V3(max_v.x, max_v.y, 0.0f);
}

static void cache_bounds_3d(BLB_ObjectHandler *handler, const BLB_Polygon3D *polygon) {
  if (!handler || !polygon || !polygon->vertices || polygon->vertex_count == 0)
    return;

  HMM_Vec3 min_v = polygon->vertices[0];
  HMM_Vec3 max_v = polygon->vertices[0];

  for (size_t i = 1; i < polygon->vertex_count; ++i) {
    const HMM_Vec3 p = polygon->vertices[i];
    min_v.x = fminf(min_v.x, p.x);
    min_v.y = fminf(min_v.y, p.y);
    min_v.z = fminf(min_v.z, p.z);
    max_v.x = fmaxf(max_v.x, p.x);
    max_v.y = fmaxf(max_v.y, p.y);
    max_v.z = fmaxf(max_v.z, p.z);
  }

  handler->object_geometry = polygon;
  handler->object_bounds_min = min_v;
  handler->object_bounds_max = max_v;
}

static bool ray_aabb(HMM_Vec3 origin, HMM_Vec3 direction, HMM_Vec3 min_v, HMM_Vec3 max_v, float *out_t) {
  float t_min = 0.0f;
  float t_max = FLT_MAX;

  for (int axis = 0; axis < 3; ++axis) {
    const float o = axis == 0 ? origin.x : axis == 1 ? origin.y : origin.z;
    const float d = axis == 0 ? direction.x : axis == 1 ? direction.y : direction.z;
    const float mn = axis == 0 ? min_v.x : axis == 1 ? min_v.y : min_v.z;
    const float mx = axis == 0 ? max_v.x : axis == 1 ? max_v.y : max_v.z;

    if (fabsf(d) < 1e-7f) {
      if (o < mn || o > mx)
        return false;

      continue;
    }

    float t1 = (mn - o) / d;
    float t2 = (mx - o) / d;

    if (t1 > t2) {
      const float tmp = t1;
      t1 = t2;
      t2 = tmp;
    }

    if (t1 > t_min)
      t_min = t1;

    if (t2 < t_max)
      t_max = t2;

    if (t_min > t_max)
      return false;
  }

  if (out_t)
    *out_t = t_min;

  return t_max >= 0.0f;
}

static HMM_Vec2 inverse_point_2d(const BLB_Object2D *object, HMM_Vec2 point) {
  const float sx = object->scale.x;
  const float sy = object->scale.y;

  if (fabsf(sx) < 1e-7f || fabsf(sy) < 1e-7f)
    return HMM_V2(FLT_MAX, FLT_MAX);

  const float angle = HMM_AngleDeg(object->rotation);
  const float c = cosf(angle);
  const float s = sinf(angle);
  const float x = point.x - object->position.x;
  const float y = point.y - object->position.y;

  return HMM_V2((x * c + y * s) / sx, (-x * s + y * c) / sy);
}

static bool point_in_triangle_2d(HMM_Vec2 point, HMM_Vec2 a, HMM_Vec2 b, HMM_Vec2 c) {
  const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

  if (fabsf(area) < 1e-12f)
    return false;

  const float ab = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
  const float bc = (c.x - b.x) * (point.y - b.y) - (c.y - b.y) * (point.x - b.x);
  const float ca = (a.x - c.x) * (point.y - c.y) - (a.y - c.y) * (point.x - c.x);
  const float tolerance = fmaxf(fabsf(area) * 1e-6f, 1e-12f);
  const bool has_negative = ab < -tolerance || bc < -tolerance || ca < -tolerance;
  const bool has_positive = ab > tolerance || bc > tolerance || ca > tolerance;

  return !(has_negative && has_positive);
}

static bool point_in_polygon(const BLB_Polygon2D *polygon, HMM_Vec2 point) {
  if (!polygon || !polygon->vertices || polygon->vertex_count < 3)
    return false;

  if (polygon->indices && polygon->index_count >= 3 && polygon->index_count % 3 == 0) {
    for (size_t index = 0; index < polygon->index_count; index += 3u) {
      const unsigned int ia = polygon->indices[index];
      const unsigned int ib = polygon->indices[index + 1u];
      const unsigned int ic = polygon->indices[index + 2u];

      if (ia >= polygon->vertex_count || ib >= polygon->vertex_count || ic >= polygon->vertex_count)
        continue;

      if (point_in_triangle_2d(point, polygon->vertices[ia], polygon->vertices[ib], polygon->vertices[ic]))
        return true;
    }

    return false;
  }

  bool inside = false;
  size_t j = polygon->vertex_count - 1u;

  for (size_t i = 0; i < polygon->vertex_count; ++i) {
    const HMM_Vec2 a = polygon->vertices[i];
    const HMM_Vec2 b = polygon->vertices[j];
    const float denominator = b.y - a.y;
    const float safe_denominator = fabsf(denominator) < 1e-20f ? (denominator < 0.0f ? -1e-20f : 1e-20f) : denominator;
    const bool crosses = ((a.y > point.y) != (b.y > point.y)) && point.x < (b.x - a.x) * (point.y - a.y) / safe_denominator + a.x;

    if (crosses)
      inside = !inside;

    j = i;
  }

  return inside;
}

static bool ray_triangle(HMM_Vec3 origin, HMM_Vec3 direction, HMM_Vec3 a, HMM_Vec3 b, HMM_Vec3 c, float *out_t, HMM_Vec3 *out_normal) {
  const HMM_Vec3 edge1 = HMM_SubV3(b, a);
  const HMM_Vec3 edge2 = HMM_SubV3(c, a);
  const HMM_Vec3 h = HMM_Cross(edge2, direction);
  const float det = HMM_DotV3(edge1, h);

  if (fabsf(det) < 1e-7f)
    return false;

  const float inv_det = 1.0f / det;
  const HMM_Vec3 s = HMM_SubV3(origin, a);
  const float u = HMM_DotV3(s, h) * inv_det;

  if (u < 0.0f || u > 1.0f)
    return false;

  const HMM_Vec3 q = HMM_Cross(s, edge1);
  const float v = HMM_DotV3(direction, q) * inv_det;

  if (v < 0.0f || u + v > 1.0f)
    return false;

  const float t = HMM_DotV3(edge2, q) * inv_det;

  if (t < 0.0f)
    return false;

  if (out_t)
    *out_t = t;

  if (out_normal)
    *out_normal = HMM_NormV3(HMM_Cross(edge1, edge2));

  return true;
}

static bool framebuffer_mouse_coordinates(const BLB_Handlers *handlers, float *screen_x, float *screen_y, float *width, float *height) {
  if (!handlers || !handlers->window || !screen_x || !screen_y || !width || !height)
    return false;

  int window_width = 0;
  int window_height = 0;
  int framebuffer_width = 0;
  int framebuffer_height = 0;

  glfwGetWindowSize(handlers->window, &window_width, &window_height);
  glfwGetFramebufferSize(handlers->window, &framebuffer_width, &framebuffer_height);

  if (window_width <= 0 || window_height <= 0 || framebuffer_width <= 0 || framebuffer_height <= 0)
    return false;

  *width = (float)framebuffer_width;
  *height = (float)framebuffer_height;
  *screen_x = (float)(handlers->mouse_x * (double)framebuffer_width / (double)window_width);
  *screen_y = (float)(handlers->mouse_y * (double)framebuffer_height / (double)window_height);

  return true;
}

static HMM_Mat4 model_3d(const BLB_Object3D *object, const BLB_CameraCache *camera_cache) {
  const HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));
  const HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));
  const HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));
  const HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));
  HMM_Mat4 model = HMM_MulM4(HMM_Translate(object->position), HMM_MulM4(rotation, HMM_Scale(object->scale)));

  if (object->camera_attached && camera_cache && camera_cache->valid)
    model = HMM_MulM4(HMM_InvGeneralM4(camera_cache->view), model);

  return model;
}

static bool screen_ray(const BLB_CameraCache *camera_cache, HMM_Vec3 *origin, HMM_Vec3 *direction, HMM_Vec2 *world2d, float width, float height,
                       float screen_x, float screen_y) {
  if (!camera_cache || !camera_cache->valid || !origin || !direction || width <= 0.0f || height <= 0.0f)
    return false;

  const HMM_Mat4 inv_vp = HMM_InvGeneralM4(camera_cache->view_projection);
  const float nx = screen_x / width * 2.0f - 1.0f;
  const float ny = screen_y / height * 2.0f - 1.0f;
  const HMM_Vec4 near_clip = HMM_V4(nx, ny, 0.0f, 1.0f);
  const HMM_Vec4 far_clip = HMM_V4(nx, ny, 1.0f, 1.0f);
  const HMM_Vec4 near_world = HMM_MulM4V4(inv_vp, near_clip);
  const HMM_Vec4 far_world = HMM_MulM4V4(inv_vp, far_clip);

  if (fabsf(near_world.w) < 1e-7f || fabsf(far_world.w) < 1e-7f)
    return false;

  *origin = HMM_V3(near_world.x / near_world.w, near_world.y / near_world.w, near_world.z / near_world.w);

  const HMM_Vec3 far_point = HMM_V3(far_world.x / far_world.w, far_world.y / far_world.w, far_world.z / far_world.w);
  const HMM_Vec3 ray = HMM_SubV3(far_point, *origin);

  if (HMM_LenV3(ray) < 1e-7f)
    return false;

  *direction = HMM_NormV3(ray);

  if (world2d) {
    if (fabsf(direction->z) > 1e-7f) {
      const float t = -origin->z / direction->z;

      if (t < 0.0f) {
        *world2d = HMM_V2(0.0f, 0.0f);
      } else {
        *world2d = HMM_V2(origin->x + direction->x * t, origin->y + direction->y * t);
      }
    } else {
      *world2d = HMM_V2(origin->x, origin->y);
    }
  }

  return true;
}

static bool camera_plane_point(const BLB_CameraCache *camera_cache, HMM_Vec3 ray_origin, HMM_Vec3 ray_direction, float camera_z,
                               HMM_Vec2 *local_point, HMM_Vec3 *world_point, float *out_t) {
  if (!camera_cache || !camera_cache->valid || !local_point)
    return false;

  const HMM_Vec4 origin_view4 = HMM_MulM4V4(camera_cache->view, HMM_V4(ray_origin.x, ray_origin.y, ray_origin.z, 1.0f));
  const HMM_Vec4 direction_view4 = HMM_MulM4V4(camera_cache->view, HMM_V4(ray_direction.x, ray_direction.y, ray_direction.z, 0.0f));

  if (fabsf(direction_view4.z) < 1e-7f)
    return false;

  const float t = (camera_z - origin_view4.z) / direction_view4.z;

  if (t < 0.0f)
    return false;

  const HMM_Vec3 point_view = HMM_V3(origin_view4.x + direction_view4.x * t, origin_view4.y + direction_view4.y * t, camera_z);
  *local_point = HMM_V2(point_view.x, point_view.y);

  if (world_point)
    *world_point = HMM_AddV3(ray_origin, HMM_MulV3F(ray_direction, t));

  if (out_t)
    *out_t = t;

  return true;
}

typedef struct {
  bool valid;
  HMM_Vec3 origin;
  HMM_Vec3 direction;
  HMM_Vec2 world2d;
  HMM_Vec2 camera2d;
  HMM_Vec3 camera_plane_world;
  bool camera_plane_valid;
} RayQuery;

static bool build_ray_query(BLB_Handlers *handlers, BLB_Scene *scene, RayQuery *query) {
  if (!handlers || !scene || !scene->camera || !scene->camera->camera_cache || !query)
    return false;

  memset(query, 0, sizeof(*query));

  float framebuffer_x = 0.0f;
  float framebuffer_y = 0.0f;
  float framebuffer_width = 0.0f;
  float framebuffer_height = 0.0f;

  if (!framebuffer_mouse_coordinates(handlers, &framebuffer_x, &framebuffer_y, &framebuffer_width, &framebuffer_height))
    return false;

  if (!screen_ray(scene->camera->camera_cache, &query->origin, &query->direction, &query->world2d, framebuffer_width, framebuffer_height,
                  framebuffer_x, framebuffer_y))
    return false;

  query->camera_plane_valid = camera_plane_point(scene->camera->camera_cache, query->origin, query->direction, -BLB_OBJECT2D_CAMERA_SPACE_DEPTH,
                                                 &query->camera2d, &query->camera_plane_world, NULL);
  query->valid = true;

  return true;
}

typedef struct {
  void *object;
  BLB_ObjectHandler *handler;
  uint8_t type;
  bool hit;
  bool camera_attached;
  unsigned short layer;
  float distance;
  float t;
  HMM_Vec2 world2d;
  HMM_Vec3 local_position;
  HMM_Vec3 hit_point;
  HMM_Vec3 hit_normal;
  HMM_Vec3 ray_origin;
  HMM_Vec3 ray_direction;
} ObjectHit;

static bool better_hit(const ObjectHit *candidate, const ObjectHit *best) {
  if (!candidate || !candidate->hit)
    return false;

  if (!best || !best->hit)
    return true;

  if (candidate->camera_attached != best->camera_attached)
    return candidate->camera_attached;

  if (candidate->layer != best->layer)
    return candidate->layer > best->layer;

  if (candidate->type == best->type)
    return candidate->distance < best->distance;

  return candidate->distance < best->distance;
}

static ObjectHit find_hit(BLB_Handlers *handlers, BLB_Scene *scene) {
  ObjectHit best = {0};
  RayQuery ray = {0};

  if (!build_ray_query(handlers, scene, &ray))
    return best;

  best.ray_origin = ray.origin;
  best.ray_direction = ray.direction;
  best.world2d = ray.world2d;

  for (int i = 0; i < scene->object2d_count; ++i) {
    BLB_Object2D *object = scene->objects2d[i];

    if (!object || !object->visible || !object->handler || !object->handler->enabled)
      continue;

    if (!object->polygon || !object->polygon->vertices || object->polygon->vertex_count == 0)
      continue;

    if (object->handler->object_geometry != object->polygon)
      cache_bounds_2d(object->handler, object->polygon);

    const bool camera_local = object->camera_attached;
    if (camera_local && !ray.camera_plane_valid)
      continue;

    const HMM_Vec2 point = camera_local ? ray.camera2d : ray.world2d;
    const HMM_Vec2 local = inverse_point_2d(object, point);

    if (local.x == FLT_MAX)
      continue;

    if (local.x < object->handler->object_bounds_min.x || local.x > object->handler->object_bounds_max.x ||
        local.y < object->handler->object_bounds_min.y || local.y > object->handler->object_bounds_max.y)
      continue;

    if (!point_in_polygon(object->polygon, local))
      continue;

    float hit_t = 0.0f;
    float distance = 0.0f;

    HMM_Vec3 hit_point = HMM_V3(point.x, point.y, 0.0f);

    if (camera_local) {
      hit_point = ray.camera_plane_world;
      distance = HMM_LenV3(HMM_SubV3(hit_point, ray.origin));
      hit_t = distance;
    } else {
      if (fabsf(ray.direction.z) < 1e-7f)
        continue;

      hit_t = -ray.origin.z / ray.direction.z;

      if (hit_t < 0.0f)
        continue;

      hit_point = HMM_AddV3(ray.origin, HMM_MulV3F(ray.direction, hit_t));
      distance = HMM_LenV3(HMM_SubV3(hit_point, ray.origin));
    }

    ObjectHit candidate = {.object = object,
                           .handler = object->handler,
                           .type = BLB_OBJECT_HANDLER_2D,
                           .hit = true,
                           .camera_attached = object->camera_attached,
                           .layer = object->layer,
                           .distance = distance,
                           .t = hit_t,
                           .world2d = point,
                           .local_position = HMM_V3(local.x, local.y, 0.0f),
                           .hit_point = hit_point,
                           .hit_normal = HMM_V3(0.0f, 0.0f, 1.0f),
                           .ray_origin = ray.origin,
                           .ray_direction = ray.direction};

    if (better_hit(&candidate, &best))
      best = candidate;
  }

  for (int i = 0; i < scene->object3d_count; ++i) {
    BLB_Object3D *object = scene->objects3d[i];

    if (!object || !object->visible || !object->handler || !object->handler->enabled)
      continue;

    if (fabsf(object->scale.x) < 1e-7f || fabsf(object->scale.y) < 1e-7f || fabsf(object->scale.z) < 1e-7f)
      continue;

    const BLB_Polygon3D *polygon = object->type == BLB_OBJECT_CUSTOM_MODEL ? object->polygon : object->polygon;
    if (!polygon || !polygon->vertices || polygon->vertex_count == 0)
      continue;

    if (object->handler->object_geometry != polygon || object->animation3d)
      cache_bounds_3d(object->handler, polygon);

    const HMM_Mat4 model = model_3d(object, scene->camera ? scene->camera->camera_cache : NULL);
    const HMM_Mat4 inv_model = HMM_InvGeneralM4(model);
    const HMM_Vec4 local_origin4 = HMM_MulM4V4(inv_model, HMM_V4(ray.origin.x, ray.origin.y, ray.origin.z, 1.0f));
    const HMM_Vec4 local_direction4 = HMM_MulM4V4(inv_model, HMM_V4(ray.direction.x, ray.direction.y, ray.direction.z, 0.0f));
    const HMM_Vec3 local_origin = HMM_V3(local_origin4.x, local_origin4.y, local_origin4.z);
    const HMM_Vec3 local_direction = HMM_V3(local_direction4.x, local_direction4.y, local_direction4.z);

    if (HMM_LenV3(local_direction) < 1e-7f)
      continue;

    float bounds_t = 0.0f;

    if (!ray_aabb(local_origin, local_direction, object->handler->object_bounds_min, object->handler->object_bounds_max, &bounds_t))
      continue;

    float nearest_t = FLT_MAX;
    HMM_Vec3 nearest_normal = HMM_V3(0.0f, 0.0f, 0.0f);
    bool triangle_hit = false;

    if (polygon->indices && polygon->index_count >= 3 && polygon->index_count % 3 == 0) {
      for (size_t index = 0; index < polygon->index_count; index += 3u) {
        const unsigned int ia = polygon->indices[index];
        const unsigned int ib = polygon->indices[index + 1u];
        const unsigned int ic = polygon->indices[index + 2u];

        if (ia >= polygon->vertex_count || ib >= polygon->vertex_count || ic >= polygon->vertex_count)
          continue;

        const HMM_Vec3 a = polygon->vertices[ia];
        const HMM_Vec3 b = polygon->vertices[ib];
        const HMM_Vec3 c = polygon->vertices[ic];
        float triangle_t = 0.0f;

        if (!ray_triangle(local_origin, local_direction, a, b, c, &triangle_t, NULL))
          continue;

        if (triangle_t < nearest_t) {
          const HMM_Vec4 world_a4 = HMM_MulM4V4(model, HMM_V4(a.x, a.y, a.z, 1.0f));
          const HMM_Vec4 world_b4 = HMM_MulM4V4(model, HMM_V4(b.x, b.y, b.z, 1.0f));
          const HMM_Vec4 world_c4 = HMM_MulM4V4(model, HMM_V4(c.x, c.y, c.z, 1.0f));
          const HMM_Vec3 world_a = HMM_V3(world_a4.x, world_a4.y, world_a4.z);
          const HMM_Vec3 world_b = HMM_V3(world_b4.x, world_b4.y, world_b4.z);
          const HMM_Vec3 world_c = HMM_V3(world_c4.x, world_c4.y, world_c4.z);

          nearest_t = triangle_t;
          nearest_normal = HMM_NormV3(HMM_Cross(HMM_SubV3(world_b, world_a), HMM_SubV3(world_c, world_a)));
          triangle_hit = true;
        }
      }
    }

    if (!triangle_hit) {
      nearest_t = bounds_t;
      nearest_normal = HMM_NormV3(HMM_SubV3(local_origin, HMM_V3(0.0f, 0.0f, 0.0f)));
    }

    const HMM_Vec3 local_hit = HMM_AddV3(local_origin, HMM_MulV3F(local_direction, nearest_t));
    const HMM_Vec4 world_hit4 = HMM_MulM4V4(model, HMM_V4(local_hit.x, local_hit.y, local_hit.z, 1.0f));
    const HMM_Vec3 hit_point = HMM_V3(world_hit4.x, world_hit4.y, world_hit4.z);
    const HMM_Vec3 hit_normal = triangle_hit ? HMM_NormV3(nearest_normal) : HMM_V3(0.0f, 0.0f, 1.0f);
    const float distance = HMM_LenV3(HMM_SubV3(hit_point, ray.origin));

    ObjectHit candidate = {.object = object,
                           .handler = object->handler,
                           .type = BLB_OBJECT_HANDLER_3D,
                           .hit = true,
                           .camera_attached = object->camera_attached,
                           .layer = object->layer,
                           .distance = distance,
                           .t = nearest_t,
                           .world2d = HMM_V2(hit_point.x, hit_point.y),
                           .local_position = local_hit,
                           .hit_point = hit_point,
                           .hit_normal = hit_normal,
                           .ray_origin = ray.origin,
                           .ray_direction = ray.direction};

    if (better_hit(&candidate, &best))
      best = candidate;
  }

  return best;
}

static BLB_ObjectEvent make_object_event(const BLB_Handlers *handlers, const BLB_Scene *scene, const ObjectHit *hit, BLB_ObjectHandlerEventType type,
                                         int button, uint32_t mods, bool captured, bool inside) {
  BLB_ObjectEvent event = BLB_ObjectEventZero();

  event.type = type;
  event.object_type = hit ? (BLB_ObjectHandlerObjectType)hit->type : 0;

  if (hit && hit->type == BLB_OBJECT_HANDLER_2D)
    event.object.object2d = (BLB_Object2D *)hit->object;
  else if (hit)
    event.object.object3d = (BLB_Object3D *)hit->object;

  event.button = button;
  event.mods = mods;
  event.frame = scene ? scene->frame_index : 0;
  event.time = event_time();
  event.mouse_x = handlers ? handlers->mouse_x : 0.0;
  event.mouse_y = handlers ? handlers->mouse_y : 0.0;
  event.mouse_dx = handlers ? handlers->mouse_dx : 0.0;
  event.mouse_dy = handlers ? handlers->mouse_dy : 0.0;
  event.world_position = hit ? hit->world2d : HMM_V2(0.0f, 0.0f);
  event.screen_position = handlers ? HMM_V2((float)handlers->mouse_x, (float)handlers->mouse_y) : HMM_V2(0.0f, 0.0f);
  event.world_point = hit ? hit->hit_point : HMM_V3(0.0f, 0.0f, 0.0f);
  event.local_position = hit ? hit->local_position : HMM_V3(0.0f, 0.0f, 0.0f);
  event.ray_origin = hit ? hit->ray_origin : HMM_V3(0.0f, 0.0f, 0.0f);
  event.ray_direction = hit ? hit->ray_direction : HMM_V3(0.0f, 0.0f, -1.0f);
  event.hit_point = hit ? hit->hit_point : HMM_V3(0.0f, 0.0f, 0.0f);
  event.hit_normal = hit ? hit->hit_normal : HMM_V3(0.0f, 0.0f, 0.0f);
  event.distance = hit ? hit->distance : -1.0f;
  event.t = hit ? hit->t : -1.0f;
  event.hovered = hit && hit->handler ? hit->handler->hovered : false;
  event.captured = captured;
  event.inside = inside;
  if (handlers && button >= 0 && button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT) {
    const BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];
    event.button_pressed = state->pressed;
    event.button_down = state->down;
    event.button_released = state->released;
    event.hold_duration = state->down ? fmax(0.0, event.time - state->last_change_time) : 0.0;
    if (handlers->object_drags[(size_t)button].active && handlers->object_drags[(size_t)button].started)
      event.dragging = true;
  }

  return event;
}

static void invoke(BLB_ObjectHandler *handler, BLB_ObjectHandlerEventType type, const BLB_ObjectEvent *event) {
  const BLB_ObjectHandlerCallback callback = handler_callback(type, handler);

  if (callback)
    callback(event, handler->userdata);
}

static BLB_ObjectHandler *object_handler(void *object, uint8_t type) {
  if (!object)
    return NULL;
  if (type == BLB_OBJECT_HANDLER_2D)
    return ((BLB_Object2D *)object)->handler;
  if (type == BLB_OBJECT_HANDLER_3D)
    return ((BLB_Object3D *)object)->handler;
  return NULL;
}

static ObjectHit object_reference_hit(void *object, uint8_t type, const BLB_Scene *scene) {
  ObjectHit hit = {0};
  if (!object || (type != BLB_OBJECT_HANDLER_2D && type != BLB_OBJECT_HANDLER_3D))
    return hit;

  hit.object = object;
  hit.handler = object_handler(object, type);
  hit.type = type;
  hit.hit = hit.handler != NULL;
  hit.ray_direction = HMM_V3(0.0f, 0.0f, -1.0f);

  if (type == BLB_OBJECT_HANDLER_2D) {
    const BLB_Object2D *value = object;
    hit.camera_attached = value->camera_attached;
    hit.layer = value->layer;
    hit.world2d = value->position;
    hit.local_position = HMM_V3(0.0f, 0.0f, 0.0f);
    hit.hit_point = HMM_V3(value->position.x, value->position.y, 0.0f);
    if ((value->camera_attached) && scene && scene->camera && scene->camera->camera_cache && scene->camera->camera_cache->valid) {
      HMM_Mat4 camera_world = HMM_InvGeneralM4(scene->camera->camera_cache->view);
      HMM_Vec4 world4 = HMM_MulM4V4(camera_world, HMM_V4(value->position.x, value->position.y, -BLB_OBJECT2D_CAMERA_SPACE_DEPTH, 1.0f));
      if (fabsf(world4.w) > 1e-7f)
        hit.hit_point = HMM_V3(world4.x / world4.w, world4.y / world4.w, world4.z / world4.w);
    }
  } else {
    const BLB_Object3D *value = object;
    hit.camera_attached = value->camera_attached;
    hit.layer = value->layer;
    HMM_Mat4 model = model_3d(value, scene && scene->camera ? scene->camera->camera_cache : NULL);
    HMM_Vec4 world4 = HMM_MulM4V4(model, HMM_V4(0.0f, 0.0f, 0.0f, 1.0f));
    if (fabsf(world4.w) > 1e-7f)
      hit.hit_point = HMM_V3(world4.x / world4.w, world4.y / world4.w, world4.z / world4.w);
    hit.world2d = HMM_V2(hit.hit_point.x, hit.hit_point.y);
    hit.local_position = HMM_V3(0.0f, 0.0f, 0.0f);
  }

  hit.distance = -1.0f;
  hit.t = -1.0f;
  return hit;
}

static void initialize_drag_plane(BLB_Scene *scene, const ObjectHit *hit, BLB_ObjectDragState *drag) {
  if (!scene || !hit || !drag || hit->type != BLB_OBJECT_HANDLER_3D || !hit->object || !scene->camera || !scene->camera->camera_cache ||
      !scene->camera->camera_cache->valid)
    return;

  const BLB_Object3D *object = hit->object;
  const BLB_CameraCache *camera_cache = scene->camera->camera_cache;
  HMM_Mat4 object_model = model_3d(object, camera_cache);
  HMM_Vec4 point4 = HMM_MulM4V4(object_model, HMM_V4(0.0f, 0.0f, 0.0f, 1.0f));
  HMM_Mat4 camera_world = HMM_InvGeneralM4(camera_cache->view);
  HMM_Vec4 normal4 = HMM_MulM4V4(camera_world, HMM_V4(0.0f, 0.0f, 1.0f, 0.0f));

  if (fabsf(point4.w) <= 1e-7f)
    return;

  HMM_Vec3 point = HMM_V3(point4.x / point4.w, point4.y / point4.w, point4.z / point4.w);
  HMM_Vec3 normal = HMM_NormV3(HMM_V3(normal4.x, normal4.y, normal4.z));
  if (HMM_LenV3(normal) <= 1e-7f)
    return;

  drag->plane_point[0] = point.x;
  drag->plane_point[1] = point.y;
  drag->plane_point[2] = point.z;
  drag->plane_normal[0] = normal.x;
  drag->plane_normal[1] = normal.y;
  drag->plane_normal[2] = normal.z;
}

static bool capture_object_hit(BLB_Handlers *handlers, BLB_Scene *scene, const ObjectHit *current, void *captured, uint8_t type,
                               const BLB_ObjectDragState *drag, ObjectHit *output) {
  if (!handlers || !scene || !current || !captured || !output)
    return false;

  *output = object_reference_hit(captured, type, scene);
  output->ray_origin = current->ray_origin;
  output->ray_direction = current->ray_direction;
  output->hit = true;

  bool found = false;
  if (type == BLB_OBJECT_HANDLER_2D) {
    BLB_Object2D *object = captured;
    const bool camera_local = object->camera_attached;
    if (camera_local) {
      if (scene->camera && scene->camera->camera_cache && scene->camera->camera_cache->valid) {
        HMM_Vec2 local_point = HMM_V2(0.0f, 0.0f);
        HMM_Vec3 world_point = HMM_V3(0.0f, 0.0f, 0.0f);
        float t = 0.0f;
        if (camera_plane_point(scene->camera->camera_cache, current->ray_origin, current->ray_direction, -BLB_OBJECT2D_CAMERA_SPACE_DEPTH,
                               &local_point, &world_point, &t)) {
          output->world2d = local_point;
          output->hit_point = world_point;
          HMM_Vec2 object_local = inverse_point_2d(object, local_point);
          output->local_position = HMM_V3(object_local.x, object_local.y, 0.0f);
          output->t = t;
          output->distance = HMM_LenV3(HMM_SubV3(world_point, current->ray_origin));
          found = true;
        }
      }
    } else if (fabsf(current->ray_direction.z) > 1e-7f) {
      const float t = -current->ray_origin.z / current->ray_direction.z;
      if (t >= 0.0f) {
        output->hit_point = HMM_AddV3(current->ray_origin, HMM_MulV3F(current->ray_direction, t));
        output->world2d = HMM_V2(output->hit_point.x, output->hit_point.y);
        HMM_Vec2 local = inverse_point_2d(object, output->world2d);
        output->local_position = HMM_V3(local.x, local.y, 0.0f);
        output->t = t;
        output->distance = HMM_LenV3(HMM_SubV3(output->hit_point, current->ray_origin));
        found = true;
      }
    }
  } else if (type == BLB_OBJECT_HANDLER_3D && scene->camera && scene->camera->camera_cache && scene->camera->camera_cache->valid) {
    BLB_Object3D *object = captured;
    HMM_Mat4 camera_world = HMM_InvGeneralM4(scene->camera->camera_cache->view);
    HMM_Vec4 normal4 = HMM_MulM4V4(camera_world, HMM_V4(0.0f, 0.0f, 1.0f, 0.0f));
    HMM_Vec3 plane_normal = HMM_NormV3(HMM_V3(normal4.x, normal4.y, normal4.z));
    HMM_Mat4 object_model = model_3d(object, scene->camera->camera_cache);
    HMM_Vec4 object_origin4 = HMM_MulM4V4(object_model, HMM_V4(0.0f, 0.0f, 0.0f, 1.0f));
    HMM_Vec3 plane_point = HMM_V3(object_origin4.x, object_origin4.y, object_origin4.z);

    if (drag && drag->active && drag->object == captured && drag->object_type == type) {
      HMM_Vec3 stored_point = HMM_V3(drag->plane_point[0], drag->plane_point[1], drag->plane_point[2]);
      HMM_Vec3 stored_normal = HMM_V3(drag->plane_normal[0], drag->plane_normal[1], drag->plane_normal[2]);
      if (HMM_LenV3(stored_normal) > 1e-7f) {
        plane_point = stored_point;
        plane_normal = HMM_NormV3(stored_normal);
      }
    }

    const float denominator = HMM_DotV3(plane_normal, current->ray_direction);
    if (fabsf(denominator) > 1e-7f) {
      const float t = HMM_DotV3(plane_normal, HMM_SubV3(plane_point, current->ray_origin)) / denominator;
      if (t >= 0.0f) {
        output->hit_point = HMM_AddV3(current->ray_origin, HMM_MulV3F(current->ray_direction, t));
        output->hit_normal = plane_normal;
        output->t = t;
        output->distance = HMM_LenV3(HMM_SubV3(output->hit_point, current->ray_origin));
        output->world2d = HMM_V2(output->hit_point.x, output->hit_point.y);
        HMM_Vec4 local4 = HMM_MulM4V4(HMM_InvGeneralM4(object_model), HMM_V4(output->hit_point.x, output->hit_point.y, output->hit_point.z, 1.0f));
        if (fabsf(local4.w) > 1e-7f)
          output->local_position = HMM_V3(local4.x / local4.w, local4.y / local4.w, local4.z / local4.w);
        found = true;
      }
    }
  }

  if (!found) {
    output->distance = -1.0f;
    output->t = -1.0f;
    output->hit_point = object_reference_hit(captured, type, scene).hit_point;
    output->world2d = object_reference_hit(captured, type, scene).world2d;
    output->hit_normal = HMM_V3(0.0f, 0.0f, 0.0f);
  }

  return found;
}

static void clear_handler_poll_state(BLB_Scene *scene) {
  if (!scene)
    return;
  for (int i = 0; i < scene->object2d_count; ++i) {
    BLB_Object2D *object = scene->objects2d[i];
    if (object && object->handler) {
      object->handler->down_buttons = 0;
      object->handler->pressed_buttons = 0;
      object->handler->released_buttons = 0;
    }
  }
  for (int i = 0; i < scene->object3d_count; ++i) {
    BLB_Object3D *object = scene->objects3d[i];
    if (object && object->handler) {
      object->handler->down_buttons = 0;
      object->handler->pressed_buttons = 0;
      object->handler->released_buttons = 0;
    }
  }
}

static void refresh_captured_poll_state(BLB_Handlers *handlers) {
  if (!handlers)
    return;
  for (size_t button = 0; button < BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    if (!handlers->mouse_buttons[button].down)
      continue;
    BLB_ObjectHandler *handler = object_handler(handlers->captured_objects[button], handlers->captured_object_types[button]);
    if (handler)
      handler->down_buttons |= (uint16_t)(1u << (unsigned)button);
  }
}

void BLB_HandlersProcessObjects(BLB_Handlers *handlers, BLB_Scene *scene) {
  if (!handlers || !scene || !handlers->initialized)
    return;

  clear_handler_poll_state(scene);
  refresh_captured_poll_state(handlers);

  if (handlers->focus_changed && !handlers->focused) {
    for (size_t button = 0; button < BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
      void *captured = handlers->captured_objects[button];
      const uint8_t type = handlers->captured_object_types[button];
      BLB_ObjectHandler *handler = object_handler(captured, type);
      if (!captured || !handler)
        continue;
      const ObjectHit hit = object_reference_hit(captured, type, scene);
      const BLB_ObjectEvent event =
          make_object_event(handlers, scene, &hit, BLB_OBJECT_HANDLER_CANCEL, (int)button, handlers->mouse_buttons[button].mods, true, false);
      handler->released_buttons |= (uint16_t)(1u << (unsigned)button);
      invoke(handler, BLB_OBJECT_HANDLER_CANCEL, &event);
      handler->captured_buttons &= (uint16_t) ~(1u << (unsigned)button);
      handlers->captured_objects[button] = NULL;
      handlers->captured_object_types[button] = 0;
      handlers->object_drags[button] = (BLB_ObjectDragState){0};
    }
  }

  const ObjectHit current = find_hit(handlers, scene);
  const ObjectHit hover_hit = handlers->mouse_inside && handlers->focused ? current : (ObjectHit){0};
  void *previous_hovered = handlers->hovered_object;
  const uint8_t previous_hovered_type = handlers->hovered_object_type;
  const bool hover_changed =
      previous_hovered != (hover_hit.hit ? hover_hit.object : NULL) || previous_hovered_type != (hover_hit.hit ? hover_hit.type : 0);

  if (hover_changed) {
    if (previous_hovered) {
      BLB_ObjectHandler *previous_handler = object_handler(previous_hovered, previous_hovered_type);
      if (previous_handler) {
        previous_handler->hovered = false;
        if (previous_handler->enabled) {
          const ObjectHit previous_hit = object_reference_hit(previous_hovered, previous_hovered_type, scene);
          const BLB_ObjectEvent event = make_object_event(handlers, scene, &previous_hit, BLB_OBJECT_HANDLER_HOVER_LEAVE, -1, 0, false, false);
          invoke(previous_handler, BLB_OBJECT_HANDLER_HOVER_LEAVE, &event);
        }
      }
    }

    if (hover_hit.hit && hover_hit.handler && hover_hit.handler->enabled) {
      hover_hit.handler->hovered = true;
      const BLB_ObjectEvent event = make_object_event(handlers, scene, &hover_hit, BLB_OBJECT_HANDLER_HOVER_ENTER, -1, 0, false, true);
      invoke(hover_hit.handler, BLB_OBJECT_HANDLER_HOVER_ENTER, &event);
    }

    handlers->hovered_object = hover_hit.hit ? hover_hit.object : NULL;
    handlers->hovered_object_type = hover_hit.hit ? hover_hit.type : 0;
  } else if (hover_hit.hit && hover_hit.handler && hover_hit.handler->enabled) {
    hover_hit.handler->hovered = true;
  }

  for (int button = 0; button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];
    if (!state->pressed || !hover_hit.hit || !hover_hit.handler || !hover_hit.handler->enabled)
      continue;

    BLB_ObjectHandler *handler = hover_hit.handler;
    const uint16_t mask = (uint16_t)(1u << (unsigned)button);
    handlers->captured_objects[(size_t)button] = hover_hit.object;
    handlers->captured_object_types[(size_t)button] = hover_hit.type;
    handler->captured_buttons |= mask;
    handler->pressed_buttons |= mask;

    BLB_ObjectDragState *drag = &handlers->object_drags[(size_t)button];
    *drag = (BLB_ObjectDragState){0};
    drag->active = true;
    drag->object = hover_hit.object;
    drag->object_type = hover_hit.type;
    drag->press_mouse_x = handlers->mouse_x;
    drag->press_mouse_y = handlers->mouse_y;
    drag->start_point[0] = hover_hit.world2d.x;
    drag->start_point[1] = hover_hit.world2d.y;
    drag->start_point[2] = hover_hit.hit_point.z;
    drag->last_point[0] = drag->start_point[0];
    drag->last_point[1] = drag->start_point[1];
    drag->last_point[2] = drag->start_point[2];
    initialize_drag_plane(scene, &hover_hit, drag);

    const BLB_ObjectEvent event = make_object_event(handlers, scene, &hover_hit, BLB_OBJECT_HANDLER_PRESS, button, state->mods, true, true);
    invoke(handler, BLB_OBJECT_HANDLER_PRESS, &event);
  }

  refresh_captured_poll_state(handlers);

  for (int button = 0; button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];
    if (!state->down)
      continue;

    void *captured = handlers->captured_objects[(size_t)button];
    const uint8_t type = handlers->captured_object_types[(size_t)button];
    if (!captured)
      continue;

    BLB_ObjectHandler *handler = object_handler(captured, type);
    if (!handler || !handler->enabled)
      continue;

    ObjectHit capture_hit = {0};
    BLB_ObjectDragState *drag = &handlers->object_drags[(size_t)button];
    const bool has_drag_point = capture_object_hit(handlers, scene, &current, captured, type, drag, &capture_hit);
    const bool inside = hover_hit.hit && hover_hit.object == captured && hover_hit.type == type;
    const double dx = handlers->mouse_x - drag->press_mouse_x;
    const double dy = handlers->mouse_y - drag->press_mouse_y;
    const bool crossed_threshold = dx * dx + dy * dy >= BLB_HANDLER_DRAG_THRESHOLD * BLB_HANDLER_DRAG_THRESHOLD;

    if (drag->active && !drag->started && crossed_threshold && has_drag_point) {
      drag->started = true;
      const BLB_ObjectEvent event =
          make_object_event(handlers, scene, &capture_hit, BLB_OBJECT_HANDLER_DRAG_START, button, state->mods, true, inside);
      invoke(handler, BLB_OBJECT_HANDLER_DRAG_START, &event);
    }

    if (drag->active && drag->started && has_drag_point) {
      const BLB_ObjectEvent event = make_object_event(handlers, scene, &capture_hit, BLB_OBJECT_HANDLER_DRAG, button, state->mods, true, inside);
      invoke(handler, BLB_OBJECT_HANDLER_DRAG, &event);
    }

    if (has_drag_point) {
      drag->last_point[0] = capture_hit.world2d.x;
      drag->last_point[1] = capture_hit.world2d.y;
      drag->last_point[2] = capture_hit.hit_point.z;
    }

    if (has_drag_point) {
      BLB_ObjectEvent event = make_object_event(handlers, scene, &capture_hit, BLB_OBJECT_HANDLER_HOLD, button, state->mods, true, inside);
      event.dragging = drag->started;
      invoke(handler, BLB_OBJECT_HANDLER_HOLD, &event);
    } else {
      ObjectHit unavailable = object_reference_hit(captured, type, scene);
      unavailable.distance = -1.0f;
      unavailable.t = -1.0f;
      BLB_ObjectEvent event = make_object_event(handlers, scene, &unavailable, BLB_OBJECT_HANDLER_HOLD, button, state->mods, true, inside);
      event.dragging = drag->started;
      invoke(handler, BLB_OBJECT_HANDLER_HOLD, &event);
    }
  }

  for (int button = 0; button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];
    if (!state->released)
      continue;

    void *captured = handlers->captured_objects[(size_t)button];
    const uint8_t type = handlers->captured_object_types[(size_t)button];
    if (!captured)
      continue;

    BLB_ObjectHandler *handler = object_handler(captured, type);
    const uint16_t mask = (uint16_t)(1u << (unsigned)button);
    if (!handler || !handler->enabled) {
      handlers->captured_objects[(size_t)button] = NULL;
      handlers->captured_object_types[(size_t)button] = 0;
      handlers->object_drags[(size_t)button] = (BLB_ObjectDragState){0};
      continue;
    }

    const bool inside = hover_hit.hit && hover_hit.object == captured && hover_hit.type == type;
    ObjectHit release_hit = {0};
    BLB_ObjectDragState *drag = &handlers->object_drags[(size_t)button];
    capture_object_hit(handlers, scene, &current, captured, type, drag, &release_hit);
    release_hit.hit = true;
    release_hit.object = captured;
    release_hit.handler = handler;
    release_hit.type = type;
    handler->released_buttons |= mask;

    if (drag->active && drag->started) {
      const BLB_ObjectEvent event = make_object_event(handlers, scene, &release_hit, BLB_OBJECT_HANDLER_DRAG_END, button, state->mods, true, inside);
      invoke(handler, BLB_OBJECT_HANDLER_DRAG_END, &event);
    }

    const BLB_ObjectEvent release_event =
        make_object_event(handlers, scene, &release_hit, BLB_OBJECT_HANDLER_RELEASE, button, state->mods, true, inside);
    invoke(handler, BLB_OBJECT_HANDLER_RELEASE, &release_event);

    if (inside && (!drag->active || !drag->started)) {
      ObjectHit click_hit = hover_hit;
      click_hit.object = captured;
      click_hit.handler = handler;
      click_hit.type = type;
      const BLB_ObjectEvent event = make_object_event(handlers, scene, &click_hit, BLB_OBJECT_HANDLER_CLICK, button, state->mods, true, true);
      invoke(handler, BLB_OBJECT_HANDLER_CLICK, &event);
    }

    handler->captured_buttons &= (uint16_t)~mask;
    handlers->captured_objects[(size_t)button] = NULL;
    handlers->captured_object_types[(size_t)button] = 0;
    handlers->object_drags[(size_t)button] = (BLB_ObjectDragState){0};
  }
}
