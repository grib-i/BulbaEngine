#include "box2d/id.h"
#include "box2d/math_types.h"
#include <limits.h>
#include "bulba/core/physics/physics2d.h"
#include "physics2d.h"

void blb_physics2d_free_shape_copy(BLB_Physics2DShape *shape) {
  if (!shape)
    return;

  if (shape->type == BLB_PHYSICS2D_SHAPE_POLYGON) {
    free(shape->data.polygon.vertices);

    shape->data.polygon.vertices = NULL;
    shape->data.polygon.vertex_count = 0;

  } else if (shape->type == BLB_PHYSICS2D_SHAPE_CHAIN) {
    free(shape->data.chain.points);

    shape->data.chain.points = NULL;
    shape->data.chain.point_count = 0;
  }
}

bool blb_physics2d_copy_shape(BLB_Physics2DShape *dst, const BLB_Physics2DShape *src) {
  if (!dst || !src)
    return false;

  *dst = *src;

  if (src->type == BLB_PHYSICS2D_SHAPE_BOX) {
    if (!isfinite(src->data.box.half_extents.x) || !isfinite(src->data.box.half_extents.y) ||
        src->data.box.half_extents.x <= 0.0f || src->data.box.half_extents.y <= 0.0f)
      return false;
  } else if (src->type == BLB_PHYSICS2D_SHAPE_CIRCLE) {
    if (!isfinite(src->data.circle.radius) || src->data.circle.radius <= 0.0f)
      return false;
  } else if (src->type == BLB_PHYSICS2D_SHAPE_CAPSULE) {
    if (!isfinite(src->data.capsule.radius) || !isfinite(src->data.capsule.half_height) ||
        src->data.capsule.radius <= 0.0f || src->data.capsule.half_height < 0.0f)
      return false;
  } else if (src->type == BLB_PHYSICS2D_SHAPE_SEGMENT) {
    BLB_Vec2 a = src->data.segment.point1;
    BLB_Vec2 b = src->data.segment.point2;
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float length_squared = dx * dx + dy * dy;
    if (!isfinite(a.x) || !isfinite(a.y) || !isfinite(b.x) || !isfinite(b.y) ||
        !isfinite(dx) || !isfinite(dy) || !isfinite(length_squared) ||
        length_squared <= 0.000025f)
      return false;
  } else if (src->type != BLB_PHYSICS2D_SHAPE_POLYGON &&
             src->type != BLB_PHYSICS2D_SHAPE_CHAIN) {
    return false;
  }

  if (src->type == BLB_PHYSICS2D_SHAPE_POLYGON) {
    if (src->data.polygon.vertex_count < 3 ||
        src->data.polygon.vertex_count > B2_MAX_POLYGON_VERTICES ||
        !src->data.polygon.vertices) {
      dst->data.polygon.vertices = NULL;
      dst->data.polygon.vertex_count = 0;

      return false;
    }

    dst->data.polygon.vertices =
        malloc(src->data.polygon.vertex_count * sizeof(*dst->data.polygon.vertices));

    if (!dst->data.polygon.vertices)
      return false;

    memcpy(dst->data.polygon.vertices, src->data.polygon.vertices,
           src->data.polygon.vertex_count * sizeof(*dst->data.polygon.vertices));
    for (size_t i = 0; i < dst->data.polygon.vertex_count; ++i) {
      if (!isfinite(dst->data.polygon.vertices[i].x) ||
          !isfinite(dst->data.polygon.vertices[i].y)) {
        blb_physics2d_free_shape_copy(dst);
        return false;
      }
    }
  } else if (src->type == BLB_PHYSICS2D_SHAPE_CHAIN) {
    if (src->data.chain.point_count < (src->data.chain.loop ? 3u : 2u) ||
        src->data.chain.point_count > INT_MAX ||
        src->data.chain.point_count > SIZE_MAX / sizeof(BLB_Vec2) ||
        !src->data.chain.points || !isfinite(src->data.chain.ghost1.x) ||
        !isfinite(src->data.chain.ghost1.y) || !isfinite(src->data.chain.ghost2.x) ||
        !isfinite(src->data.chain.ghost2.y)) {
      dst->data.chain.points = NULL;
      dst->data.chain.point_count = 0;

      return false;
    }

    dst->data.chain.points =
        malloc(src->data.chain.point_count * sizeof(*dst->data.chain.points));

    if (!dst->data.chain.points)
      return false;

    memcpy(dst->data.chain.points, src->data.chain.points,
           src->data.chain.point_count * sizeof(*dst->data.chain.points));
    for (size_t i = 0; i < dst->data.chain.point_count; ++i) {
      if (!isfinite(dst->data.chain.points[i].x) ||
          !isfinite(dst->data.chain.points[i].y)) {
        blb_physics2d_free_shape_copy(dst);
        return false;
      }
      if (i > 0) {
        float dx = dst->data.chain.points[i].x - dst->data.chain.points[i - 1].x;
        float dy = dst->data.chain.points[i].y - dst->data.chain.points[i - 1].y;
        float length_squared = dx * dx + dy * dy;
        if (!isfinite(dx) || !isfinite(dy) || !isfinite(length_squared) ||
            length_squared <= 0.000025f) {
          blb_physics2d_free_shape_copy(dst);
          return false;
        }
      }
    }
    if (dst->data.chain.loop) {
      BLB_Vec2 first = dst->data.chain.points[0];
      BLB_Vec2 last = dst->data.chain.points[dst->data.chain.point_count - 1];
      float dx = first.x - last.x;
      float dy = first.y - last.y;
      float length_squared = dx * dx + dy * dy;
      if (!isfinite(dx) || !isfinite(dy) || !isfinite(length_squared) ||
          length_squared <= 0.000025f) {
        blb_physics2d_free_shape_copy(dst);
        return false;
      }
    } else {
      BLB_Vec2 first = dst->data.chain.points[0];
      BLB_Vec2 last = dst->data.chain.points[dst->data.chain.point_count - 1];
      float dx1 = first.x - dst->data.chain.ghost1.x;
      float dy1 = first.y - dst->data.chain.ghost1.y;
      float dx2 = last.x - dst->data.chain.ghost2.x;
      float dy2 = last.y - dst->data.chain.ghost2.y;
      float length1_squared = dx1 * dx1 + dy1 * dy1;
      float length2_squared = dx2 * dx2 + dy2 * dy2;
      if (!isfinite(dx1) || !isfinite(dy1) || !isfinite(dx2) || !isfinite(dy2) ||
          !isfinite(length1_squared) || !isfinite(length2_squared) ||
          length1_squared <= 0.000025f || length2_squared <= 0.000025f) {
        blb_physics2d_free_shape_copy(dst);
        return false;
      }
    }
  }

  return true;
}

bool blb_physics2d_create_backend_shape(BLB_Collider2D *collider, const b2ShapeDef *def,
                                        b2ShapeId *shape_handle, b2ChainId *chain_handle) {
  if (!collider || !collider->body || !def || !shape_handle || !chain_handle)
    return false;

  *shape_handle = b2_nullShapeId;
  *chain_handle = b2_nullChainId;
  b2BodyId body = collider->body->handle;

  switch (collider->shape.type) {
  case BLB_PHYSICS2D_SHAPE_BOX: {
    b2Polygon polygon = b2MakeBox(collider->shape.data.box.half_extents.x,
                                  collider->shape.data.box.half_extents.y);
    *shape_handle = b2CreatePolygonShape(body, def, &polygon);
    break;
  }
  case BLB_PHYSICS2D_SHAPE_CIRCLE: {
    b2Circle circle = {
        .center = {0.0f, 0.0f},
        .radius = collider->shape.data.circle.radius,
    };
    *shape_handle = b2CreateCircleShape(body, def, &circle);
    break;
  }
  case BLB_PHYSICS2D_SHAPE_CAPSULE: {
    b2Capsule capsule = {
        .center1 = {0.0f, -collider->shape.data.capsule.half_height},
        .center2 = {0.0f, collider->shape.data.capsule.half_height},
        .radius = collider->shape.data.capsule.radius,
    };
    *shape_handle = b2CreateCapsuleShape(body, def, &capsule);
    break;
  }
  case BLB_PHYSICS2D_SHAPE_SEGMENT: {
    b2Segment segment = {
        .point1 = {collider->shape.data.segment.point1.x,
                   collider->shape.data.segment.point1.y},
        .point2 = {collider->shape.data.segment.point2.x,
                   collider->shape.data.segment.point2.y},
    };
    *shape_handle = b2CreateSegmentShape(body, def, &segment);
    break;
  }
  case BLB_PHYSICS2D_SHAPE_POLYGON: {
    if (!collider->shape.data.polygon.vertices ||
        collider->shape.data.polygon.vertex_count < 3 ||
        collider->shape.data.polygon.vertex_count > B2_MAX_POLYGON_VERTICES)
      return false;

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];
    for (size_t i = 0; i < collider->shape.data.polygon.vertex_count; ++i) {
      points[i].x = collider->shape.data.polygon.vertices[i].x;
      points[i].y = collider->shape.data.polygon.vertices[i].y;
    }

    b2Hull hull = b2ComputeHull(points, (int)collider->shape.data.polygon.vertex_count);
    if (hull.count < 3)
      return false;

    b2Polygon polygon = b2MakePolygon(&hull, 0.0f);
    *shape_handle = b2CreatePolygonShape(body, def, &polygon);
    break;
  }
  case BLB_PHYSICS2D_SHAPE_CHAIN: {
    const BLB_ChainShape2D *chain_data = &collider->shape.data.chain;
    if (collider->body->type != BLB_PHYSICS2D_STATIC || !chain_data->points ||
        chain_data->point_count < (chain_data->loop ? 3u : 2u) ||
        chain_data->point_count > INT_MAX || def->isSensor)
      return false;

    b2Vec2 *points = malloc(chain_data->point_count * sizeof(*points));
    if (!points)
      return false;

    for (size_t i = 0; i < chain_data->point_count; ++i) {
      points[i].x = chain_data->points[i].x;
      points[i].y = chain_data->points[i].y;
    }

    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = def->material.friction;
    material.restitution = def->material.restitution;
    b2ChainDef chain_def = b2DefaultChainDef();
    chain_def.userData = collider->user_data;
    chain_def.points = points;
    chain_def.pointCount = (int)chain_data->point_count;
    chain_def.isLoop = chain_data->loop;
    chain_def.ghost1 = (b2Vec2){chain_data->ghost1.x, chain_data->ghost1.y};
    chain_def.ghost2 = (b2Vec2){chain_data->ghost2.x, chain_data->ghost2.y};
    chain_def.filter = def->filter;
    chain_def.materials = &material;
    chain_def.materialCount = 1;
    chain_def.enableSensorEvents = def->enableSensorEvents;

    b2ChainId chain = b2CreateChain(body, &chain_def);
    free(points);
    if (!b2Chain_IsValid(chain))
      return false;

    int segment_count = b2Chain_GetSegmentCount(chain);
    if (segment_count < 1 || b2Chain_GetSegments(chain, shape_handle, 1) != 1) {
      b2DestroyChain(chain);
      *shape_handle = b2_nullShapeId;
      return false;
    }
    *chain_handle = chain;
    break;
  }
  default:
    return false;
  }

  if (!b2Shape_IsValid(*shape_handle)) {
    *shape_handle = b2_nullShapeId;
    return false;
  }

  return true;
}

bool blb_physics2d_collect_shape_ids(const BLB_Collider2D *collider, b2ShapeId **shape_ids,
                                     int *shape_count) {
  if (!shape_ids || !shape_count)
    return false;

  *shape_ids = NULL;
  *shape_count = 0;
  if (!collider)
    return false;

  int count = 0;
  if (b2Chain_IsValid(collider->chain_handle))
    count = b2Chain_GetSegmentCount(collider->chain_handle);
  else if (b2Shape_IsValid(collider->handle))
    count = 1;
  if (count <= 0)
    return false;

  b2ShapeId *ids = malloc((size_t)count * sizeof(*ids));
  if (!ids)
    return false;

  int actual = 0;
  if (b2Chain_IsValid(collider->chain_handle))
    actual = b2Chain_GetSegments(collider->chain_handle, ids, count);
  else {
    ids[0] = collider->handle;
    actual = 1;
  }

  if (actual != count) {
    free(ids);
    return false;
  }

  *shape_ids = ids;
  *shape_count = count;
  return true;
}

bool blb_physics2d_recreate_shape(BLB_Collider2D *collider, bool trigger) {
  if (!collider ||
      (!b2Shape_IsValid(collider->handle) && !b2Chain_IsValid(collider->chain_handle)))
    return false;
  if (collider->shape.type == BLB_PHYSICS2D_SHAPE_CHAIN && trigger)
    return false;

  b2ShapeDef def = b2DefaultShapeDef();
  def.density = collider->density;
  def.filter = blb_physics2d_make_filter(collider);
  def.isSensor = trigger;
  def.updateBodyMass = false;
  def.material.friction = collider->friction;
  def.material.restitution = collider->restitution;

  b2ShapeId replacement = b2_nullShapeId;
  b2ChainId replacement_chain = b2_nullChainId;
  if (!blb_physics2d_create_backend_shape(collider, &def, &replacement, &replacement_chain))
    return false;

  if (b2Chain_IsValid(replacement_chain)) {
    int count = b2Chain_GetSegmentCount(replacement_chain);
    b2ShapeId *ids = count > 0 ? malloc((size_t)count * sizeof(*ids)) : NULL;
    if (!ids || b2Chain_GetSegments(replacement_chain, ids, count) != count) {
      free(ids);
      b2DestroyChain(replacement_chain);
      return false;
    }
    for (int i = 0; i < count; ++i)
      b2Shape_SetUserData(ids[i], collider->user_data);
    free(ids);
  } else {
    b2Shape_SetUserData(replacement, collider->user_data);
  }

  b2ShapeId old = collider->handle;
  b2ChainId old_chain = collider->chain_handle;
  collider->handle = replacement;
  collider->chain_handle = replacement_chain;
  collider->trigger = trigger;

  if (b2Chain_IsValid(old_chain))
    b2DestroyChain(old_chain);
  else if (b2Shape_IsValid(old))
    b2DestroyShape(old, false);
  return true;
}

BLB_Physics2DShape *BLB_Physics2D_CreateShape(BLB_Physics2DShapeType type,
                                              BLB_Vec2 scale) {
  if (!isfinite(scale.x) || !isfinite(scale.y) || scale.x <= 0 || scale.y <= 0)
    return NULL;

  if (type != BLB_PHYSICS2D_SHAPE_BOX && type != BLB_PHYSICS2D_SHAPE_CIRCLE &&
      type != BLB_PHYSICS2D_SHAPE_CAPSULE)
    return NULL;

  BLB_Physics2DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  shape->type = type;

  if (type == BLB_PHYSICS2D_SHAPE_BOX) {
    shape->data.box.half_extents = BLB_MulV2F(scale, 0.5f);

  } else if (type == BLB_PHYSICS2D_SHAPE_CIRCLE) {
    shape->data.circle.radius = fminf(scale.x, scale.y) * 0.5f;

  } else {
    shape->data.capsule.radius = scale.x * 0.5f;
    shape->data.capsule.half_height = fmaxf(0.0f, (scale.y - scale.x) * 0.5f);
  }

  return shape;
}

BLB_Physics2DShape *BLB_Physics2D_CreateShapePolygon(BLB_Vec2 *vertices,
                                                     size_t vertex_count) {
  BLB_Physics2DShape source = {.type = BLB_PHYSICS2D_SHAPE_POLYGON};
  source.data.polygon.vertices = vertices;
  source.data.polygon.vertex_count = vertex_count;

  BLB_Physics2DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  if (!blb_physics2d_copy_shape(shape, &source)) {
    blb_physics2d_free_shape_copy(shape);
    free(shape);

    return NULL;
  }

  return shape;
}

BLB_Physics2DShape *BLB_Physics2D_CreateShapeSegment(BLB_Vec2 point1, BLB_Vec2 point2) {
  BLB_Physics2DShape source = {.type = BLB_PHYSICS2D_SHAPE_SEGMENT};
  source.data.segment.point1 = point1;
  source.data.segment.point2 = point2;

  BLB_Physics2DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  if (!blb_physics2d_copy_shape(shape, &source)) {
    blb_physics2d_free_shape_copy(shape);
    free(shape);

    return NULL;
  }

  return shape;
}

BLB_Physics2DShape *BLB_Physics2D_CreateShapeChain(BLB_Vec2 *points, size_t point_count,
                                                   bool loop, BLB_Vec2 ghost1,
                                                   BLB_Vec2 ghost2) {
  BLB_Physics2DShape source = {.type = BLB_PHYSICS2D_SHAPE_CHAIN};
  source.data.chain.points = points;
  source.data.chain.point_count = point_count;
  source.data.chain.loop = loop;

  source.data.chain.ghost1 = ghost1;
  source.data.chain.ghost2 = ghost2;

  BLB_Physics2DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  if (!blb_physics2d_copy_shape(shape, &source)) {
    blb_physics2d_free_shape_copy(shape);
    free(shape);

    return NULL;
  }

  return shape;
}

BLB_Physics2DShape *BLB_Physics2D_CreateShapeFromObject2D(const BLB_Object2D *object) {
  if (!object)
    return NULL;

  return BLB_Physics2D_CreateShape(object->type == BLB_OBJECT_CIRCLE2D
                                       ? BLB_PHYSICS2D_SHAPE_CIRCLE
                                       : BLB_PHYSICS2D_SHAPE_BOX,
                                   object->scale);
}

void BLB_Physics2D_DestroyShape(BLB_Physics2DShape *shape) {
  if (!shape)
    return;

  blb_physics2d_free_shape_copy(shape);
  free(shape);
}
