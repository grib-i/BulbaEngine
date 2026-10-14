#include "physics2d.h"

BLB_Physics2DWorld *BLB_Physics2DWorld_Create(void) {
  BLB_Physics2DWorld *world = calloc(1, sizeof(*world));
  if (!world)
    return NULL;

  BLB_Box2DBackend *backend = calloc(1, sizeof(*backend));
  if (!backend) {
    free(world);
    return NULL;
  }

  b2WorldDef def = b2DefaultWorldDef();
  backend->world = world;
  backend->world_id = b2CreateWorld(&def);

  if (!b2World_IsValid(backend->world_id)) {
    free(backend);
    free(world);
    return NULL;
  }

  for (size_t i = 0; i < 64; ++i)
    backend->layer_masks[i] = UINT64_MAX;

  b2Vec2 gravity = b2World_GetGravity(backend->world_id);

  world->gravity = HMM_V2(gravity.x, gravity.y);
  world->fixed_timestep = BLB_PHYSICS_DEFAULT_FIXED_TIMESTEP;
  world->substeps = BLB_PHYSICS_DEFAULT_SUBSTEPS;
  world->accumulator = 0.0f;
  world->interpolation = 0.0f;
  world->body_count = 0;
  world->collider_count = 0;
  world->next_body_id = 1;
  world->next_collider_id = 1;
  world->backend = backend;

  return world;
}

void BLB_Physics2DWorld_Destroy(BLB_Physics2DWorld *world) {
  if (!world)
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);

  if (!backend) {
    free(world);
    return;
  }

  while (world->body_count > 0) {
    BLB_RigidBody2D *body = backend->bodies[world->body_count - 1];
    BLB_Physics2D_BodyDestroy(world, body);
  }

  if (b2World_IsValid(backend->world_id))
    b2DestroyWorld(backend->world_id);

  free(backend->bodies);
  free(backend->colliders);
  free(backend);
  free(world);
}

void BLB_Physics2DWorld_Step(BLB_Physics2DWorld *world, float delta_time) {
  if (!blb_physics2d_valid_world(world) || delta_time <= 0.0f || world->fixed_timestep <= 0.0f || world->substeps < 1)
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);
  world->accumulator += delta_time;

  while (world->accumulator >= world->fixed_timestep) {
    b2World_Step(backend->world_id, world->fixed_timestep, world->substeps);
    world->accumulator -= world->fixed_timestep;
  }

  world->interpolation = world->accumulator / world->fixed_timestep;
}

void BLB_Physics2DWorld_SetGravity(BLB_Physics2DWorld *world, HMM_Vec2 gravity) {
  if (!blb_physics2d_valid_world(world))
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);
  world->gravity = gravity;
  b2World_SetGravity(backend->world_id, (b2Vec2){gravity.x, gravity.y});
}

void BLB_Physics2DWorld_SetFixedTimestep(BLB_Physics2DWorld *world, float timestep) {
  if (!world || timestep <= 0.0f)
    return;

  world->fixed_timestep = timestep;
  world->accumulator = 0.0f;
  world->interpolation = 0.0f;
}

void BLB_Physics2DWorld_SetSubsteps(BLB_Physics2DWorld *world, int substeps) {
  if (!world || substeps < 1)
    return;

  world->substeps = substeps;
}

void BLB_Physics2DWorld_SetLayerCollision(BLB_Physics2DWorld *world, uint8_t layer_a, uint8_t layer_b, bool enabled) {
  if (!blb_physics2d_valid_world(world) || layer_a >= 64 || layer_b >= 64)
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);
  BLB_PhysicsLayerMask bit_a = blb_physics2d_layer_bit(layer_a);
  BLB_PhysicsLayerMask bit_b = blb_physics2d_layer_bit(layer_b);

  if (enabled) {
    backend->layer_masks[layer_a] |= bit_b;
    backend->layer_masks[layer_b] |= bit_a;
  } else {
    backend->layer_masks[layer_a] &= ~bit_b;
    backend->layer_masks[layer_b] &= ~bit_a;
  }

  for (size_t i = 0; i < world->collider_count; ++i) {
    BLB_Collider2D *collider = backend->colliders[i];
    b2Shape_SetFilter(collider->handle, blb_physics2d_make_filter(collider));
  }
}

void BLB_Physics2DWorld_SetDebugDraw(BLB_Physics2DWorld *world, bool enabled) {
  if (world)
    world->debug_draw_enabled = enabled;
}

bool BLB_Physics2DWorld_GetDebugDraw(const BLB_Physics2DWorld *world) { return world && world->debug_draw_enabled; }

size_t BLB_Physics2DWorld_GetDebugColliderCount(const BLB_Physics2DWorld *world) {
  const BLB_Box2DBackend *backend = blb_physics2d_get_backend_const(world);
  return blb_physics2d_valid_world(world) && backend ? world->collider_count : 0;
}

bool BLB_Physics2DWorld_GetDebugCollider(const BLB_Physics2DWorld *world, size_t index, BLB_Physics2DDebugCollider *out_collider) {
  const BLB_Box2DBackend *backend = blb_physics2d_get_backend_const(world);
  if (!out_collider || !blb_physics2d_valid_world(world) || !backend || index >= world->collider_count || !backend->colliders)
    return false;

  const BLB_Collider2D *collider = backend->colliders[index];

  if (!collider || !collider->body)
    return false;

  out_collider->shape = &collider->shape;
  out_collider->position = BLB_Physics2D_BodyGetPosition(collider->body);
  out_collider->rotation = BLB_Physics2D_BodyGetRotation(collider->body);
  out_collider->active = collider->active && BLB_Physics2D_BodyIsActive(collider->body);
  out_collider->trigger = collider->trigger;
  return true;
}

const BLB_Physics2DShape *BLB_Physics2D_ColliderGetShape(const BLB_Collider2D *collider) { return collider ? &collider->shape : NULL; }
