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

static double event_time(void) { return glfwGetTime(); }

void BLB_ObjectHandlerInit(BLB_ObjectHandler *handler) {
  if (!handler)
    return;

  *handler = (BLB_ObjectHandler){0};
  handler->enabled = true;
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

static bool point_in_polygon(const BLB_Polygon2D *polygon, HMM_Vec2 point) {
  if (!polygon || !polygon->vertices || polygon->vertex_count < 3)
    return false;

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

static bool window_mouse_coordinates(const BLB_Handlers *handlers, float *screen_x, float *screen_y, float *width, float *height) {
  if (!handlers || !handlers->window || !screen_x || !screen_y || !width || !height)
    return false;

  int window_width = 0;
  int window_height = 0;

  glfwGetWindowSize(handlers->window, &window_width, &window_height);

  if (window_width <= 0 || window_height <= 0)
    return false;

  *width = (float)window_width;
  *height = (float)window_height;
  *screen_x = (float)handlers->mouse_x;
  *screen_y = (float)window_height - (float)handlers->mouse_y;

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
  *screen_y = (float)framebuffer_height - (float)(handlers->mouse_y * (double)framebuffer_height / (double)window_height);

  return true;
}

static HMM_Mat4 model_3d(const BLB_Object3D *object) {
  const HMM_Mat4 rx = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.x), HMM_V3(1.0f, 0.0f, 0.0f));
  const HMM_Mat4 ry = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.y), HMM_V3(0.0f, 1.0f, 0.0f));
  const HMM_Mat4 rz = HMM_Rotate_RH(HMM_AngleDeg(object->rotation.z), HMM_V3(0.0f, 0.0f, 1.0f));
  const HMM_Mat4 rotation = HMM_MulM4(rz, HMM_MulM4(ry, rx));

  return HMM_MulM4(HMM_Translate(object->position), HMM_MulM4(rotation, HMM_Scale(object->scale)));
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

      if (t < 0.0f)
        return false;

      *world2d = HMM_V2(origin->x + direction->x * t, origin->y + direction->y * t);
    } else {
      *world2d = HMM_V2(origin->x, origin->y);
    }
  }

  return true;
}

typedef struct {
  bool valid;
  HMM_Vec3 origin;
  HMM_Vec3 direction;
  HMM_Vec2 world2d;
  float screen_x;
  float screen_y;
} RayQuery;

static bool build_ray_query(BLB_Handlers *handlers, BLB_Scene *scene, RayQuery *query) {
  if (!handlers || !scene || !scene->camera || !scene->camera->camera_cache || !query)
    return false;

  memset(query, 0, sizeof(*query));

  float window_width = 0.0f;
  float window_height = 0.0f;
  float framebuffer_x = 0.0f;
  float framebuffer_y = 0.0f;
  float framebuffer_width = 0.0f;
  float framebuffer_height = 0.0f;

  if (!window_mouse_coordinates(handlers, &query->screen_x, &query->screen_y, &window_width, &window_height))
    return false;

  if (!framebuffer_mouse_coordinates(handlers, &framebuffer_x, &framebuffer_y, &framebuffer_width, &framebuffer_height))
    return false;

  if (!screen_ray(scene->camera->camera_cache, &query->origin, &query->direction, &query->world2d, framebuffer_width, framebuffer_height,
                  framebuffer_x, framebuffer_y))
    return false;

  query->valid = true;

  return true;
}

typedef struct {
  void *object;
  BLB_ObjectHandler *handler;
  uint8_t type;
  bool hit;
  bool screen_space;
  unsigned short layer;
  float distance;
  float t;
  HMM_Vec2 world2d;
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

  if (candidate->layer != best->layer)
    return candidate->layer > best->layer;

  if (candidate->type == best->type)
    return candidate->distance < best->distance;

  if (candidate->screen_space != best->screen_space)
    return candidate->screen_space;

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

    if (!object || !object->visible || !object->handler || !object->handler->enabled || !object->polygon || !object->polygon->vertices ||
        object->polygon->vertex_count == 0)
      continue;

    if (object->handler->object_geometry != object->polygon)
      cache_bounds_2d(object->handler, object->polygon);

    const HMM_Vec2 point = object->screen_space ? HMM_V2(ray.screen_x, ray.screen_y) : ray.world2d;
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

    if (!object->screen_space) {
      if (fabsf(ray.direction.z) < 1e-7f)
        continue;

      hit_t = -ray.origin.z / ray.direction.z;

      if (hit_t < 0.0f)
        continue;

      const HMM_Vec3 world_hit = HMM_AddV3(ray.origin, HMM_MulV3F(ray.direction, hit_t));
      distance = HMM_LenV3(HMM_SubV3(world_hit, ray.origin));
    }

    ObjectHit candidate = {.object = object,
                           .handler = object->handler,
                           .type = BLB_OBJECT_HANDLER_2D,
                           .hit = true,
                           .screen_space = object->screen_space,
                           .layer = object->layer,
                           .distance = distance,
                           .t = hit_t,
                           .world2d = ray.world2d,
                           .hit_point = HMM_V3(point.x, point.y, 0.0f),
                           .hit_normal = HMM_V3(0.0f, 0.0f, 1.0f),
                           .ray_origin = ray.origin,
                           .ray_direction = ray.direction};

    if (better_hit(&candidate, &best))
      best = candidate;
  }

  for (int i = 0; i < scene->object3d_count; ++i) {
    BLB_Object3D *object = scene->objects3d[i];

    if (!object || !object->visible || !object->handler || !object->handler->enabled || !object->polygon || !object->polygon->vertices ||
        object->polygon->vertex_count == 0)
      continue;

    if (fabsf(object->scale.x) < 1e-7f || fabsf(object->scale.y) < 1e-7f || fabsf(object->scale.z) < 1e-7f)
      continue;

    if (object->handler->object_geometry != object->polygon)
      cache_bounds_3d(object->handler, object->polygon);

    const HMM_Mat4 model = model_3d(object);
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

    if (object->polygon->indices && object->polygon->index_count >= 3 && object->polygon->index_count % 3 == 0) {
      for (size_t index = 0; index < object->polygon->index_count; index += 3u) {
        const unsigned int ia = object->polygon->indices[index];
        const unsigned int ib = object->polygon->indices[index + 1u];
        const unsigned int ic = object->polygon->indices[index + 2u];

        if (ia >= object->polygon->vertex_count || ib >= object->polygon->vertex_count || ic >= object->polygon->vertex_count)
          continue;

        float triangle_t = 0.0f;
        HMM_Vec3 triangle_normal = HMM_V3(0.0f, 0.0f, 0.0f);

        if (!ray_triangle(local_origin, local_direction, object->polygon->vertices[ia], object->polygon->vertices[ib], object->polygon->vertices[ic],
                          &triangle_t, &triangle_normal))
          continue;

        if (triangle_t < nearest_t) {
          nearest_t = triangle_t;
          nearest_normal = triangle_normal;
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
                           .screen_space = false,
                           .layer = object->layer,
                           .distance = distance,
                           .t = nearest_t,
                           .world2d = ray.world2d,
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
  event.ray_origin = hit ? hit->ray_origin : HMM_V3(0.0f, 0.0f, 0.0f);
  event.ray_direction = hit ? hit->ray_direction : HMM_V3(0.0f, 0.0f, -1.0f);
  event.hit_point = hit ? hit->hit_point : HMM_V3(0.0f, 0.0f, 0.0f);
  event.hit_normal = hit ? hit->hit_normal : HMM_V3(0.0f, 0.0f, 0.0f);
  event.distance = hit ? hit->distance : -1.0f;
  event.t = hit ? hit->t : -1.0f;
  event.hovered = hit && hit->handler ? hit->handler->hovered : false;
  event.captured = captured;
  event.inside = inside;

  return event;
}

static void invoke(BLB_ObjectHandler *handler, BLB_ObjectHandlerEventType type, const BLB_ObjectEvent *event) {
  const BLB_ObjectHandlerCallback callback = handler_callback(type, handler);

  if (callback)
    callback(event, handler->userdata);
}

void BLB_HandlersProcessObjects(BLB_Handlers *handlers, BLB_Scene *scene) {
  if (!handlers || !scene || !handlers->initialized)
    return;

  bool interaction = !handlers->object_hover_initialized || handlers->mouse_moved;

  for (size_t button = 0; button < BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    const BLB_MouseButtonState *state = &handlers->mouse_buttons[button];

    if (state->pressed || state->released || state->down) {
      interaction = true;
      break;
    }
  }

  if (!interaction)
    return;

  const ObjectHit current = find_hit(handlers, scene);
  const ObjectHit hover_hit = handlers->mouse_inside ? current : (ObjectHit){0};
  void *previous_hovered = handlers->hovered_object;
  const uint8_t previous_hovered_type = handlers->hovered_object_type;

  if (!handlers->object_hover_initialized || handlers->mouse_moved) {
    if (previous_hovered && (!hover_hit.hit || hover_hit.object != previous_hovered || hover_hit.type != previous_hovered_type)) {
      BLB_ObjectHandler *previous_handler =
          previous_hovered_type == BLB_OBJECT_HANDLER_2D ? ((BLB_Object2D *)previous_hovered)->handler : ((BLB_Object3D *)previous_hovered)->handler;

      if (previous_handler && previous_handler->enabled)
        previous_handler->hovered = false;
    }

    if (hover_hit.hit)
      hover_hit.handler->hovered = true;

    handlers->hovered_object = hover_hit.hit ? hover_hit.object : NULL;
    handlers->hovered_object_type = hover_hit.hit ? hover_hit.type : 0;
    handlers->object_hover_initialized = true;
  }

  for (int button = 0; button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];

    if (!state->pressed)
      continue;

    if (!current.hit || !current.handler)
      continue;

    handlers->captured_objects[(size_t)button] = current.object;
    handlers->captured_object_types[(size_t)button] = current.type;
    current.handler->captured_buttons |= (uint16_t)(1u << (unsigned)button);
  }

  for (int button = 0; button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];

    if (!state->down)
      continue;

    void *captured = handlers->captured_objects[(size_t)button];
    const uint8_t type = handlers->captured_object_types[(size_t)button];

    if (!captured)
      continue;

    BLB_ObjectHandler *handler = type == BLB_OBJECT_HANDLER_2D ? ((BLB_Object2D *)captured)->handler : ((BLB_Object3D *)captured)->handler;

    if (!handler || !handler->enabled)
      continue;

    ObjectHit capture_hit = current;
    capture_hit.object = captured;
    capture_hit.handler = handler;
    capture_hit.type = type;

    const bool inside = current.hit && current.object == captured && current.type == type;

    if (!inside) {
      capture_hit.hit = false;
      capture_hit.distance = -1.0f;
      capture_hit.t = -1.0f;
      capture_hit.hit_point = HMM_V3(0.0f, 0.0f, 0.0f);
      capture_hit.hit_normal = HMM_V3(0.0f, 0.0f, 0.0f);
    }

    const BLB_ObjectEvent event = make_object_event(handlers, scene, &capture_hit, BLB_OBJECT_HANDLER_HOLD, button, state->mods, true, inside);
    invoke(handler, BLB_OBJECT_HANDLER_HOLD, &event);
  }

  for (int button = 0; button < (int)BLB_HANDLER_MOUSE_BUTTON_COUNT; ++button) {
    BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];

    if (!state->released)
      continue;

    void *captured = handlers->captured_objects[(size_t)button];
    const uint8_t type = handlers->captured_object_types[(size_t)button];

    if (!captured)
      continue;

    BLB_ObjectHandler *handler = type == BLB_OBJECT_HANDLER_2D ? ((BLB_Object2D *)captured)->handler : ((BLB_Object3D *)captured)->handler;

    if (!handler || !handler->enabled)
      goto clear_capture;

    const bool inside = current.hit && current.object == captured && current.type == type;

    if (inside) {
      ObjectHit click_hit = current;
      click_hit.object = captured;
      click_hit.handler = handler;
      click_hit.type = type;

      const BLB_ObjectEvent event = make_object_event(handlers, scene, &click_hit, BLB_OBJECT_HANDLER_CLICK, button, state->mods, true, true);

      invoke(handler, BLB_OBJECT_HANDLER_CLICK, &event);
    }

    handler->captured_buttons &= (uint16_t) ~(1u << (unsigned)button);

  clear_capture:
    handlers->captured_objects[(size_t)button] = NULL;
    handlers->captured_object_types[(size_t)button] = 0;
  }
}
