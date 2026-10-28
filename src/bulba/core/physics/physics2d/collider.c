#include "physics2d.h"

BLB_Collider2D *BLB_Physics2D_SetCollider(BLB_Object2D *object,
                                          const BLB_Physics2DShape *shape) {
  BLB_RigidBody2D *body = blb_physics2d_object_body(object);

  if (!body || !shape || !b2Body_IsValid(body->handle))
    return NULL;

  BLB_Collider2D *collider = calloc(1, sizeof(*collider));

  if (!collider)
    return NULL;

  if (!blb_physics2d_copy_shape(&collider->shape, shape)) {
    blb_physics2d_free_shape_copy(&collider->shape);
    free(collider);

    return NULL;
  }

  collider->body = body;
  collider->id = body->world->next_collider_id++;
  collider->filter.layer = 0;
  collider->filter.mask = UINT64_MAX;
  collider->filter.group = 0;
  collider->sync_scale = object->scale;
  collider->trigger = false;
  collider->active = true;
  collider->user_data = shape->user_data;

  b2ShapeDef def = b2DefaultShapeDef();
  def.filter = blb_physics2d_make_filter(collider);
  def.userData = collider->user_data;

  b2ShapeId handle = b2_nullShapeId;
  b2ChainId chain_handle = b2_nullChainId;
  if (!blb_physics2d_create_backend_shape(collider, &def, &handle, &chain_handle)) {
    blb_physics2d_free_shape_copy(&collider->shape);
    free(collider);

    return NULL;
  }

  collider->handle = handle;
  collider->chain_handle = chain_handle;
  collider->density = b2Shape_GetDensity(handle);
  collider->friction = b2Shape_GetFriction(handle);
  collider->restitution = b2Shape_GetRestitution(handle);

  b2ShapeId *shape_ids = NULL;
  int shape_count = 0;
  if (blb_physics2d_collect_shape_ids(collider, &shape_ids, &shape_count)) {
    for (int i = 0; i < shape_count; ++i)
      b2Shape_SetUserData(shape_ids[i], collider->user_data);
    free(shape_ids);
  }

  BLB_Physics2DWorld *world = body->world;
  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);

  if (!blb_physics2d_append_body_collider(body, collider)) {
    if (b2Chain_IsValid(chain_handle))
      b2DestroyChain(chain_handle);
    else if (b2Shape_IsValid(handle))
      b2DestroyShape(handle, true);
    blb_physics2d_free_shape_copy(&collider->shape);
    free(collider);

    return NULL;
  }

  if (!blb_physics2d_append_collider(backend, collider, world->collider_count)) {
    blb_physics2d_remove_body_collider(body, collider);
    if (b2Chain_IsValid(chain_handle))
      b2DestroyChain(chain_handle);
    else if (b2Shape_IsValid(handle))
      b2DestroyShape(handle, true);
    blb_physics2d_free_shape_copy(&collider->shape);
    free(collider);

    return NULL;
  }

  ++world->collider_count;
  BLB_Physics2D_ColliderDestroy(object);
  object->collider = collider;

  return collider;
}

void BLB_Physics2D_ColliderDestroy(BLB_Object2D *object) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || !collider->body || !collider->body->world)
    return;

  BLB_Physics2DWorld *world = collider->body->world;
  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);

  if (b2Chain_IsValid(collider->chain_handle))
    b2DestroyChain(collider->chain_handle);
  else if (b2Shape_IsValid(collider->handle))
    b2DestroyShape(collider->handle, true);

  blb_physics2d_remove_body_collider(collider->body, collider);
  blb_physics2d_remove_collider(backend, collider, world->collider_count);

  if (world->collider_count > 0)
    --world->collider_count;

  blb_physics2d_free_shape_copy(&collider->shape);

  if (collider->body->object && collider->body->object->collider == collider)
    collider->body->object->collider = NULL;
  free(collider);
}

BLB_Physics2DColliderId BLB_Physics2D_ColliderGetId(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  return collider ? collider->id : 0;
}

void BLB_Physics2D_ColliderSetFilter(BLB_Object2D *object, BLB_PhysicsFilter filter) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider)
    return;

  collider->filter = filter;
  b2ShapeId *ids = NULL;
  int count = 0;
  if (blb_physics2d_collect_shape_ids(collider, &ids, &count)) {
    b2Filter backend_filter = blb_physics2d_make_filter(collider);
    for (int i = 0; i < count; ++i)
      b2Shape_SetFilter(ids[i], backend_filter);
    free(ids);
  }
}

BLB_PhysicsFilter BLB_Physics2D_ColliderGetFilter(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);
  BLB_PhysicsFilter filter = {0, 0, 0};

  if (collider)
    filter = collider->filter;

  return filter;
}

void BLB_Physics2D_ColliderSetLayer(BLB_Object2D *object, uint8_t layer) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || layer >= 64)
    return;

  collider->filter.layer = layer;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (blb_physics2d_collect_shape_ids(collider, &ids, &count)) {
    b2Filter backend_filter = blb_physics2d_make_filter(collider);
    for (int i = 0; i < count; ++i)
      b2Shape_SetFilter(ids[i], backend_filter);
    free(ids);
  }
}

void BLB_Physics2D_ColliderSetMask(BLB_Object2D *object, BLB_PhysicsLayerMask mask) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider)
    return;

  collider->filter.mask = mask;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (blb_physics2d_collect_shape_ids(collider, &ids, &count)) {
    b2Filter backend_filter = blb_physics2d_make_filter(collider);
    for (int i = 0; i < count; ++i)
      b2Shape_SetFilter(ids[i], backend_filter);
    free(ids);
  }
}

void BLB_Physics2D_ColliderSetGroup(BLB_Object2D *object, int32_t group) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider)
    return;

  collider->filter.group = group;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (blb_physics2d_collect_shape_ids(collider, &ids, &count)) {
    b2Filter backend_filter = blb_physics2d_make_filter(collider);
    for (int i = 0; i < count; ++i)
      b2Shape_SetFilter(ids[i], backend_filter);
    free(ids);
  }
}

void BLB_Physics2D_ColliderSetDensity(BLB_Object2D *object, float density) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || !isfinite(density) || density < 0.0f ||
      collider->shape.type == BLB_PHYSICS2D_SHAPE_CHAIN)
    return;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (!blb_physics2d_collect_shape_ids(collider, &ids, &count))
    return;
  collider->density = density;
  for (int i = 0; i < count; ++i)
    b2Shape_SetDensity(ids[i], density, true);
  free(ids);
}

float BLB_Physics2D_ColliderGetDensity(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || collider->shape.type == BLB_PHYSICS2D_SHAPE_CHAIN ||
      !b2Shape_IsValid(collider->handle))
    return 0.0f;

  return b2Shape_GetDensity(collider->handle);
}

void BLB_Physics2D_ColliderSetFriction(BLB_Object2D *object, float friction) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || !isfinite(friction) || friction < 0.0f)
    return;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (!blb_physics2d_collect_shape_ids(collider, &ids, &count))
    return;
  collider->friction = friction;
  for (int i = 0; i < count; ++i)
    b2Shape_SetFriction(ids[i], friction);
  free(ids);
}

float BLB_Physics2D_ColliderGetFriction(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || !b2Shape_IsValid(collider->handle))
    return 0.0f;

  return b2Shape_GetFriction(collider->handle);
}

void BLB_Physics2D_ColliderSetRestitution(BLB_Object2D *object, float restitution) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || !isfinite(restitution) || restitution < 0.0f || restitution > 1.0f)
    return;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (!blb_physics2d_collect_shape_ids(collider, &ids, &count))
    return;
  collider->restitution = restitution;
  for (int i = 0; i < count; ++i)
    b2Shape_SetRestitution(ids[i], restitution);
  free(ids);
}

float BLB_Physics2D_ColliderGetRestitution(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || !b2Shape_IsValid(collider->handle))
    return 0.0f;

  return b2Shape_GetRestitution(collider->handle);
}

void BLB_Physics2D_ColliderSetTrigger(BLB_Object2D *object, bool trigger) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider || collider->trigger == trigger ||
      (collider->shape.type == BLB_PHYSICS2D_SHAPE_CHAIN && trigger))
    return;

  blb_physics2d_recreate_shape(collider, trigger);
}

bool BLB_Physics2D_ColliderIsTrigger(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  return collider ? collider->trigger : false;
}

void BLB_Physics2D_ColliderSetActive(BLB_Object2D *object, bool active) {
  BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  if (!collider)
    return;

  b2ShapeId *ids = NULL;
  int count = 0;
  if (!blb_physics2d_collect_shape_ids(collider, &ids, &count))
    return;
  collider->active = active;
  b2Filter backend_filter = blb_physics2d_make_filter(collider);
  for (int i = 0; i < count; ++i)
    b2Shape_SetFilter(ids[i], backend_filter);
  free(ids);
}

bool BLB_Physics2D_ColliderIsActive(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  return collider ? collider->active : false;
}

BLB_Collider2D *BLB_Physics2D_GetCollider(BLB_Object2D *object) {
  return blb_physics2d_object_collider(object);
}

const BLB_Physics2DShape *BLB_Physics2D_ColliderGetShape(const BLB_Object2D *object) {
  const BLB_Collider2D *collider = blb_physics2d_object_collider(object);

  return collider ? &collider->shape : NULL;
}

void BLB_Physics2D_ColliderSetScale(BLB_Object2D *object, BLB_Vec2 scale) {
  BLB_Collider2D *collider = object ? object->collider : NULL;

  if (!collider || !isfinite(scale.x) || scale.x <= 0.0f ||
      collider->sync_scale.x <= 0.0f || !isfinite(scale.y) || scale.y <= 0.0f ||
      collider->sync_scale.y <= 0.0f)
    return;

  if (!memcmp(&scale, &collider->sync_scale, sizeof(scale))) {
    blb_object2d_sync_from_body(object);

    return;
  }

  BLB_Physics2DShape next = {0};

  if (!blb_physics2d_copy_shape(&next, &collider->shape)) {
    blb_physics2d_free_shape_copy(&next);

    return;
  }

  BLB_Vec2 ratio = BLB_DivV2(scale, collider->sync_scale);

  switch (next.type) {
  case BLB_PHYSICS2D_SHAPE_BOX:
    next.data.box.half_extents = BLB_MulV2(next.data.box.half_extents, ratio);
    break;
  case BLB_PHYSICS2D_SHAPE_CIRCLE:
    next.data.circle.radius *=
        fminf(scale.x, scale.y) / fminf(collider->sync_scale.x, collider->sync_scale.y);
    break;
  case BLB_PHYSICS2D_SHAPE_CAPSULE:
    next.data.capsule.radius *= fminf(ratio.x, ratio.y);
    next.data.capsule.half_height *= ratio.y;
    break;
  case BLB_PHYSICS2D_SHAPE_SEGMENT:
    next.data.segment.point1 = BLB_MulV2(next.data.segment.point1, ratio);
    next.data.segment.point2 = BLB_MulV2(next.data.segment.point2, ratio);
    break;
  case BLB_PHYSICS2D_SHAPE_POLYGON:
    for (size_t i = 0; i < next.data.polygon.vertex_count; ++i)
      next.data.polygon.vertices[i] = BLB_MulV2(next.data.polygon.vertices[i], ratio);
    break;
  case BLB_PHYSICS2D_SHAPE_CHAIN:
    for (size_t i = 0; i < next.data.chain.point_count; ++i)
      next.data.chain.points[i] = BLB_MulV2(next.data.chain.points[i], ratio);
    next.data.chain.ghost1 = BLB_MulV2(next.data.chain.ghost1, ratio);
    next.data.chain.ghost2 = BLB_MulV2(next.data.chain.ghost2, ratio);
    break;
  default:
    blb_physics2d_free_shape_copy(&next);

    return;
  }

  BLB_Physics2DShape previous = collider->shape;
  collider->shape = next;

  if (!blb_physics2d_recreate_shape(collider, collider->trigger)) {
    collider->shape = previous;
    blb_physics2d_free_shape_copy(&next);

    return;
  }

  blb_physics2d_free_shape_copy(&previous);
  collider->sync_scale = scale;
  blb_object2d_sync_from_body(object);
}
