#include "bulba/core/physics.h"

#include <math.h>
#include <stdlib.h>

static float clamp_dt(float value) {
  if (value < 0.0f)
    return 0.0f;
  if (value > 0.25f)
    return 0.25f;
  return value;
}

BLB_PhysicsWorld *BLB_PhysicsWorld_Create(void) {
  BLB_PhysicsWorld *world = calloc(1, sizeof(*world));
  if (!world)
    return NULL;
  world->gravity = HMM_V3(0.0f, -9.81f, 0.0f);
  world->fixed_timestep = 1.0f / 60.0f;
  world->next_body_id = 1;
  world->next_collider_id = 1;
  return world;
}

void BLB_PhysicsWorld_Destroy(BLB_PhysicsWorld *world) {
  if (!world)
    return;
  for (size_t i = 0; i < world->collider_count; i++)
    free(world->colliders[i]);
  for (size_t i = 0; i < world->body_count; i++)
    free(world->bodies[i]);
  free(world->colliders);
  free(world->bodies);
  free(world);
}

BLB_RigidBody *BLB_PhysicsWorld_CreateBody(BLB_PhysicsWorld *world, BLB_PhysicsBodyType type) {
  if (!world)
    return NULL;
  BLB_RigidBody *body = calloc(1, sizeof(*body));
  if (!body)
    return NULL;
  body->id = world->next_body_id++;
  body->type = type;
  body->mass = type == BLB_PHYSICS_DYNAMIC ? 1.0f : 0.0f;
  body->inverse_mass = body->mass > 0.0f ? 1.0f / body->mass : 0.0f;
  body->rotation = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);
  body->linear_damping = 0.02f;
  body->angular_damping = 0.05f;
  body->gravity_scale = 1.0f;
  body->active = true;
  if (type == BLB_PHYSICS_STATIC)
    body->sleeping = true;

  BLB_RigidBody **items = realloc(world->bodies, sizeof(*items) * (world->body_count + 1));
  if (!items) {
    free(body);
    return NULL;
  }
  world->bodies = items;
  world->bodies[world->body_count++] = body;
  return body;
}

BLB_Collider *BLB_PhysicsWorld_CreateCollider(BLB_PhysicsWorld *world, BLB_PhysicsBodyId body_id, const BLB_PhysicsShape *shape) {
  if (!world || !shape)
    return NULL;
  BLB_Collider *collider = calloc(1, sizeof(*collider));
  if (!collider)
    return NULL;
  collider->id = world->next_collider_id++;
  collider->body_id = body_id;
  collider->shape = *shape;
  collider->local_rotation = HMM_Q(0.0f, 0.0f, 0.0f, 1.0f);
  collider->friction = 0.7f;
  collider->restitution = 0.1f;
  collider->active = true;

  BLB_Collider **items = realloc(world->colliders, sizeof(*items) * (world->collider_count + 1));
  if (!items) {
    free(collider);
    return NULL;
  }
  world->colliders = items;
  world->colliders[world->collider_count++] = collider;
  return collider;
}

int BLB_PhysicsWorld_RemoveBody(BLB_PhysicsWorld *world, BLB_PhysicsBodyId id) {
  if (!world)
    return -1;
  for (size_t i = 0; i < world->body_count; i++) {
    if (!world->bodies[i] || world->bodies[i]->id != id)
      continue;
    free(world->bodies[i]);
    world->bodies[i] = world->bodies[world->body_count - 1];
    world->body_count--;
    if (!world->body_count) {
      free(world->bodies);
      world->bodies = NULL;
    } else {
      BLB_RigidBody **items = realloc(world->bodies, sizeof(*items) * world->body_count);
      if (items)
        world->bodies = items;
    }
    return 0;
  }
  return -1;
}

int BLB_PhysicsWorld_RemoveCollider(BLB_PhysicsWorld *world, BLB_ColliderId id) {
  if (!world)
    return -1;
  for (size_t i = 0; i < world->collider_count; i++) {
    if (!world->colliders[i] || world->colliders[i]->id != id)
      continue;
    free(world->colliders[i]);
    world->colliders[i] = world->colliders[world->collider_count - 1];
    world->collider_count--;
    if (!world->collider_count) {
      free(world->colliders);
      world->colliders = NULL;
    } else {
      BLB_Collider **items = realloc(world->colliders, sizeof(*items) * world->collider_count);
      if (items)
        world->colliders = items;
    }
    return 0;
  }
  return -1;
}

static void integrate_body(BLB_PhysicsWorld *world, BLB_RigidBody *body, float dt) {
  if (!body || !body->active || body->sleeping || body->type != BLB_PHYSICS_DYNAMIC)
    return;

  HMM_Vec3 acceleration = HMM_AddV3(HMM_MulV3F(body->force, body->inverse_mass), HMM_MulV3F(world->gravity, body->gravity_scale));
  body->linear_velocity = HMM_AddV3(body->linear_velocity, HMM_MulV3F(acceleration, dt));
  body->linear_velocity = HMM_MulV3F(body->linear_velocity, expf(-body->linear_damping * dt));
  body->position = HMM_AddV3(body->position, HMM_MulV3F(body->linear_velocity, dt));
  body->force = HMM_V3(0.0f, 0.0f, 0.0f);
  body->torque = HMM_V3(0.0f, 0.0f, 0.0f);
}

void BLB_PhysicsWorld_Step(BLB_PhysicsWorld *world, float delta_time) {
  if (!world)
    return;

  world->accumulator += clamp_dt(delta_time);
  while (world->accumulator >= world->fixed_timestep) {
    for (size_t i = 0; i < world->body_count; i++)
      integrate_body(world, world->bodies[i], world->fixed_timestep);
    world->accumulator -= world->fixed_timestep;
  }
  world->interpolation = world->fixed_timestep > 0.0f ? world->accumulator / world->fixed_timestep : 0.0f;
}

void BLB_PhysicsWorld_SetGravity(BLB_PhysicsWorld *world, HMM_Vec3 gravity) {
  if (world)
    world->gravity = gravity;
}

void BLB_PhysicsWorld_SetFixedTimestep(BLB_PhysicsWorld *world, float timestep) {
  if (!world)
    return;
  if (timestep < 0.0001f)
    timestep = 0.0001f;
  if (timestep > 0.1f)
    timestep = 0.1f;
  world->fixed_timestep = timestep;
}
