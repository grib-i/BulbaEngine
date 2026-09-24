#include "bulba/core/physics2d.h"

#include <box2d/box2d.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define BLB_PHYSICS_DEFAULT_FIXED_TIMESTEP (1.0f / 60.0f)
#define BLB_PHYSICS_DEFAULT_SUBSTEPS 4

struct BLB_Collider2D;

struct BLB_RigidBody2D {
  BLB_Physics2DBodyId id;
  b2BodyId handle;
  BLB_Physics2DWorld *world;
  BLB_Physics2DBodyType type;
  struct BLB_Collider2D **colliders;
  size_t collider_count;
  void *user_data;
};

struct BLB_Collider2D {
  BLB_Physics2DColliderId id;
  b2ShapeId handle;
  BLB_RigidBody2D *body;
  BLB_Physics2DShape shape;
  BLB_PhysicsFilter filter;
  float density;
  float friction;
  float restitution;
  bool trigger;
  bool active;
  void *user_data;
};

typedef struct {
  BLB_Physics2DWorld *world;
  b2WorldId world_id;
  BLB_RigidBody2D **bodies;
  BLB_Collider2D **colliders;
  BLB_PhysicsLayerMask layer_masks[64];
} BLB_Box2DBackend;

static bool valid_world(const BLB_Physics2DWorld *world) {
  return world && world->backend && b2World_IsValid(((const BLB_Box2DBackend *)world->backend)->world_id);
}

static BLB_Box2DBackend *get_backend(BLB_Physics2DWorld *world) {
  return world ? (BLB_Box2DBackend *)world->backend : NULL;
}

static const BLB_Box2DBackend *get_backend_const(const BLB_Physics2DWorld *world) {
  return world ? (const BLB_Box2DBackend *)world->backend : NULL;
}

static BLB_PhysicsLayerMask layer_bit(uint8_t layer) {
  return layer < 64 ? BLB_PhysicsLayer(layer) : 0;
}

static b2BodyType to_b2_body_type(BLB_Physics2DBodyType type) {
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

static BLB_Physics2DBodyType from_b2_body_type(b2BodyType type) {
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

static void free_shape_copy(BLB_Physics2DShape *shape) {
  if (!shape)
    return;

  if (shape->type == BLB_PHYSICS2D_SHAPE_POLYGON) {
    free(shape->data.polygon.vertices);
    shape->data.polygon.vertices = NULL;
    shape->data.polygon.vertex_count = 0;
  } else if (shape->type == BLB_PHYSICS2D_SHAPE_CHAIN) {
    free(shape->data.chain.vertices);
    shape->data.chain.vertices = NULL;
    shape->data.chain.vertex_count = 0;
  }
}

static bool copy_shape(BLB_Physics2DShape *dst, const BLB_Physics2DShape *src) {
  if (!dst || !src)
    return false;

  *dst = *src;

  if (src->type == BLB_PHYSICS2D_SHAPE_POLYGON) {
    if (src->data.polygon.vertex_count == 0 || !src->data.polygon.vertices) {
      dst->data.polygon.vertices = NULL;
      dst->data.polygon.vertex_count = 0;
      return false;
    }

    dst->data.polygon.vertices = malloc(src->data.polygon.vertex_count * sizeof(*dst->data.polygon.vertices));
    if (!dst->data.polygon.vertices)
      return false;

    memcpy(dst->data.polygon.vertices, src->data.polygon.vertices,
           src->data.polygon.vertex_count * sizeof(*dst->data.polygon.vertices));
  } else if (src->type == BLB_PHYSICS2D_SHAPE_CHAIN) {
    if (src->data.chain.vertex_count == 0 || !src->data.chain.vertices) {
      dst->data.chain.vertices = NULL;
      dst->data.chain.vertex_count = 0;
      return false;
    }

    dst->data.chain.vertices = malloc(src->data.chain.vertex_count * sizeof(*dst->data.chain.vertices));
    if (!dst->data.chain.vertices)
      return false;

    memcpy(dst->data.chain.vertices, src->data.chain.vertices,
           src->data.chain.vertex_count * sizeof(*dst->data.chain.vertices));
  }

  return true;
}

static bool append_body(BLB_Box2DBackend *backend, BLB_RigidBody2D *body, size_t count) {
  BLB_RigidBody2D **items = realloc(backend->bodies, (count + 1) * sizeof(*items));
  if (!items)
    return false;

  items[count] = body;
  backend->bodies = items;
  return true;
}

static bool append_collider(BLB_Box2DBackend *backend, BLB_Collider2D *collider, size_t count) {
  BLB_Collider2D **items = realloc(backend->colliders, (count + 1) * sizeof(*items));
  if (!items)
    return false;

  items[count] = collider;
  backend->colliders = items;
  return true;
}

static bool append_body_collider(BLB_RigidBody2D *body, BLB_Collider2D *collider) {
  BLB_Collider2D **items = realloc(body->colliders, (body->collider_count + 1) * sizeof(*items));
  if (!items)
    return false;

  items[body->collider_count] = collider;
  body->colliders = items;
  return true;
}

static void remove_body(BLB_Box2DBackend *backend, BLB_RigidBody2D *body, size_t count) {
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

static void remove_collider(BLB_Box2DBackend *backend, BLB_Collider2D *collider, size_t count) {
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

static void remove_body_collider(BLB_RigidBody2D *body, BLB_Collider2D *collider) {
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
      BLB_Collider2D **items = realloc(body->colliders, body->collider_count * sizeof(*items));
      if (items)
        body->colliders = items;
    }

    return;
  }
}

static b2Filter make_b2_filter(const BLB_Collider2D *collider) {
  BLB_PhysicsLayerMask category = layer_bit(collider->filter.layer);
  BLB_PhysicsLayerMask mask = collider->filter.mask;

  const BLB_Box2DBackend *backend = get_backend_const(collider->body->world);

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

static b2ShapeId create_backend_shape(const BLB_Collider2D *collider, const b2ShapeDef *def) {
  if (!collider || !def)
    return b2_nullShapeId;

  b2BodyId body = collider->body->handle;

  switch (collider->shape.type) {
    case BLB_PHYSICS2D_SHAPE_BOX: {
      b2Polygon polygon = b2MakeBox(collider->shape.data.box.half_extents.x, collider->shape.data.box.half_extents.y);
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
          .point1 = {collider->shape.data.segment.point1.x, collider->shape.data.segment.point1.y},
          .point2 = {collider->shape.data.segment.point2.x, collider->shape.data.segment.point2.y},
      };
      return b2CreateSegmentShape(body, def, &segment);
    }

    case BLB_PHYSICS2D_SHAPE_POLYGON: {
      if (!collider->shape.data.polygon.vertices || collider->shape.data.polygon.vertex_count < 3 ||
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

    case BLB_PHYSICS2D_SHAPE_CHAIN:
    default:
      return b2_nullShapeId;
  }
}

static bool recreate_shape(BLB_Collider2D *collider, bool trigger) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return false;

  b2ShapeDef def = b2DefaultShapeDef();
  def.density = collider->density;
  def.filter = make_b2_filter(collider);
  def.isSensor = trigger;
  def.updateBodyMass = false;
  def.material.friction = collider->friction;
  def.material.restitution = collider->restitution;

  b2ShapeId replacement = create_backend_shape(collider, &def);
  if (!b2Shape_IsValid(replacement))
    return false;

  b2ShapeId old = collider->handle;
  collider->handle = replacement;
  collider->trigger = trigger;

  b2Shape_SetUserData(replacement, collider->user_data);
  b2DestroyShape(old, false);
  b2Body_ApplyMassFromShapes(collider->body->handle);

  return true;
}

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

  BLB_Box2DBackend *backend = get_backend(world);
  if (!backend)
    return free(world);

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
  if (!valid_world(world) || delta_time <= 0.0f || world->fixed_timestep <= 0.0f || world->substeps < 1)
    return;

  BLB_Box2DBackend *backend = get_backend(world);
  world->accumulator += delta_time;

  while (world->accumulator >= world->fixed_timestep) {
    b2World_Step(backend->world_id, world->fixed_timestep, world->substeps);
    world->accumulator -= world->fixed_timestep;
  }

  world->interpolation = world->accumulator / world->fixed_timestep;
}

void BLB_Physics2DWorld_SetGravity(BLB_Physics2DWorld *world, HMM_Vec2 gravity) {
  if (!valid_world(world))
    return;

  BLB_Box2DBackend *backend = get_backend(world);
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
  if (!valid_world(world) || layer_a >= 64 || layer_b >= 64)
    return;

  BLB_Box2DBackend *backend = get_backend(world);
  BLB_PhysicsLayerMask bit_a = layer_bit(layer_a);
  BLB_PhysicsLayerMask bit_b = layer_bit(layer_b);

  if (enabled) {
    backend->layer_masks[layer_a] |= bit_b;
    backend->layer_masks[layer_b] |= bit_a;
  } else {
    backend->layer_masks[layer_a] &= ~bit_b;
    backend->layer_masks[layer_b] &= ~bit_a;
  }

  for (size_t i = 0; i < world->collider_count; ++i) {
    BLB_Collider2D *collider = backend->colliders[i];
    b2Shape_SetFilter(collider->handle, make_b2_filter(collider));
  }
}

BLB_RigidBody2D *BLB_Physics2D_BodyCreate(BLB_Physics2DWorld *world, BLB_Physics2DBodyType type) {
  if (!valid_world(world))
    return NULL;

  BLB_Box2DBackend *backend = get_backend(world);
  b2BodyDef def = b2DefaultBodyDef();
  def.type = to_b2_body_type(type);

  b2BodyId handle = b2CreateBody(backend->world_id, &def);
  if (!b2Body_IsValid(handle))
    return NULL;

  BLB_RigidBody2D *body = calloc(1, sizeof(*body));
  if (!body) {
    b2DestroyBody(handle);
    return NULL;
  }

  body->id = world->next_body_id++;
  body->handle = handle;
  body->world = world;
  body->type = type;
  body->user_data = NULL;

  b2Body_SetUserData(handle, body);

  if (!append_body(backend, body, world->body_count)) {
    b2DestroyBody(handle);
    free(body);
    return NULL;
  }

  ++world->body_count;
  return body;
}

void BLB_Physics2D_BodyDestroy(BLB_Physics2DWorld *world, BLB_RigidBody2D *body) {
  if (!world || !body || body->world != world)
    return;

  BLB_Box2DBackend *backend = get_backend(world);

  if (b2Body_IsValid(body->handle))
    b2DestroyBody(body->handle);

  while (body->collider_count > 0) {
    BLB_Collider2D *collider = body->colliders[body->collider_count - 1];
    remove_collider(backend, collider, world->collider_count);
    if (world->collider_count > 0)
      --world->collider_count;
    free_shape_copy(&collider->shape);
    free(collider);
    --body->collider_count;
  }

  free(body->colliders);
  body->colliders = NULL;

  remove_body(backend, body, world->body_count);
  if (world->body_count > 0)
    --world->body_count;

  free(body);
}

BLB_Physics2DBodyId BLB_Physics2D_BodyGetId(const BLB_RigidBody2D *body) {
  return body ? body->id : 0;
}

void BLB_Physics2D_BodySetType(BLB_RigidBody2D *body, BLB_Physics2DBodyType type) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  b2Body_SetType(body->handle, to_b2_body_type(type));
  body->type = type;
}

BLB_Physics2DBodyType BLB_Physics2D_BodyGetType(const BLB_RigidBody2D *body) {
  if (!body || !b2Body_IsValid(body->handle))
    return BLB_PHYSICS2D_STATIC;

  return from_b2_body_type(b2Body_GetType(body->handle));
}

void BLB_Physics2D_BodySetPosition(BLB_RigidBody2D *body, HMM_Vec2 position) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  b2Rot rotation = b2Body_GetRotation(body->handle);
  b2Body_SetTransform(body->handle, (b2Vec2){position.x, position.y}, rotation);
}

HMM_Vec2 BLB_Physics2D_BodyGetPosition(const BLB_RigidBody2D *body) {
  if (!body || !b2Body_IsValid(body->handle))
    return HMM_V2(0.0f, 0.0f);

  b2Vec2 position = b2Body_GetPosition(body->handle);
  return HMM_V2(position.x, position.y);
}

void BLB_Physics2D_BodySetRotation(BLB_RigidBody2D *body, float rotation) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  b2Vec2 position = b2Body_GetPosition(body->handle);
  b2Body_SetTransform(body->handle, position, b2MakeRot(rotation));
}

float BLB_Physics2D_BodyGetRotation(const BLB_RigidBody2D *body) {
  if (!body || !b2Body_IsValid(body->handle))
    return 0.0f;

  b2Rot rotation = b2Body_GetRotation(body->handle);
  return atan2f(rotation.s, rotation.c);
}

void BLB_Physics2D_BodySetVelocity(BLB_RigidBody2D *body, HMM_Vec2 velocity) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  b2Body_SetLinearVelocity(body->handle, (b2Vec2){velocity.x, velocity.y});
}

HMM_Vec2 BLB_Physics2D_BodyGetVelocity(const BLB_RigidBody2D *body) {
  if (!body || !b2Body_IsValid(body->handle))
    return HMM_V2(0.0f, 0.0f);

  b2Vec2 velocity = b2Body_GetLinearVelocity(body->handle);
  return HMM_V2(velocity.x, velocity.y);
}

void BLB_Physics2D_BodyApplyForce(BLB_RigidBody2D *body, HMM_Vec2 force) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  b2Body_ApplyForceToCenter(body->handle, (b2Vec2){force.x, force.y}, true);
}

void BLB_Physics2D_BodyApplyImpulse(BLB_RigidBody2D *body, HMM_Vec2 impulse) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  b2Body_ApplyLinearImpulseToCenter(body->handle, (b2Vec2){impulse.x, impulse.y}, true);
}

void BLB_Physics2D_BodySetGravityScale(BLB_RigidBody2D *body, float scale) {
  if (body && b2Body_IsValid(body->handle))
    b2Body_SetGravityScale(body->handle, scale);
}

void BLB_Physics2D_BodySetLinearDamping(BLB_RigidBody2D *body, float damping) {
  if (body && b2Body_IsValid(body->handle))
    b2Body_SetLinearDamping(body->handle, damping);
}

void BLB_Physics2D_BodySetAngularDamping(BLB_RigidBody2D *body, float damping) {
  if (body && b2Body_IsValid(body->handle))
    b2Body_SetAngularDamping(body->handle, damping);
}

void BLB_Physics2D_BodySetActive(BLB_RigidBody2D *body, bool active) {
  if (!body || !b2Body_IsValid(body->handle))
    return;

  if (active)
    b2Body_Enable(body->handle);
  else
    b2Body_Disable(body->handle);
}

bool BLB_Physics2D_BodyIsActive(const BLB_RigidBody2D *body) {
  return body && b2Body_IsValid(body->handle) && b2Body_IsEnabled(body->handle);
}

BLB_Collider2D *BLB_Physics2D_ColliderCreate(BLB_RigidBody2D *body, const BLB_Physics2DShape *shape) {
  if (!body || !shape || !b2Body_IsValid(body->handle))
    return NULL;

  BLB_Collider2D *collider = calloc(1, sizeof(*collider));
  if (!collider)
    return NULL;

  if (!copy_shape(&collider->shape, shape)) {
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  collider->body = body;
  collider->id = body->world->next_collider_id++;
  collider->filter.layer = 0;
  collider->filter.mask = UINT64_MAX;
  collider->filter.group = 0;
  collider->trigger = false;
  collider->active = true;
  collider->user_data = shape->user_data;

  b2ShapeDef def = b2DefaultShapeDef();
  def.filter = make_b2_filter(collider);
  def.userData = collider->user_data;

  b2ShapeId handle = create_backend_shape(collider, &def);
  if (!b2Shape_IsValid(handle)) {
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  collider->handle = handle;
  collider->density = b2Shape_GetDensity(handle);
  collider->friction = b2Shape_GetFriction(handle);
  collider->restitution = b2Shape_GetRestitution(handle);

  b2Shape_SetUserData(handle, collider->user_data);

  BLB_Physics2DWorld *world = body->world;
  BLB_Box2DBackend *backend = get_backend(world);

  if (!append_body_collider(body, collider)) {
    b2DestroyShape(handle, true);
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  if (!append_collider(backend, collider, world->collider_count)) {
    remove_body_collider(body, collider);
    b2DestroyShape(handle, true);
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  ++world->collider_count;
  return collider;
}

void BLB_Physics2D_ColliderDestroy(BLB_Collider2D *collider) {
  if (!collider || !collider->body || !collider->body->world)
    return;

  BLB_Physics2DWorld *world = collider->body->world;
  BLB_Box2DBackend *backend = get_backend(world);

  if (b2Shape_IsValid(collider->handle))
    b2DestroyShape(collider->handle, true);

  remove_body_collider(collider->body, collider);
  remove_collider(backend, collider, world->collider_count);

  if (world->collider_count > 0)
    --world->collider_count;

  free_shape_copy(&collider->shape);
  free(collider);
}

BLB_Physics2DColliderId BLB_Physics2D_ColliderGetId(const BLB_Collider2D *collider) {
  return collider ? collider->id : 0;
}

void BLB_Physics2D_ColliderSetFilter(BLB_Collider2D *collider, BLB_PhysicsFilter filter) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return;

  collider->filter = filter;
  b2Shape_SetFilter(collider->handle, make_b2_filter(collider));
}

BLB_PhysicsFilter BLB_Physics2D_ColliderGetFilter(const BLB_Collider2D *collider) {
  BLB_PhysicsFilter filter = {0, 0, 0};
  if (collider)
    filter = collider->filter;
  return filter;
}

void BLB_Physics2D_ColliderSetLayer(BLB_Collider2D *collider, uint8_t layer) {
  if (!collider || layer >= 64)
    return;

  collider->filter.layer = layer;
  if (b2Shape_IsValid(collider->handle))
    b2Shape_SetFilter(collider->handle, make_b2_filter(collider));
}

void BLB_Physics2D_ColliderSetMask(BLB_Collider2D *collider, BLB_PhysicsLayerMask mask) {
  if (!collider)
    return;

  collider->filter.mask = mask;
  if (b2Shape_IsValid(collider->handle))
    b2Shape_SetFilter(collider->handle, make_b2_filter(collider));
}

void BLB_Physics2D_ColliderSetGroup(BLB_Collider2D *collider, int32_t group) {
  if (!collider)
    return;

  collider->filter.group = group;
  if (b2Shape_IsValid(collider->handle))
    b2Shape_SetFilter(collider->handle, make_b2_filter(collider));
}

void BLB_Physics2D_ColliderSetDensity(BLB_Collider2D *collider, float density) {
  if (!collider || density < 0.0f || !b2Shape_IsValid(collider->handle))
    return;

  collider->density = density;
  b2Shape_SetDensity(collider->handle, density, true);
}

float BLB_Physics2D_ColliderGetDensity(const BLB_Collider2D *collider) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return 0.0f;

  return b2Shape_GetDensity(collider->handle);
}

void BLB_Physics2D_ColliderSetFriction(BLB_Collider2D *collider, float friction) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return;

  collider->friction = friction;
  b2Shape_SetFriction(collider->handle, friction);
}

float BLB_Physics2D_ColliderGetFriction(const BLB_Collider2D *collider) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return 0.0f;

  return b2Shape_GetFriction(collider->handle);
}

void BLB_Physics2D_ColliderSetRestitution(BLB_Collider2D *collider, float restitution) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return;

  collider->restitution = restitution;
  b2Shape_SetRestitution(collider->handle, restitution);
}

float BLB_Physics2D_ColliderGetRestitution(const BLB_Collider2D *collider) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return 0.0f;

  return b2Shape_GetRestitution(collider->handle);
}

void BLB_Physics2D_ColliderSetTrigger(BLB_Collider2D *collider, bool trigger) {
  if (!collider || collider->trigger == trigger)
    return;

  recreate_shape(collider, trigger);
}

bool BLB_Physics2D_ColliderIsTrigger(const BLB_Collider2D *collider) {
  return collider ? collider->trigger : false;
}

void BLB_Physics2D_ColliderSetActive(BLB_Collider2D *collider, bool active) {
  if (!collider || !b2Shape_IsValid(collider->handle))
    return;

  collider->active = active;
  b2Shape_SetFilter(collider->handle, make_b2_filter(collider));
}

bool BLB_Physics2D_ColliderIsActive(const BLB_Collider2D *collider) {
  return collider ? collider->active : false;
}
