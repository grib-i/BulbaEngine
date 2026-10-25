#include "box2d/id.h"
#include "box2d/math_types.h"
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
  } else if (src->type == BLB_PHYSICS2D_SHAPE_CHAIN) {
    if (src->data.chain.point_count < 2 ||
        src->data.chain.point_count > SIZE_MAX / sizeof(BLB_Vec2) ||
        !src->data.chain.points) {
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
  }

  return true;
}

b2ShapeId blb_physics2d_create_backend_shape(const BLB_Collider2D *collider,
                                             const b2ShapeDef *def) {
  if (!collider || !def)
    return b2_nullShapeId;

  b2BodyId body = collider->body->handle;

  switch (collider->shape.type) {
  case BLB_PHYSICS2D_SHAPE_BOX: {
    b2Polygon polygon = b2MakeBox(collider->shape.data.box.half_extents.x,
                                  collider->shape.data.box.half_extents.y);

    return b2CreatePolygonShape(body, def, &polygon);
  }

  case BLB_PHYSICS2D_SHAPE_CIRCLE: {
    b2Circle circle = {
        .center = {0.0f, 0.0f},
        .radius = collider->shape.data.circle.radius,
    };

    return b2CreateCircleShape(body, def, &circle);
  }

  case BLB_PHYSICS2D_SHAPE_CAPSULE: {
    b2Capsule capsule = {
        .center1 = {0.0f, -collider->shape.data.capsule.half_height},
        .center2 = {0.0f, collider->shape.data.capsule.half_height},
        .radius = collider->shape.data.capsule.radius,
    };

    return b2CreateCapsuleShape(body, def, &capsule);
  }

  case BLB_PHYSICS2D_SHAPE_SEGMENT: {
    b2Segment segment = {
        .point1 = {collider->shape.data.segment.point1.x,
                   collider->shape.data.segment.point1.y},
        .point2 = {collider->shape.data.segment.point2.x,
                   collider->shape.data.segment.point2.y},
    };

    return b2CreateSegmentShape(body, def, &segment);
  }

  case BLB_PHYSICS2D_SHAPE_POLYGON: {
    if (!collider->shape.data.polygon.vertices ||
        collider->shape.data.polygon.vertex_count < 3 ||
        collider->shape.data.polygon.vertex_count > B2_MAX_POLYGON_VERTICES)
      return b2_nullShapeId;

    b2Vec2 points[B2_MAX_POLYGON_VERTICES];

    for (size_t i = 0; i < collider->shape.data.polygon.vertex_count; ++i) {
      points[i].x = collider->shape.data.polygon.vertices[i].x;
      points[i].y = collider->shape.data.polygon.vertices[i].y;
    }

    b2Hull hull = b2ComputeHull(points, (int)collider->shape.data.polygon.vertex_count);

    if (hull.count < 3)
      return b2_nullShapeId;

    b2Polygon polygon = b2MakePolygon(&hull, 0.0f);

    return b2CreatePolygonShape(body, def, &polygon);
  }

  case BLB_PHYSICS2D_SHAPE_CHAIN: {
    const __typeof__(collider->shape.data.chain) *chain_data =
        &collider->shape.data.chain;

    if (!chain_data->points || chain_data->point_count < 0 ||
        chain_data->point_count < (chain_data->loop ? 3 : 2))
      return b2_nullShapeId;

    b2ChainDef chainDef = b2DefaultChainDef();

    _Static_assert(sizeof(BLB_Vec2) == sizeof(b2Vec2), "Vector sizes differ");
    b2Vec2 *points = malloc(chain_data->point_count * sizeof(*points));

    if (!points)
      return b2_nullShapeId;

    memcpy(points, chain_data->points, chain_data->point_count * sizeof(*points));
    chainDef.points = points;
    chainDef.pointCount = chain_data->point_count;
    chainDef.isLoop = chain_data->loop;

    chainDef.ghost1 = (b2Vec2){chain_data->ghost1.x, chain_data->ghost1.y};
    chainDef.ghost2 = (b2Vec2){chain_data->ghost2.x, chain_data->ghost2.y};

    b2ChainId chain = b2CreateChain(body, &chainDef);
    if (b2Chain_IsValid(chain) == false) {
      return b2_nullShapeId;
    }

    return b2_nullShapeId;
  }

  default:
    return b2_nullShapeId;
  }
}

bool blb_physics2d_recreate_shape(BLB_Collider2D *collider, bool trigger) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return false;

  b2ShapeDef def = b2DefaultShapeDef();
  def.density = collider->density;
  def.filter = blb_physics2d_make_filter(collider);
  def.isSensor = trigger;
  def.updateBodyMass = false;
  def.material.friction = collider->friction;
  def.material.restitution = collider->restitution;

  b2ShapeId replacement = blb_physics2d_create_backend_shape(collider, &def);

  if (!b2Shape_IsValid(replacement))
    return false;

  b2ShapeId old = collider->handle;
  collider->handle = replacement;
  collider->trigger = trigger;

  b2Shape_SetUserData(replacement, collider->user_data);
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
  source.data.segment.point1 = point2;

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
