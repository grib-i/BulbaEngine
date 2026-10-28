#include "bulba/core/utils/delta_time.h"
#include "physics3d.h"

BLB_Physics3DWorld *BLB_Physics3D_WorldCreate(void) {
  BLB_Physics3DWorld *world = calloc(1, sizeof(*world));

  if (!world)
    return NULL;

  BLB_Box3DBackend *backend = calloc(1, sizeof(*backend));

  if (!backend) {
    free(world);

    return NULL;
  }

  b3WorldDef def = b3DefaultWorldDef();
  backend->world = world;
  backend->world_id = b3CreateWorld(&def);

  if (!b3World_IsValid(backend->world_id)) {
    free(backend);
    free(world);

    return NULL;
  }

  for (size_t i = 0; i < 64; ++i)
    backend->layer_masks[i] = UINT64_MAX;

  b3Vec3 gravity = b3World_GetGravity(backend->world_id);

  world->gravity = BLB_V3(gravity.x, gravity.y, gravity.z);
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

void BLB_Physics3D_WorldDestroy(BLB_Physics3DWorld *world) {
  if (!world)
    return;

  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);

  if (!backend) {
    free(world);

    return;
  }

  while (world->body_count > 0) {
    BLB_RigidBody3D *body = backend->bodies[world->body_count - 1];
    BLB_Physics3D_BodyDestroy(world, body->object);
  }

  if (b3World_IsValid(backend->world_id))
    b3DestroyWorld(backend->world_id);

  free(backend->bodies);
  free(backend->colliders);
  free(backend);
  free(world);
}

void BLB_Physics3D_WorldUpdate(BLB_Physics3DWorld *world) {
  float delta_time = BLB_DeltaTime_Get();

  if (!blb_physics3d_valid_world(world) || !isfinite(delta_time) || delta_time <= 0.0f ||
      !isfinite(world->fixed_timestep) || world->fixed_timestep <= 0.0f ||
      world->substeps < 1)
    return;

  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);

  for (size_t i = 0; i < world->body_count; ++i)
    blb_object3d_sync_to_body(backend->bodies[i]->object);

  world->accumulator += fminf(delta_time, world->fixed_timestep * 8.0f);

  while (world->accumulator >= world->fixed_timestep) {
    b3World_Step(backend->world_id, world->fixed_timestep, world->substeps);
    world->accumulator -= world->fixed_timestep;
  }

  world->interpolation = world->accumulator / world->fixed_timestep;

  for (size_t i = 0; i < world->body_count; ++i)
    blb_object3d_sync_from_body(backend->bodies[i]->object);
}

void BLB_Physics3D_WorldSetGravity(BLB_Physics3DWorld *world, BLB_Vec3 gravity) {
  if (!blb_physics3d_valid_world(world) || !isfinite(gravity.x) || !isfinite(gravity.y) ||
      !isfinite(gravity.z))
    return;

  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);
  world->gravity = gravity;
  b3World_SetGravity(backend->world_id, (b3Vec3){gravity.x, gravity.y, gravity.z});
}

void BLB_Physics3D_WorldSetFixedTimestep(BLB_Physics3DWorld *world, float timestep) {
  if (!world || !isfinite(timestep) || timestep <= 0.0f)
    return;

  world->fixed_timestep = timestep;
  world->accumulator = 0.0f;
  world->interpolation = 0.0f;
}

void BLB_Physics3D_WorldSetSubsteps(BLB_Physics3DWorld *world, int substeps) {
  if (!world || substeps < 1)
    return;

  world->substeps = substeps;
}

void BLB_Physics3D_WorldSetDebugDraw(BLB_Physics3DWorld *world, bool enabled) {
  if (world)
    world->debug_draw_enabled = enabled;
}

bool BLB_Physics3D_WorldGetDebugDraw(const BLB_Physics3DWorld *world) {
  return world && world->debug_draw_enabled;
}

size_t BLB_Physics3D_WorldGetDebugColliderCount(const BLB_Physics3DWorld *world) {
  const BLB_Box3DBackend *backend = blb_physics3d_get_backend_const(world);

  return blb_physics3d_valid_world(world) && backend ? world->collider_count : 0;
}

bool BLB_Physics3D_WorldGetDebugCollider(const BLB_Physics3DWorld *world, size_t index,
                                         BLB_Physics3DDebugCollider *out_collider) {
  const BLB_Box3DBackend *backend = blb_physics3d_get_backend_const(world);

  if (!out_collider || !blb_physics3d_valid_world(world) || !backend ||
      index >= world->collider_count || !backend->colliders)
    return false;

  const BLB_Collider3D *collider = backend->colliders[index];

  if (!collider || !collider->body)
    return false;

  out_collider->shape = &collider->shape;
  out_collider->position = BLB_Physics3D_BodyGetPosition(collider->body->object);
  out_collider->rotation = BLB_Physics3D_BodyGetRotation(collider->body->object);
  out_collider->active =
      collider->active && BLB_Physics3D_BodyIsActive(collider->body->object);
  out_collider->trigger = collider->trigger;

  return true;
}

void BLB_Physics3D_WorldSetLayerCollision(BLB_Physics3DWorld *world, uint8_t layer_a,
                                          uint8_t layer_b, bool enabled) {
  if (!blb_physics3d_valid_world(world) || layer_a >= 64 || layer_b >= 64)
    return;

  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);
  BLB_PhysicsLayerMask bit_a = blb_physics3d_layer_bit(layer_a);
  BLB_PhysicsLayerMask bit_b = blb_physics3d_layer_bit(layer_b);

  if (enabled) {
    backend->layer_masks[layer_a] |= bit_b;
    backend->layer_masks[layer_b] |= bit_a;
  } else {
    backend->layer_masks[layer_a] &= ~bit_b;
    backend->layer_masks[layer_b] &= ~bit_a;
  }

  for (size_t i = 0; i < world->collider_count; ++i) {
    BLB_Collider3D *collider = backend->colliders[i];
    b3Shape_SetFilter(collider->handle, blb_physics3d_make_filter(collider), true);
  }
}
