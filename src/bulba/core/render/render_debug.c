#include "render_debug.h"
#include "bulba/core/utils/config.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef BLB_OBJECT2D_CAMERA_SPACE_DEPTH
#define BLB_OBJECT2D_CAMERA_SPACE_DEPTH 1.0f
#endif

static BLB_Vec3 physics_debug_transform_point(BLB_Vec3 local, BLB_Vec3 position,
                                              BLB_Quat rotation) {
  return BLB_AddV3(position, BLB_RotateV3Q(local, rotation));
}

static bool physics_debug_reserve(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                  size_t *index_capacity, size_t vertices_needed,
                                  size_t indices_needed) {
  if (!polygon || !vertex_capacity || !index_capacity)
    return false;

  if (polygon->vertex_count + vertices_needed > *vertex_capacity) {
    size_t capacity = *vertex_capacity ? *vertex_capacity : 128;

    while (capacity < polygon->vertex_count + vertices_needed) {
      if (capacity > SIZE_MAX / 2)
        return false;
      capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof(*polygon->vertices))
      return false;

    BLB_Vec3 *vertices = realloc(polygon->vertices, capacity * sizeof(*vertices));

    if (!vertices)
      return false;
    polygon->vertices = vertices;
    *vertex_capacity = capacity;
  }

  if (polygon->index_count + indices_needed > *index_capacity) {
    size_t capacity = *index_capacity ? *index_capacity : 384;

    while (capacity < polygon->index_count + indices_needed) {
      if (capacity > SIZE_MAX / 2)
        return false;
      capacity *= 2;
    }

    if (capacity > SIZE_MAX / sizeof(*polygon->indices))
      return false;

    unsigned int *indices = realloc(polygon->indices, capacity * sizeof(*indices));

    if (!indices)
      return false;
    polygon->indices = indices;
    *index_capacity = capacity;
  }

  return true;
}

static bool physics_debug_add_line(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                   size_t *index_capacity, BLB_Vec3 a, BLB_Vec3 b,
                                   const BLB_CameraCache *camera_cache) {
  BLB_Vec3 delta = BLB_SubV3(b, a);
  float length = BLB_LenV3(delta);

  if (length <= 0.00001f)
    return true;

  if (!physics_debug_reserve(polygon, vertex_capacity, index_capacity, 4, 6))
    return false;

  BLB_Vec3 direction = BLB_MulV3F(delta, 1.0f / length);
  BLB_Vec3 midpoint = BLB_MulV3F(BLB_AddV3(a, b), 0.5f);
  BLB_Vec3 camera_position = BLB_V3(0.0f, 0.0f, 0.0f);
  BLB_Vec3 camera_up = BLB_V3(0.0f, 1.0f, 0.0f);
  BLB_Vec3 camera_right = BLB_V3(1.0f, 0.0f, 0.0f);

  if (camera_cache && camera_cache->valid) {
    BLB_Mat4 camera_world = BLB_InvGeneralM4(camera_cache->view);
    BLB_Vec4 position4 = BLB_MulM4V4(camera_world, BLB_V4(0.0f, 0.0f, 0.0f, 1.0f));
    BLB_Vec4 up4 = BLB_MulM4V4(camera_world, BLB_V4(0.0f, 1.0f, 0.0f, 0.0f));
    BLB_Vec4 right4 = BLB_MulM4V4(camera_world, BLB_V4(1.0f, 0.0f, 0.0f, 0.0f));

    if (fabsf(position4.w) > 1e-7f)
      camera_position = BLB_V3(position4.x / position4.w, position4.y / position4.w,
                               position4.z / position4.w);
    camera_up = BLB_NormV3(BLB_V3(up4.x, up4.y, up4.z));
    camera_right = BLB_NormV3(BLB_V3(right4.x, right4.y, right4.z));
  }

  BLB_Vec3 view_direction = BLB_NormV3(BLB_SubV3(camera_position, midpoint));
  BLB_Vec3 side = BLB_Cross(direction, view_direction);

  if (BLB_LenV3(side) < 1e-6f)
    side = BLB_Cross(direction, camera_up);

  if (BLB_LenV3(side) < 1e-6f)
    side = BLB_Cross(direction, camera_right);

  if (BLB_LenV3(side) < 1e-6f)
    return false;

  const float camera_distance = BLB_LenV3(BLB_SubV3(camera_position, midpoint));
  const float half_width = fmaxf(0.0005f, fminf(0.008f, camera_distance * 0.0012f));
  side = BLB_MulV3F(BLB_NormV3(side), half_width);
  BLB_Vec3 depth_offset = BLB_MulV3F(view_direction, half_width * 0.2f);
  a = BLB_AddV3(a, depth_offset);
  b = BLB_AddV3(b, depth_offset);

  const size_t base = polygon->vertex_count;
  polygon->vertices[base] = BLB_SubV3(a, side);
  polygon->vertices[base + 1] = BLB_SubV3(b, side);
  polygon->vertices[base + 2] = BLB_AddV3(b, side);
  polygon->vertices[base + 3] = BLB_AddV3(a, side);
  polygon->vertex_count += 4;

  polygon->indices[polygon->index_count] = (unsigned int)base;
  polygon->indices[polygon->index_count + 1] = (unsigned int)(base + 1);
  polygon->indices[polygon->index_count + 2] = (unsigned int)(base + 2);
  polygon->indices[polygon->index_count + 3] = (unsigned int)base;
  polygon->indices[polygon->index_count + 4] = (unsigned int)(base + 2);
  polygon->indices[polygon->index_count + 5] = (unsigned int)(base + 3);
  polygon->index_count += 6;

  return true;
}

static BLB_Vec3 physics_debug_half_extents(const BLB_Physics3DShape *shape,
                                           BLB_Vec3 *local_center) {
  BLB_Vec3 half = BLB_V3(0.5f, 0.5f, 0.5f);

  if (local_center)
    *local_center = BLB_V3(0.0f, 0.0f, 0.0f);

  if (!shape)
    return half;

  switch (shape->type) {
  case BLB_PHYSICS3D_SHAPE_BOX:
    half = shape->data.box.scale;
    break;
  case BLB_PHYSICS3D_SHAPE_SPHERE:
    half = BLB_V3(shape->data.sphere.radius, shape->data.sphere.radius,
                  shape->data.sphere.radius);
    break;
  case BLB_PHYSICS3D_SHAPE_CAPSULE:
    half = BLB_V3(shape->data.capsule.radius,
                  shape->data.capsule.height + shape->data.capsule.radius,
                  shape->data.capsule.radius);
    break;
  case BLB_PHYSICS3D_SHAPE_CONVEX_HULL: {
    const BLB_Vec3 *vertices = shape->data.convex_hull.vertices;
    size_t count = shape->data.convex_hull.vertex_count;

    if (vertices && count) {
      BLB_Vec3 min_v = vertices[0], max_v = vertices[0];

      for (size_t i = 1; i < count; ++i) {
        min_v.x = fminf(min_v.x, vertices[i].x);
        min_v.y = fminf(min_v.y, vertices[i].y);
        min_v.z = fminf(min_v.z, vertices[i].z);
        max_v.x = fmaxf(max_v.x, vertices[i].x);
        max_v.y = fmaxf(max_v.y, vertices[i].y);
        max_v.z = fmaxf(max_v.z, vertices[i].z);
      }

      half = BLB_MulV3F(BLB_SubV3(max_v, min_v), 0.5f);

      if (local_center)
        *local_center = BLB_MulV3F(BLB_AddV3(max_v, min_v), 0.5f);
    }
    break;
  }
  case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH: {
    const BLB_Vec3 *vertices = shape->data.triangle_mesh.vertices;
    size_t count = shape->data.triangle_mesh.vertex_count;

    if (vertices && count) {
      BLB_Vec3 min_v = vertices[0], max_v = vertices[0];

      for (size_t i = 1; i < count; ++i) {
        min_v.x = fminf(min_v.x, vertices[i].x);
        min_v.y = fminf(min_v.y, vertices[i].y);
        min_v.z = fminf(min_v.z, vertices[i].z);
        max_v.x = fmaxf(max_v.x, vertices[i].x);
        max_v.y = fmaxf(max_v.y, vertices[i].y);
        max_v.z = fmaxf(max_v.z, vertices[i].z);
      }
      half = BLB_MulV3F(BLB_SubV3(max_v, min_v), 0.5f);

      if (local_center)
        *local_center = BLB_MulV3F(BLB_AddV3(max_v, min_v), 0.5f);
    }
    break;
  }
  default:
    break;
  }
  half.x = fmaxf(fabsf(half.x), 0.01f);
  half.y = fmaxf(fabsf(half.y), 0.01f);
  half.z = fmaxf(fabsf(half.z), 0.01f);

  return half;
}

static bool physics_debug_draw_box(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                   size_t *index_capacity, BLB_Vec3 half,
                                   BLB_Vec3 position, BLB_Quat rotation,
                                   const BLB_CameraCache *camera_cache) {
  BLB_Vec3 corners[8];

  for (unsigned int i = 0; i < 8; ++i) {
    BLB_Vec3 local = BLB_V3((i & 1u) ? half.x : -half.x, (i & 2u) ? half.y : -half.y,
                            (i & 4u) ? half.z : -half.z);
    corners[i] = physics_debug_transform_point(local, position, rotation);
  }

  for (unsigned int i = 0; i < 8; ++i) {
    for (unsigned int axis = 0; axis < 3; ++axis) {
      if ((i & (1u << axis)) != 0u)
        continue;

      unsigned int other = i | (1u << axis);

      if (!physics_debug_add_line(polygon, vertex_capacity, index_capacity, corners[i],
                                  corners[other], camera_cache))
        return false;
    }
  }

  return true;
}

static bool physics_debug_draw_sphere(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                      size_t *index_capacity, float radius,
                                      float half_height, bool capsule, BLB_Vec3 position,
                                      BLB_Quat rotation,
                                      const BLB_CameraCache *camera_cache) {
  const unsigned int segments = 16;
  const float tau = (float)(BLB_PI * 2.0);
  float levels[2] = {0.0f, 0.0f};
  size_t level_count = 1;

  if (capsule) {
    levels[0] = -half_height;
    levels[1] = half_height;
    level_count = 2;
  }

  for (size_t level = 0; level < level_count; ++level) {
    for (unsigned int i = 0; i < segments; ++i) {
      float a0 = tau * (float)i / (float)segments;
      float a1 = tau * (float)(i + 1) / (float)segments;
      BLB_Vec3 p0 = BLB_V3(radius * cosf(a0), levels[level], radius * sinf(a0));
      BLB_Vec3 p1 = BLB_V3(radius * cosf(a1), levels[level], radius * sinf(a1));

      if (!physics_debug_add_line(polygon, vertex_capacity, index_capacity,
                                  physics_debug_transform_point(p0, position, rotation),
                                  physics_debug_transform_point(p1, position, rotation),
                                  camera_cache))
        return false;
    }
  }

  const BLB_Vec3 ring_points[4] = {BLB_V3(radius, 0.0f, 0.0f), BLB_V3(0.0f, 0.0f, radius),
                                   BLB_V3(-radius, 0.0f, 0.0f),
                                   BLB_V3(0.0f, 0.0f, -radius)};

  if (capsule) {
    for (size_t i = 0; i < 4; ++i) {
      BLB_Vec3 low = BLB_V3(ring_points[i].x, -half_height, ring_points[i].z);
      BLB_Vec3 high = BLB_V3(ring_points[i].x, half_height, ring_points[i].z);

      if (!physics_debug_add_line(polygon, vertex_capacity, index_capacity,
                                  physics_debug_transform_point(low, position, rotation),
                                  physics_debug_transform_point(high, position, rotation),
                                  camera_cache))
        return false;
    }

    for (int sign = -1; sign <= 1; sign += 2) {
      for (unsigned int plane = 0; plane < 2; ++plane) {
        for (unsigned int i = 0; i < 8; ++i) {
          float a0 = (float)BLB_PI * 0.5f * (float)i / 8.0f;
          float a1 = (float)BLB_PI * 0.5f * (float)(i + 1u) / 8.0f;
          BLB_Vec3 p0, p1;

          if (plane == 0) {
            p0 = BLB_V3(radius * cosf(a0),
                        (float)sign * (half_height + radius * sinf(a0)), 0.0f);
            p1 = BLB_V3(radius * cosf(a1),
                        (float)sign * (half_height + radius * sinf(a1)), 0.0f);
          } else {
            p0 = BLB_V3(0.0f, (float)sign * (half_height + radius * sinf(a0)),
                        radius * cosf(a0));
            p1 = BLB_V3(0.0f, (float)sign * (half_height + radius * sinf(a1)),
                        radius * cosf(a1));
          }

          if (!physics_debug_add_line(
                  polygon, vertex_capacity, index_capacity,
                  physics_debug_transform_point(p0, position, rotation),
                  physics_debug_transform_point(p1, position, rotation), camera_cache))
            return false;
        }
      }
    }
  } else {
    for (unsigned int plane = 0; plane < 2; ++plane) {
      for (unsigned int i = 0; i < segments; ++i) {
        float a0 = tau * (float)i / (float)segments;
        float a1 = tau * (float)(i + 1) / (float)segments;
        BLB_Vec3 p0, p1;

        if (plane == 0) {
          p0 = BLB_V3(radius * cosf(a0), radius * sinf(a0), 0.0f);
          p1 = BLB_V3(radius * cosf(a1), radius * sinf(a1), 0.0f);
        } else {
          p0 = BLB_V3(0.0f, radius * cosf(a0), radius * sinf(a0));
          p1 = BLB_V3(0.0f, radius * cosf(a1), radius * sinf(a1));
        }

        if (!physics_debug_add_line(polygon, vertex_capacity, index_capacity,
                                    physics_debug_transform_point(p0, position, rotation),
                                    physics_debug_transform_point(p1, position, rotation),
                                    camera_cache))
          return false;
      }
    }
  }

  return true;
}

void blb_render_draw_physics_debug_colliders3d(BLB_Scene *scene, BLB_Renderer *renderer,
                                               const BLB_CameraCache *camera_cache) {
  BLB_Physics3DWorld *world = scene ? scene->physics_world3d : NULL;

  if (!world || (!BLB_DEBUG && !BLB_Physics3D_WorldGetDebugDraw(world)) || !renderer ||
      !camera_cache || !camera_cache->valid)
    return;

  BLB_Polygon3D polygon = {0};
  polygon.dynamic_vertices = true;
  size_t vertex_capacity = 0;
  size_t index_capacity = 0;
  size_t count = BLB_Physics3D_WorldGetDebugColliderCount(world);

  for (size_t i = 0; i < count; ++i) {
    BLB_Physics3DDebugCollider collider;

    if (!BLB_Physics3D_WorldGetDebugCollider(world, i, &collider) || !collider.shape ||
        !collider.active)
      continue;

    const BLB_Physics3DShape *shape = collider.shape;
    bool ok = true;

    if (shape->type == BLB_PHYSICS3D_SHAPE_SPHERE) {
      ok = physics_debug_draw_sphere(&polygon, &vertex_capacity, &index_capacity,
                                     shape->data.sphere.radius, 0.0f, false,
                                     collider.position, collider.rotation, camera_cache);
    } else if (shape->type == BLB_PHYSICS3D_SHAPE_CAPSULE) {
      ok = physics_debug_draw_sphere(&polygon, &vertex_capacity, &index_capacity,
                                     shape->data.capsule.radius,
                                     shape->data.capsule.height, true, collider.position,
                                     collider.rotation, camera_cache);
    } else if (shape->type != BLB_PHYSICS3D_SHAPE_PLANE &&
               shape->type != BLB_PHYSICS3D_SHAPE_COMPOUND) {

      BLB_Vec3 local_center = BLB_V3(0.0f, 0.0f, 0.0f);
      BLB_Vec3 center = collider.position;
      BLB_Vec3 half = physics_debug_half_extents(shape, &local_center);

      if (BLB_LenV3(local_center) > 0.000001f)
        center = physics_debug_transform_point(local_center, collider.position,
                                               collider.rotation);

      ok = physics_debug_draw_box(&polygon, &vertex_capacity, &index_capacity, half,
                                  center, collider.rotation, camera_cache);
    }

    if (!ok)
      break;
  }

  if (polygon.vertex_count && polygon.index_count) {
    BLB_Mat4 view_projection = BLB_MulM4(camera_cache->projection, camera_cache->view);
    float model_rows[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float color[4] = {0};
    BLB_RenderMaterial material = {0};
    color[0] = 0.05f;
    color[1] = 1.0f;
    color[2] = 0.24f;
    color[3] = 1.0f;
    material.lighting_enabled = false;
    material.unlit = true;
    material.depth_enabled = false;
    material.depth_write = false;
    material.double_sided = true;
    material.alpha_mode = BLB_ALPHA_OPAQUE;
    material.render_mode = BLB_RENDER_ADDITIVE;
    BLB_RendererDrawPolygon3D(renderer, &polygon, &view_projection.Elements[0][0],
                              model_rows, color[0], color[1], color[2], color[3],
                              &material, NULL);
  }

  free(polygon.vertices);
  free(polygon.indices);
  free(polygon.normals);
}

static BLB_Vec3 physics_debug_transform_point2d(BLB_Vec2 local, BLB_Vec2 position,
                                                float rotation) {
  float c = cosf(rotation);
  float s = sinf(rotation);

  return BLB_V3(position.x + local.x * c - local.y * s,
                position.y + local.x * s + local.y * c, 0.0f);
}

static bool physics_debug_draw_segment2d(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                         size_t *index_capacity, BLB_Vec2 a, BLB_Vec2 b,
                                         BLB_Vec2 position, float rotation,
                                         const BLB_CameraCache *camera_cache) {
  return physics_debug_add_line(polygon, vertex_capacity, index_capacity,
                                physics_debug_transform_point2d(a, position, rotation),
                                physics_debug_transform_point2d(b, position, rotation),
                                camera_cache);
}

static bool physics_debug_draw_circle2d(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                        size_t *index_capacity, float radius,
                                        BLB_Vec2 position, float rotation,
                                        BLB_Vec2 center,
                                        const BLB_CameraCache *camera_cache) {
  const unsigned int segments = 32;

  for (unsigned int i = 0; i < segments; ++i) {
    float a0 = (float)(BLB_PI * 2.0) * (float)i / (float)segments;
    float a1 = (float)(BLB_PI * 2.0) * (float)(i + 1u) / (float)segments;
    BLB_Vec2 p0 = BLB_V2(center.x + radius * cosf(a0), center.y + radius * sinf(a0));
    BLB_Vec2 p1 = BLB_V2(center.x + radius * cosf(a1), center.y + radius * sinf(a1));

    if (!physics_debug_draw_segment2d(polygon, vertex_capacity, index_capacity, p0, p1,
                                      position, rotation, camera_cache))
      return false;
  }

  return true;
}

static bool physics_debug_draw_capsule2d(BLB_Polygon3D *polygon, size_t *vertex_capacity,
                                         size_t *index_capacity, float radius,
                                         float half_height, BLB_Vec2 position,
                                         float rotation,
                                         const BLB_CameraCache *camera_cache) {
  const unsigned int segments = 16;

  for (unsigned int i = 0; i < segments; ++i) {
    float a0 = (float)BLB_PI * (float)i / (float)segments;
    float a1 = (float)BLB_PI * (float)(i + 1u) / (float)segments;

    BLB_Vec2 top0 = BLB_V2(radius * cosf(a0), half_height + radius * sinf(a0));
    BLB_Vec2 top1 = BLB_V2(radius * cosf(a1), half_height + radius * sinf(a1));

    float b0 = (float)BLB_PI + a0;
    float b1 = (float)BLB_PI + a1;

    BLB_Vec2 bottom0 = BLB_V2(radius * cosf(b0), -half_height + radius * sinf(b0));
    BLB_Vec2 bottom1 = BLB_V2(radius * cosf(b1), -half_height + radius * sinf(b1));

    if (!physics_debug_draw_segment2d(polygon, vertex_capacity, index_capacity, top0,
                                      top1, position, rotation, camera_cache) ||
        !physics_debug_draw_segment2d(polygon, vertex_capacity, index_capacity, bottom0,
                                      bottom1, position, rotation, camera_cache))
      return false;
  }

  return physics_debug_draw_segment2d(
             polygon, vertex_capacity, index_capacity, BLB_V2(-radius, -half_height),
             BLB_V2(-radius, half_height), position, rotation, camera_cache) &&
         physics_debug_draw_segment2d(
             polygon, vertex_capacity, index_capacity, BLB_V2(radius, -half_height),
             BLB_V2(radius, half_height), position, rotation, camera_cache);
}

void blb_render_draw_physics_debug_colliders2d(BLB_Scene *scene, BLB_Renderer *renderer,
                                               const BLB_CameraCache *camera_cache) {
  BLB_Physics2DWorld *world = scene ? scene->physics_world2d : NULL;

  if (!world || (!BLB_DEBUG && !BLB_Physics2D_WorldGetDebugDraw(world)) || !renderer ||
      !camera_cache || !camera_cache->valid)
    return;

  BLB_Polygon3D polygon = {0};
  polygon.dynamic_vertices = true;
  size_t vertex_capacity = 0;
  size_t index_capacity = 0;
  size_t count = BLB_Physics2D_WorldGetDebugColliderCount(world);

  for (size_t i = 0; i < count; ++i) {
    BLB_Physics2DDebugCollider collider;

    if (!BLB_Physics2D_WorldGetDebugCollider(world, i, &collider) || !collider.shape ||
        !collider.active)
      continue;

    const BLB_Physics2DShape *shape = collider.shape;
    bool ok = true;

    if (shape->type == BLB_PHYSICS2D_SHAPE_CIRCLE) {
      ok = physics_debug_draw_circle2d(
          &polygon, &vertex_capacity, &index_capacity, shape->data.circle.radius,
          collider.position, collider.rotation, BLB_V2(0.0f, 0.0f), camera_cache);
    } else if (shape->type == BLB_PHYSICS2D_SHAPE_BOX) {
      BLB_Vec2 half = shape->data.box.half_extents;
      BLB_Vec2 corners[4] = {BLB_V2(-half.x, -half.y), BLB_V2(half.x, -half.y),
                             BLB_V2(half.x, half.y), BLB_V2(-half.x, half.y)};

      for (size_t j = 0; j < 4 && ok; ++j)
        ok = physics_debug_draw_segment2d(
            &polygon, &vertex_capacity, &index_capacity, corners[j],
            corners[(j + 1u) % 4u], collider.position, collider.rotation, camera_cache);
    } else if (shape->type == BLB_PHYSICS2D_SHAPE_CAPSULE) {
      ok = physics_debug_draw_capsule2d(
          &polygon, &vertex_capacity, &index_capacity, shape->data.capsule.radius,
          shape->data.capsule.half_height, collider.position, collider.rotation,
          camera_cache);
    } else if (shape->type == BLB_PHYSICS2D_SHAPE_SEGMENT) {
      ok = physics_debug_draw_segment2d(
          &polygon, &vertex_capacity, &index_capacity, shape->data.segment.point1,
          shape->data.segment.point2, collider.position, collider.rotation, camera_cache);
    } else if (shape->type == BLB_PHYSICS2D_SHAPE_POLYGON) {
      const BLB_Vec2 *vertices = shape->data.polygon.vertices;
      size_t vertex_count = shape->data.polygon.vertex_count;

      for (size_t j = 0; vertices && j < vertex_count && ok; ++j)
        ok = physics_debug_draw_segment2d(&polygon, &vertex_capacity, &index_capacity,
                                          vertices[j], vertices[(j + 1u) % vertex_count],
                                          collider.position, collider.rotation,
                                          camera_cache);
    } else if (shape->type == BLB_PHYSICS2D_SHAPE_CHAIN) {
      const BLB_Vec2 *points = shape->data.chain.points;
      size_t point_count = shape->data.chain.point_count;
      size_t edge_count = shape->data.chain.loop
                              ? point_count
                              : (point_count > 0 ? point_count - 1u : 0u);

      for (size_t j = 0; points && j < edge_count && ok; ++j)
        ok = physics_debug_draw_segment2d(&polygon, &vertex_capacity, &index_capacity,
                                          points[j], points[(j + 1u) % point_count],
                                          collider.position, collider.rotation,
                                          camera_cache);
    }

    if (!ok)
      break;
  }

  if (polygon.vertex_count && polygon.index_count) {
    BLB_Mat4 view_projection = BLB_MulM4(camera_cache->projection, camera_cache->view);
    float model_rows[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    float color[4] = {0};
    BLB_RenderMaterial material = {0};
    color[0] = 0.1f;
    color[1] = 0.85f;
    color[2] = 1.0f;
    color[3] = 1.0f;
    material.lighting_enabled = false;
    material.unlit = true;
    material.depth_enabled = false;
    material.depth_write = false;
    material.double_sided = true;
    material.alpha_mode = BLB_ALPHA_OPAQUE;
    material.render_mode = BLB_RENDER_ADDITIVE;
    BLB_RendererDrawPolygon3D(renderer, &polygon, &view_projection.Elements[0][0],
                              model_rows, color[0], color[1], color[2], color[3],
                              &material, NULL);
  }

  free(polygon.vertices);
  free(polygon.indices);
  free(polygon.normals);
}

typedef struct {
  unsigned int a;
  unsigned int b;
} BLB_DebugEdge;

static int debug_edge_compare(const void *left, const void *right) {
  const BLB_DebugEdge *a = left;
  const BLB_DebugEdge *b = right;

  if (a->a != b->a)
    return a->a < b->a ? -1 : 1;

  if (a->b != b->b)
    return a->b < b->b ? -1 : 1;

  return 0;
}

static bool debug_handler_add_object2d(BLB_Polygon3D *lines, size_t *vertex_capacity,
                                       size_t *index_capacity, const BLB_Object2D *object,
                                       const BLB_CameraCache *camera_cache) {
  if (!object || !object->polygon || !object->polygon->vertices ||
      object->polygon->vertex_count < 2)
    return true;

  BLB_Mat4 model = BLB_M4D(1.0f);
  const float angle = BLB_AngleDeg(object->rotation);
  const float c = cosf(angle);
  const float s = sinf(angle);
  model.Elements[0][0] = c * object->scale.x;
  model.Elements[0][1] = s * object->scale.x;
  model.Elements[1][0] = -s * object->scale.y;
  model.Elements[1][1] = c * object->scale.y;
  model.Elements[3][0] = object->position.x;
  model.Elements[3][1] = object->position.y;

  if (object->camera_attached) {
    if (!camera_cache || !camera_cache->valid)
      return true;
    model = BLB_MulM4(BLB_CameraPixelToWorld(camera_cache), model);
  }

  const BLB_Polygon2D *polygon = object->polygon;

  if (polygon->indices && polygon->index_count >= 3 && polygon->index_count % 3 == 0) {
    if (polygon->index_count > SIZE_MAX / sizeof(BLB_DebugEdge))
      return false;

    BLB_DebugEdge *edges = malloc(polygon->index_count * sizeof(*edges));

    if (!edges)
      return false;

    size_t edge_count = 0;

    for (size_t i = 0; i + 2 < polygon->index_count; i += 3) {
      const unsigned int ids[3] = {polygon->indices[i], polygon->indices[i + 1],
                                   polygon->indices[i + 2]};

      if (ids[0] >= polygon->vertex_count || ids[1] >= polygon->vertex_count ||
          ids[2] >= polygon->vertex_count)
        continue;

      for (size_t edge = 0; edge < 3; ++edge) {
        unsigned int a = ids[edge];
        unsigned int b = ids[(edge + 1u) % 3u];

        if (a == b)
          continue;
        edges[edge_count++] = (BLB_DebugEdge){.a = a < b ? a : b, .b = a < b ? b : a};
      }
    }

    qsort(edges, edge_count, sizeof(*edges), debug_edge_compare);
    bool ok = true;

    for (size_t i = 0; i < edge_count && ok;) {
      size_t next = i + 1;

      while (next < edge_count && edges[next].a == edges[i].a &&
             edges[next].b == edges[i].b)
        ++next;

      if (next == i + 1) {
        const BLB_Vec2 a = polygon->vertices[edges[i].a];
        const BLB_Vec2 b = polygon->vertices[edges[i].b];
        const BLB_Vec4 a4 = BLB_MulM4V4(model, BLB_V4(a.x, a.y, 0.0f, 1.0f));
        const BLB_Vec4 b4 = BLB_MulM4V4(model, BLB_V4(b.x, b.y, 0.0f, 1.0f));
        ok = physics_debug_add_line(lines, vertex_capacity, index_capacity,
                                    BLB_V3(a4.x / a4.w, a4.y / a4.w, a4.z / a4.w),
                                    BLB_V3(b4.x / b4.w, b4.y / b4.w, b4.z / b4.w),
                                    camera_cache);
      }
      i = next;
    }
    free(edges);

    return ok;
  }

  for (size_t i = 0; i < polygon->vertex_count; ++i) {
    const BLB_Vec2 a = polygon->vertices[i];
    const BLB_Vec2 b = polygon->vertices[(i + 1u) % polygon->vertex_count];
    const BLB_Vec4 a4 = BLB_MulM4V4(model, BLB_V4(a.x, a.y, 0.0f, 1.0f));
    const BLB_Vec4 b4 = BLB_MulM4V4(model, BLB_V4(b.x, b.y, 0.0f, 1.0f));

    if (!physics_debug_add_line(lines, vertex_capacity, index_capacity,
                                BLB_V3(a4.x / a4.w, a4.y / a4.w, a4.z / a4.w),
                                BLB_V3(b4.x / b4.w, b4.y / b4.w, b4.z / b4.w),
                                camera_cache))
      return false;
  }

  return true;
}

static bool debug_handler_add_object3d(BLB_Polygon3D *lines, size_t *vertex_capacity,
                                       size_t *index_capacity, const BLB_Object3D *object,
                                       const BLB_CameraCache *camera_cache) {
  if (!object || !object->polygon || !object->polygon->vertices ||
      object->polygon->vertex_count == 0)
    return true;

  const BLB_Polygon3D *source = object->polygon;
  BLB_Vec3 min_v = source->vertices[0];
  BLB_Vec3 max_v = source->vertices[0];

  for (size_t i = 1; i < source->vertex_count; ++i) {
    const BLB_Vec3 v = source->vertices[i];
    min_v.x = fminf(min_v.x, v.x);
    min_v.y = fminf(min_v.y, v.y);
    min_v.z = fminf(min_v.z, v.z);
    max_v.x = fmaxf(max_v.x, v.x);
    max_v.y = fmaxf(max_v.y, v.y);
    max_v.z = fmaxf(max_v.z, v.z);
  }

  const BLB_Vec3 corners[8] = {
      BLB_V3(min_v.x, min_v.y, min_v.z), BLB_V3(max_v.x, min_v.y, min_v.z),
      BLB_V3(min_v.x, max_v.y, min_v.z), BLB_V3(max_v.x, max_v.y, min_v.z),
      BLB_V3(min_v.x, min_v.y, max_v.z), BLB_V3(max_v.x, min_v.y, max_v.z),
      BLB_V3(min_v.x, max_v.y, max_v.z), BLB_V3(max_v.x, max_v.y, max_v.z),
  };
  static const unsigned int edge_indices[12][2] = {
      {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
      {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7},
  };

  const BLB_Mat4 model = blb_render_object3d_model_matrix(object, camera_cache, 1.0f);

  for (size_t i = 0; i < 12; ++i) {
    const BLB_Vec3 a = corners[edge_indices[i][0]];
    const BLB_Vec3 b = corners[edge_indices[i][1]];
    const BLB_Vec4 a4 = BLB_MulM4V4(model, BLB_V4(a.x, a.y, a.z, 1.0f));
    const BLB_Vec4 b4 = BLB_MulM4V4(model, BLB_V4(b.x, b.y, b.z, 1.0f));

    if (!physics_debug_add_line(lines, vertex_capacity, index_capacity,
                                BLB_V3(a4.x / a4.w, a4.y / a4.w, a4.z / a4.w),
                                BLB_V3(b4.x / b4.w, b4.y / b4.w, b4.z / b4.w),
                                camera_cache))
      return false;
  }

  return true;
}

void blb_render_draw_handler_debug_hitboxes(BLB_Scene *scene, BLB_Renderer *renderer,
                                            const BLB_CameraCache *camera_cache) {
  if (!scene || !renderer || !camera_cache || !camera_cache->valid)
    return;

  BLB_Polygon3D lines = {0};
  lines.dynamic_vertices = true;
  size_t vertex_capacity = 0;
  size_t index_capacity = 0;
  bool ok = true;

  for (int i = 0; i < scene->object2d_count && ok; ++i) {
    BLB_Object2D *object = scene->objects2d[i];

    if (object && object->visible && object->handler && object->handler->enabled)
      ok = debug_handler_add_object2d(&lines, &vertex_capacity, &index_capacity, object,
                                      camera_cache);
  }

  for (int i = 0; i < scene->object3d_count && ok; ++i) {
    BLB_Object3D *object = scene->objects3d[i];

    if (object && object->visible && object->handler && object->handler->enabled)
      ok = debug_handler_add_object3d(&lines, &vertex_capacity, &index_capacity, object,
                                      camera_cache);
  }

  if (ok && lines.vertex_count && lines.index_count) {
    BLB_Mat4 view_projection = BLB_MulM4(camera_cache->projection, camera_cache->view);
    const float color[4] = {1.0f, 0.1f, 0.8f, 1.0f};
    BLB_RenderMaterial material = {0};
    material.lighting_enabled = false;
    material.unlit = true;
    material.depth_enabled = false;
    material.depth_write = false;
    material.double_sided = true;
    material.alpha_mode = BLB_ALPHA_OPAQUE;
    material.render_mode = BLB_RENDER_ADDITIVE;
    float model_rows[12] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                            0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    BLB_RendererDrawPolygon3D(renderer, &lines, &view_projection.Elements[0][0],
                              model_rows, color[0], color[1], color[2], color[3],
                              &material, NULL);
  }

  free(lines.vertices);
  free(lines.indices);
  free(lines.normals);
}
