#include "physics3d.h"

bool blb_physics3d_valid_world(const BLB_Physics3DWorld *world) {
  return world && world->backend &&
         b3World_IsValid(((const BLB_Box3DBackend *)world->backend)->world_id);
}

BLB_Box3DBackend *blb_physics3d_get_backend(BLB_Physics3DWorld *world) {
  return world ? (BLB_Box3DBackend *)world->backend : NULL;
}

const BLB_Box3DBackend *blb_physics3d_get_backend_const(const BLB_Physics3DWorld *world) {
  return world ? (const BLB_Box3DBackend *)world->backend : NULL;
}

BLB_PhysicsLayerMask blb_physics3d_layer_bit(uint8_t layer) {
  return layer < 64 ? BLB_PhysicsLayer(layer) : 0;
}

b3BodyType blb_physics3d_to_b3_body_type(BLB_Physics3DBodyType type) {
  switch (type) {
  case BLB_PHYSICS3D_DYNAMIC:
    return b3_dynamicBody;
  case BLB_PHYSICS3D_KINEMATIC:
    return b3_kinematicBody;
  case BLB_PHYSICS3D_STATIC:
  default:
    return b3_staticBody;
  }
}

BLB_Physics3DBodyType blb_physics3d_from_b3_body_type(b3BodyType type) {
  switch (type) {
  case b3_dynamicBody:
    return BLB_PHYSICS3D_DYNAMIC;
  case b3_kinematicBody:
    return BLB_PHYSICS3D_KINEMATIC;
  case b3_staticBody:
  default:
    return BLB_PHYSICS3D_STATIC;
  }
}

bool blb_physics3d_append_body(BLB_Box3DBackend *backend, BLB_RigidBody3D *body,
                               size_t count) {
  BLB_RigidBody3D **items = realloc(backend->bodies, (count + 1) * sizeof(*items));

  if (!items)
    return false;

  items[count] = body;
  backend->bodies = items;

  return true;
}

bool blb_physics3d_append_collider(BLB_Box3DBackend *backend, BLB_Collider3D *collider,
                                   size_t count) {
  BLB_Collider3D **items = realloc(backend->colliders, (count + 1) * sizeof(*items));

  if (!items)
    return false;

  items[count] = collider;
  backend->colliders = items;

  return true;
}

bool blb_physics3d_append_body_collider(BLB_RigidBody3D *body, BLB_Collider3D *collider) {
  BLB_Collider3D **items =
      realloc(body->colliders, (body->collider_count + 1) * sizeof(*items));

  if (!items)
    return false;

  items[body->collider_count++] = collider;
  body->colliders = items;

  return true;
}

void blb_physics3d_remove_body(BLB_Box3DBackend *backend, BLB_RigidBody3D *body,
                               size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (backend->bodies[i] != body)
      continue;

    if (i + 1 < count)
      memmove(&backend->bodies[i], &backend->bodies[i + 1],
              (count - i - 1) * sizeof(*backend->bodies));

    if (count == 1) {
      free(backend->bodies);
      backend->bodies = NULL;
    } else {
      BLB_RigidBody3D **items = realloc(backend->bodies, (count - 1) * sizeof(*items));

      if (items)
        backend->bodies = items;
    }

    return;
  }
}

void blb_physics3d_remove_collider(BLB_Box3DBackend *backend, BLB_Collider3D *collider,
                                   size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (backend->colliders[i] != collider)
      continue;

    if (i + 1 < count)
      memmove(&backend->colliders[i], &backend->colliders[i + 1],
              (count - i - 1) * sizeof(*backend->colliders));

    if (count == 1) {
      free(backend->colliders);
      backend->colliders = NULL;
    } else {
      BLB_Collider3D **items = realloc(backend->colliders, (count - 1) * sizeof(*items));

      if (items)
        backend->colliders = items;
    }

    return;
  }
}

void blb_physics3d_remove_body_collider(BLB_RigidBody3D *body, BLB_Collider3D *collider) {
  for (size_t i = 0; i < body->collider_count; ++i) {
    if (body->colliders[i] != collider)
      continue;

    if (i + 1 < body->collider_count)
      memmove(&body->colliders[i], &body->colliders[i + 1],
              (body->collider_count - i - 1) * sizeof(*body->colliders));

    --body->collider_count;

    if (body->collider_count == 0) {
      free(body->colliders);
      body->colliders = NULL;
    } else {
      BLB_Collider3D **items =
          realloc(body->colliders, body->collider_count * sizeof(*items));

      if (items)
        body->colliders = items;
    }

    return;
  }
}

b3Filter blb_physics3d_make_filter(const BLB_Collider3D *collider) {
  BLB_PhysicsLayerMask category = blb_physics3d_layer_bit(collider->filter.layer);
  BLB_PhysicsLayerMask mask = collider->filter.mask;

  const BLB_Box3DBackend *backend =
      blb_physics3d_get_backend_const(collider->body->world);

  if (!collider->active) {
    mask = 0;
  } else if (backend && collider->filter.layer < 64) {
    mask &= backend->layer_masks[collider->filter.layer];
  }

  b3Filter filter = {0};
  filter.categoryBits = category;
  filter.maskBits = mask;
  filter.groupIndex = collider->filter.group;

  return filter;
}

b3Quat blb_to_b3_quat(BLB_Quat rotation) {
  b3Quat result;
  result.v.x = rotation.x;
  result.v.y = rotation.y;
  result.v.z = rotation.z;
  result.s = rotation.w;

  return result;
}

BLB_Quat blb_from_b3_quat(b3Quat rotation) {
  BLB_Quat result;
  result.x = rotation.v.x;
  result.y = rotation.v.y;
  result.z = rotation.v.z;
  result.w = rotation.s;

  return result;
}
