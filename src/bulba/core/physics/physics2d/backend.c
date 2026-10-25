#include "physics2d.h"

bool blb_physics2d_valid_world(const BLB_Physics2DWorld *world) {
  return world && world->backend && b2World_IsValid(((const BLB_Box2DBackend *)world->backend)->world_id);
}

BLB_Box2DBackend *blb_physics2d_get_backend(BLB_Physics2DWorld *world) { return world ? (BLB_Box2DBackend *)world->backend : NULL; }

const BLB_Box2DBackend *blb_physics2d_get_backend_const(const BLB_Physics2DWorld *world) {
  return world ? (const BLB_Box2DBackend *)world->backend : NULL;
}

BLB_PhysicsLayerMask blb_physics2d_layer_bit(uint8_t layer) { return layer < 64 ? BLB_PhysicsLayer(layer) : 0; }

b2BodyType blb_physics2d_to_b2_body_type(BLB_Physics2DBodyType type) {
  switch (type) {
  case BLB_PHYSICS2D_DYNAMIC:
    return b2_dynamicBody;
  case BLB_PHYSICS2D_KINEMATIC:
    return b2_kinematicBody;
  case BLB_PHYSICS2D_STATIC:
  default:
    return b2_staticBody;
  }
}

BLB_Physics2DBodyType blb_physics2d_from_b2_body_type(b2BodyType type) {
  switch (type) {
  case b2_dynamicBody:
    return BLB_PHYSICS2D_DYNAMIC;
  case b2_kinematicBody:
    return BLB_PHYSICS2D_KINEMATIC;
  case b2_staticBody:
  default:
    return BLB_PHYSICS2D_STATIC;
  }
}

bool blb_physics2d_append_body(BLB_Box2DBackend *backend, BLB_RigidBody2D *body, size_t count) {
  BLB_RigidBody2D **items = realloc(backend->bodies, (count + 1) * sizeof(*items));

  if (!items)
    return false;

  items[count] = body;
  backend->bodies = items;

  return true;
}

bool blb_physics2d_append_collider(BLB_Box2DBackend *backend, BLB_Collider2D *collider, size_t count) {
  BLB_Collider2D **items = realloc(backend->colliders, (count + 1) * sizeof(*items));

  if (!items)
    return false;

  items[count] = collider;
  backend->colliders = items;

  return true;
}

bool blb_physics2d_append_body_collider(BLB_RigidBody2D *body, BLB_Collider2D *collider) {
  BLB_Collider2D **items = realloc(body->colliders, (body->collider_count + 1) * sizeof(*items));

  if (!items)
    return false;

  items[body->collider_count++] = collider;
  body->colliders = items;

  return true;
}

void blb_physics2d_remove_body(BLB_Box2DBackend *backend, BLB_RigidBody2D *body, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (backend->bodies[i] != body)
      continue;

    if (i + 1 < count)
      memmove(&backend->bodies[i], &backend->bodies[i + 1], (count - i - 1) * sizeof(*backend->bodies));

    if (count == 1) {
      free(backend->bodies);
      backend->bodies = NULL;
    } else {
      BLB_RigidBody2D **items = realloc(backend->bodies, (count - 1) * sizeof(*items));

      if (items)
        backend->bodies = items;
    }

    return;
  }
}

void blb_physics2d_remove_collider(BLB_Box2DBackend *backend, BLB_Collider2D *collider, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (backend->colliders[i] != collider)
      continue;

    if (i + 1 < count)
      memmove(&backend->colliders[i], &backend->colliders[i + 1], (count - i - 1) * sizeof(*backend->colliders));

    if (count == 1) {
      free(backend->colliders);
      backend->colliders = NULL;
    } else {
      BLB_Collider2D **items = realloc(backend->colliders, (count - 1) * sizeof(*items));

      if (items)
        backend->colliders = items;
    }

    return;
  }
}

void blb_physics2d_remove_body_collider(BLB_RigidBody2D *body, BLB_Collider2D *collider) {
  for (size_t i = 0; i < body->collider_count; ++i) {
    if (body->colliders[i] != collider)
      continue;

    if (i + 1 < body->collider_count)
      memmove(&body->colliders[i], &body->colliders[i + 1], (body->collider_count - i - 1) * sizeof(*body->colliders));

    --body->collider_count;

    if (body->collider_count == 0) {
      free(body->colliders);
      body->colliders = NULL;
    } else {
      BLB_Collider2D **items = realloc(body->colliders, body->collider_count * sizeof(*items));

      if (items)
        body->colliders = items;
    }

    return;
  }
}

b2Filter blb_physics2d_make_filter(const BLB_Collider2D *collider) {
  BLB_PhysicsLayerMask category = blb_physics2d_layer_bit(collider->filter.layer);
  BLB_PhysicsLayerMask mask = collider->filter.mask;

  const BLB_Box2DBackend *backend = blb_physics2d_get_backend_const(collider->body->world);

  if (!collider->active) {
    mask = 0;
  } else if (backend && collider->filter.layer < 64) {
    mask &= backend->layer_masks[collider->filter.layer];
  }

  b2Filter filter = {0};
  filter.categoryBits = category;
  filter.maskBits = mask;
  filter.groupIndex = collider->filter.group;

  return filter;
}

BLB_RigidBody2D *blb_physics2d_object_body(const BLB_Object2D *object) {
  BLB_RigidBody2D *body = object ? object->rigid_body : NULL;

  return body && b2Body_IsValid(body->handle) ? body : NULL;
}

BLB_Collider2D *blb_physics2d_object_collider(const BLB_Object2D *object) {
  BLB_Collider2D *collider = object ? object->collider : NULL;

  return collider && b2Shape_IsValid(collider->handle) ? collider : NULL;
}
