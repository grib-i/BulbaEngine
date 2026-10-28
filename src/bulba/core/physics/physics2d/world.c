#include "bulba/core/utils/delta_time.h"
#include "physics2d.h"

BLB_Physics2DWorld *BLB_Physics2D_WorldCreate(void) {
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

  world->gravity = BLB_V2(gravity.x, gravity.y);
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

void BLB_Physics2D_WorldDestroy(BLB_Physics2DWorld *world) {
  if (!world)
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);

  if (!backend) {
    free(world);

    return;
  }

  while (world->body_count > 0) {
    BLB_RigidBody2D *body = backend->bodies[world->body_count - 1];
    BLB_Physics2D_BodyDestroy(world, body->object);
  }

  if (b2World_IsValid(backend->world_id))
    b2DestroyWorld(backend->world_id);

  free(backend->bodies);
  free(backend->colliders);
  free(backend);
  free(world);
}

void BLB_Physics2D_WorldUpdate(BLB_Physics2DWorld *world) {
  float delta_time = BLB_DeltaTime_Get();

  if (!blb_physics2d_valid_world(world) || !isfinite(delta_time) || delta_time <= 0.0f ||
      !isfinite(world->fixed_timestep) || world->fixed_timestep <= 0.0f ||
      world->substeps < 1)
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);

  for (size_t i = 0; i < world->body_count; ++i)
    blb_object2d_sync_to_body(backend->bodies[i]->object);

  world->accumulator += fminf(delta_time, world->fixed_timestep * 8.0f);

  while (world->accumulator >= world->fixed_timestep) {
    b2World_Step(backend->world_id, world->fixed_timestep, world->substeps);
    world->accumulator -= world->fixed_timestep;
  }

  world->interpolation = world->accumulator / world->fixed_timestep;

  for (size_t i = 0; i < world->body_count; ++i)
    blb_object2d_sync_from_body(backend->bodies[i]->object);
}

void BLB_Physics2D_WorldSetGravity(BLB_Physics2DWorld *world, BLB_Vec2 gravity) {
  if (!blb_physics2d_valid_world(world) || !isfinite(gravity.x) || !isfinite(gravity.y))
    return;

  BLB_Box2DBackend *backend = blb_physics2d_get_backend(world);
  world->gravity = gravity;
  b2World_SetGravity(backend->world_id, (b2Vec2){gravity.x, gravity.y});
}

void BLB_Physics2D_WorldSetFixedTimestep(BLB_Physics2DWorld *world, float timestep) {
  if (!world || !isfinite(timestep) || timestep <= 0.0f)
    return;

  world->fixed_timestep = timestep;
  world->accumulator = 0.0f;
  world->interpolation = 0.0f;
}

void BLB_Physics2D_WorldSetSubsteps(BLB_Physics2DWorld *world, int substeps) {
  if (!world || substeps < 1)
    return;

  world->substeps = substeps;
}

void BLB_Physics2D_WorldSetLayerCollision(BLB_Physics2DWorld *world, uint8_t layer_a,
                                          uint8_t layer_b, bool enabled) {
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
    b2ShapeId *ids = NULL;
    int count = 0;
    if (!blb_physics2d_collect_shape_ids(collider, &ids, &count))
      continue;
    b2Filter filter = blb_physics2d_make_filter(collider);
    for (int j = 0; j < count; ++j)
      b2Shape_SetFilter(ids[j], filter);
    free(ids);
  }
}

void BLB_Physics2D_WorldSetDebugDraw(BLB_Physics2DWorld *world, bool enabled) {
  if (world)
    world->debug_draw_enabled = enabled;
}

bool BLB_Physics2D_WorldGetDebugDraw(const BLB_Physics2DWorld *world) {
  return world && world->debug_draw_enabled;
}

size_t BLB_Physics2D_WorldGetDebugColliderCount(const BLB_Physics2DWorld *world) {
  const BLB_Box2DBackend *backend = blb_physics2d_get_backend_const(world);

  return blb_physics2d_valid_world(world) && backend ? world->collider_count : 0;
}

bool BLB_Physics2D_WorldGetDebugCollider(const BLB_Physics2DWorld *world, size_t index,
                                         BLB_Physics2DDebugCollider *out_collider) {
  const BLB_Box2DBackend *backend = blb_physics2d_get_backend_const(world);

  if (!out_collider || !blb_physics2d_valid_world(world) || !backend ||
      index >= world->collider_count || !backend->colliders)
    return false;

  const BLB_Collider2D *collider = backend->colliders[index];

  if (!collider || !collider->body)
    return false;

  out_collider->shape = &collider->shape;
  out_collider->position = BLB_Physics2D_BodyGetPosition(collider->body->object);
  out_collider->rotation = BLB_Physics2D_BodyGetRotation(collider->body->object);
  out_collider->active =
      collider->active && BLB_Physics2D_BodyIsActive(collider->body->object);
  out_collider->trigger = collider->trigger;

  return true;
}
