#include "bulba/core/physics3d.h"

#include <box3d/box3d.h>

#include <stdlib.h>
#include <string.h>

#define BLB_PHYSICS_DEFAULT_FIXED_TIMESTEP (1.0f / 60.0f)
#define BLB_PHYSICS_DEFAULT_SUBSTEPS 4

struct BLB_Collider3D;

struct BLB_RigidBody3D {
  BLB_Physics3DBodyId id;
  b3BodyId handle;
  BLB_Physics3DWorld *world;
  BLB_Physics3DBodyType type;
  struct BLB_Collider3D **colliders;
  size_t collider_count;
  void *user_data;
};

struct BLB_Collider3D {
  BLB_Physics3DColliderId id;
  b3ShapeId handle;
  BLB_RigidBody3D *body;
  BLB_Physics3DShape shape;
  BLB_PhysicsFilter filter;
  float density;
  float friction;
  float restitution;
  bool trigger;
  bool active;
  void *user_data;
};

typedef struct {
  BLB_Physics3DWorld *world;
  b3WorldId world_id;
  BLB_RigidBody3D **bodies;
  BLB_Collider3D **colliders;
  BLB_PhysicsLayerMask layer_masks[64];
} BLB_Box3DBackend;

static bool valid_world(const BLB_Physics3DWorld *world) {
  return world && world->backend && b3World_IsValid(((const BLB_Box3DBackend *)world->backend)->world_id);
}

static BLB_Box3DBackend *get_backend(BLB_Physics3DWorld *world) {
  return world ? (BLB_Box3DBackend *)world->backend : NULL;
}

static const BLB_Box3DBackend *get_backend_const(const BLB_Physics3DWorld *world) {
  return world ? (const BLB_Box3DBackend *)world->backend : NULL;
}

static BLB_PhysicsLayerMask layer_bit(uint8_t layer) {
  return layer < 64 ? BLB_PhysicsLayer(layer) : 0;
}

static b3BodyType to_b3_body_type(BLB_Physics3DBodyType type) {
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

static BLB_Physics3DBodyType from_b3_body_type(b3BodyType type) {
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

static b3Quat to_b3_quat(HMM_Quat rotation) {
  b3Quat result;
  result.v.x = rotation.x;
  result.v.y = rotation.y;
  result.v.z = rotation.z;
  result.s = rotation.w;
  return result;
}

static HMM_Quat from_b3_quat(b3Quat rotation) {
  HMM_Quat result;
  result.x = rotation.v.x;
  result.y = rotation.v.y;
  result.z = rotation.v.z;
  result.w = rotation.s;
  return result;
}

static void free_shape_copy(BLB_Physics3DShape *shape) {
  if (!shape)
    return;

  if (shape->type == BLB_PHYSICS3D_SHAPE_CONVEX_HULL) {
    free(shape->data.convex_hull.vertices);
    shape->data.convex_hull.vertices = NULL;
    shape->data.convex_hull.vertex_count = 0;
  } else if (shape->type == BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH) {
    free(shape->data.triangle_mesh.vertices);
    free(shape->data.triangle_mesh.indices);
    shape->data.triangle_mesh.vertices = NULL;
    shape->data.triangle_mesh.indices = NULL;
    shape->data.triangle_mesh.vertex_count = 0;
    shape->data.triangle_mesh.index_count = 0;
  }
}

static bool copy_shape(BLB_Physics3DShape *dst, const BLB_Physics3DShape *src) {
  if (!dst || !src)
    return false;

  *dst = *src;

  if (src->type == BLB_PHYSICS3D_SHAPE_CONVEX_HULL) {
    if (!src->data.convex_hull.vertices || src->data.convex_hull.vertex_count < 4)
      return false;

    dst->data.convex_hull.vertices = malloc(src->data.convex_hull.vertex_count * sizeof(*dst->data.convex_hull.vertices));
    if (!dst->data.convex_hull.vertices)
      return false;

    memcpy(dst->data.convex_hull.vertices, src->data.convex_hull.vertices,
           src->data.convex_hull.vertex_count * sizeof(*dst->data.convex_hull.vertices));
  } else if (src->type == BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH) {
    if (!src->data.triangle_mesh.vertices || !src->data.triangle_mesh.indices ||
        src->data.triangle_mesh.vertex_count < 3 || src->data.triangle_mesh.index_count < 3)
      return false;

    dst->data.triangle_mesh.vertices = malloc(src->data.triangle_mesh.vertex_count * sizeof(*dst->data.triangle_mesh.vertices));
    if (!dst->data.triangle_mesh.vertices)
      return false;

    dst->data.triangle_mesh.indices = malloc(src->data.triangle_mesh.index_count * sizeof(*dst->data.triangle_mesh.indices));
    if (!dst->data.triangle_mesh.indices) {
      free(dst->data.triangle_mesh.vertices);
      dst->data.triangle_mesh.vertices = NULL;
      return false;
    }

    memcpy(dst->data.triangle_mesh.vertices, src->data.triangle_mesh.vertices,
           src->data.triangle_mesh.vertex_count * sizeof(*dst->data.triangle_mesh.vertices));
    memcpy(dst->data.triangle_mesh.indices, src->data.triangle_mesh.indices,
           src->data.triangle_mesh.index_count * sizeof(*dst->data.triangle_mesh.indices));
  }

  return true;
}

static bool append_body(BLB_Box3DBackend *backend, BLB_RigidBody3D *body, size_t count) {
  BLB_RigidBody3D **items = realloc(backend->bodies, (count + 1) * sizeof(*items));
  if (!items)
    return false;

  items[count] = body;
  backend->bodies = items;
  return true;
}

static bool append_collider(BLB_Box3DBackend *backend, BLB_Collider3D *collider, size_t count) {
  BLB_Collider3D **items = realloc(backend->colliders, (count + 1) * sizeof(*items));
  if (!items)
    return false;

  items[count] = collider;
  backend->colliders = items;
  return true;
}

static bool append_body_collider(BLB_RigidBody3D *body, BLB_Collider3D *collider) {
  BLB_Collider3D **items = realloc(body->colliders, (body->collider_count + 1) * sizeof(*items));
  if (!items)
    return false;

  items[body->collider_count] = collider;
  body->colliders = items;
  return true;
}

static void remove_body(BLB_Box3DBackend *backend, BLB_RigidBody3D *body, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (backend->bodies[i] != body)
      continue;

    if (i + 1 < count)
      memmove(&backend->bodies[i], &backend->bodies[i + 1], (count - i - 1) * sizeof(*backend->bodies));

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

static void remove_collider(BLB_Box3DBackend *backend, BLB_Collider3D *collider, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (backend->colliders[i] != collider)
      continue;

    if (i + 1 < count)
      memmove(&backend->colliders[i], &backend->colliders[i + 1], (count - i - 1) * sizeof(*backend->colliders));

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

static void remove_body_collider(BLB_RigidBody3D *body, BLB_Collider3D *collider) {
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
      BLB_Collider3D **items = realloc(body->colliders, body->collider_count * sizeof(*items));
      if (items)
        body->colliders = items;
    }

    return;
  }
}

static b3Filter make_b3_filter(const BLB_Collider3D *collider) {
  BLB_PhysicsLayerMask category = layer_bit(collider->filter.layer);
  BLB_PhysicsLayerMask mask = collider->filter.mask;

  const BLB_Box3DBackend *backend = get_backend_const(collider->body->world);

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

static b3ShapeId create_backend_shape(const BLB_Collider3D *collider, const b3ShapeDef *def) {
  if (!collider || !def)
    return b3_nullShapeId;

  b3BodyId body = collider->body->handle;

  switch (collider->shape.type) {
    case BLB_PHYSICS3D_SHAPE_BOX: {
      HMM_Vec3 h = collider->shape.data.box.half_extents;
      b3BoxHull box = b3MakeBoxHull(h.x, h.y, h.z);
      return b3CreateHullShape(body, def, &box.base);
    }

    case BLB_PHYSICS3D_SHAPE_SPHERE: {
      b3Sphere sphere = {
          .center = {0.0f, 0.0f, 0.0f},
          .radius = collider->shape.data.sphere.radius,
      };
      return b3CreateSphereShape(body, def, &sphere);
    }

    case BLB_PHYSICS3D_SHAPE_CAPSULE: {
      b3Capsule capsule = {
          .center1 = {0.0f, -collider->shape.data.capsule.half_height, 0.0f},
          .center2 = {0.0f, collider->shape.data.capsule.half_height, 0.0f},
          .radius = collider->shape.data.capsule.radius,
      };
      return b3CreateCapsuleShape(body, def, &capsule);
    }

    case BLB_PHYSICS3D_SHAPE_CONVEX_HULL: {
      if (!collider->shape.data.convex_hull.vertices || collider->shape.data.convex_hull.vertex_count < 4)
        return b3_nullShapeId;

      b3HullData *hull = b3CreateHull(
          (const b3Vec3 *)collider->shape.data.convex_hull.vertices,
          (int)collider->shape.data.convex_hull.vertex_count,
          (int)collider->shape.data.convex_hull.vertex_count);

      if (!hull)
        return b3_nullShapeId;

      b3ShapeId result = b3CreateHullShape(body, def, hull);
      b3DestroyHull(hull);
      return result;
    }

    case BLB_PHYSICS3D_SHAPE_PLANE:
    case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH:
    case BLB_PHYSICS3D_SHAPE_COMPOUND:
    default:
      return b3_nullShapeId;
  }
}

static bool recreate_shape(BLB_Collider3D *collider, bool trigger) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return false;

  b3ShapeDef def = b3DefaultShapeDef();
  def.density = collider->density;
  def.baseMaterial.friction = collider->friction;
  def.baseMaterial.restitution = collider->restitution;
  def.filter = make_b3_filter(collider);
  def.isSensor = trigger;
  def.updateBodyMass = false;

  b3ShapeId replacement = create_backend_shape(collider, &def);
  if (!b3Shape_IsValid(replacement))
    return false;

  b3ShapeId old = collider->handle;
  collider->handle = replacement;
  collider->trigger = trigger;

  b3Shape_SetUserData(replacement, collider->user_data);
  b3DestroyShape(old, false);
  b3Body_ApplyMassFromShapes(collider->body->handle);

  return true;
}

BLB_Physics3DWorld *BLB_Physics3DWorld_Create(void) {
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

  world->gravity = HMM_V3(gravity.x, gravity.y, gravity.z);
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

void BLB_Physics3DWorld_Destroy(BLB_Physics3DWorld *world) {
  if (!world)
    return;

  BLB_Box3DBackend *backend = get_backend(world);
  if (!backend)
    return free(world);

  while (world->body_count > 0) {
    BLB_RigidBody3D *body = backend->bodies[world->body_count - 1];
    BLB_Physics3D_BodyDestroy(world, body);
  }

  if (b3World_IsValid(backend->world_id))
    b3DestroyWorld(backend->world_id);

  free(backend->bodies);
  free(backend->colliders);
  free(backend);
  free(world);
}

void BLB_Physics3DWorld_Step(BLB_Physics3DWorld *world, float delta_time) {
  if (!valid_world(world) || delta_time <= 0.0f || world->fixed_timestep <= 0.0f || world->substeps < 1)
    return;

  BLB_Box3DBackend *backend = get_backend(world);
  world->accumulator += delta_time;

  while (world->accumulator >= world->fixed_timestep) {
    b3World_Step(backend->world_id, world->fixed_timestep, world->substeps);
    world->accumulator -= world->fixed_timestep;
  }

  world->interpolation = world->accumulator / world->fixed_timestep;
}

void BLB_Physics3DWorld_SetGravity(BLB_Physics3DWorld *world, HMM_Vec3 gravity) {
  if (!valid_world(world))
    return;

  BLB_Box3DBackend *backend = get_backend(world);
  world->gravity = gravity;
  b3World_SetGravity(backend->world_id, (b3Vec3){gravity.x, gravity.y, gravity.z});
}

void BLB_Physics3DWorld_SetFixedTimestep(BLB_Physics3DWorld *world, float timestep) {
  if (!world || timestep <= 0.0f)
    return;

  world->fixed_timestep = timestep;
  world->accumulator = 0.0f;
  world->interpolation = 0.0f;
}

void BLB_Physics3DWorld_SetSubsteps(BLB_Physics3DWorld *world, int substeps) {
  if (!world || substeps < 1)
    return;

  world->substeps = substeps;
}

void BLB_Physics3DWorld_SetLayerCollision(BLB_Physics3DWorld *world, uint8_t layer_a, uint8_t layer_b, bool enabled) {
  if (!valid_world(world) || layer_a >= 64 || layer_b >= 64)
    return;

  BLB_Box3DBackend *backend = get_backend(world);
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
    BLB_Collider3D *collider = backend->colliders[i];
    b3Shape_SetFilter(collider->handle, make_b3_filter(collider), true);
  }
}

BLB_RigidBody3D *BLB_Physics3D_BodyCreate(BLB_Physics3DWorld *world, BLB_Physics3DBodyType type) {
  if (!valid_world(world))
    return NULL;

  BLB_Box3DBackend *backend = get_backend(world);
  b3BodyDef def = b3DefaultBodyDef();
  def.type = to_b3_body_type(type);

  b3BodyId handle = b3CreateBody(backend->world_id, &def);
  if (!b3Body_IsValid(handle))
    return NULL;

  BLB_RigidBody3D *body = calloc(1, sizeof(*body));
  if (!body) {
    b3DestroyBody(handle);
    return NULL;
  }

  body->id = world->next_body_id++;
  body->handle = handle;
  body->world = world;
  body->type = type;
  body->user_data = NULL;

  b3Body_SetUserData(handle, body);

  if (!append_body(backend, body, world->body_count)) {
    b3DestroyBody(handle);
    free(body);
    return NULL;
  }

  ++world->body_count;
  return body;
}

void BLB_Physics3D_BodyDestroy(BLB_Physics3DWorld *world, BLB_RigidBody3D *body) {
  if (!world || !body || body->world != world)
    return;

  BLB_Box3DBackend *backend = get_backend(world);

  if (b3Body_IsValid(body->handle))
    b3DestroyBody(body->handle);

  while (body->collider_count > 0) {
    BLB_Collider3D *collider = body->colliders[body->collider_count - 1];
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

BLB_Physics3DBodyId BLB_Physics3D_BodyGetId(const BLB_RigidBody3D *body) {
  return body ? body->id : 0;
}

void BLB_Physics3D_BodySetType(BLB_RigidBody3D *body, BLB_Physics3DBodyType type) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Body_SetType(body->handle, to_b3_body_type(type));
  body->type = type;
}

BLB_Physics3DBodyType BLB_Physics3D_BodyGetType(const BLB_RigidBody3D *body) {
  if (!body || !b3Body_IsValid(body->handle))
    return BLB_PHYSICS3D_STATIC;

  return from_b3_body_type(b3Body_GetType(body->handle));
}

void BLB_Physics3D_BodySetPosition(BLB_RigidBody3D *body, HMM_Vec3 position) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Quat rotation = b3Body_GetRotation(body->handle);
  b3Body_SetTransform(body->handle, (b3Pos){position.x, position.y, position.z}, rotation);
}

HMM_Vec3 BLB_Physics3D_BodyGetPosition(const BLB_RigidBody3D *body) {
  if (!body || !b3Body_IsValid(body->handle))
    return HMM_V3(0.0f, 0.0f, 0.0f);

  b3Pos position = b3Body_GetPosition(body->handle);
  return HMM_V3((float)position.x, (float)position.y, (float)position.z);
}

void BLB_Physics3D_BodySetRotation(BLB_RigidBody3D *body, HMM_Quat rotation) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Pos position = b3Body_GetPosition(body->handle);
  b3Body_SetTransform(body->handle, position, to_b3_quat(rotation));
}

HMM_Quat BLB_Physics3D_BodyGetRotation(const BLB_RigidBody3D *body) {
  if (!body || !b3Body_IsValid(body->handle)) {
    HMM_Quat identity = {0};
    identity.w = 1.0f;
    return identity;
  }

  return from_b3_quat(b3Body_GetRotation(body->handle));
}

void BLB_Physics3D_BodySetVelocity(BLB_RigidBody3D *body, HMM_Vec3 velocity) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Body_SetLinearVelocity(body->handle, (b3Vec3){velocity.x, velocity.y, velocity.z});
}

HMM_Vec3 BLB_Physics3D_BodyGetVelocity(const BLB_RigidBody3D *body) {
  if (!body || !b3Body_IsValid(body->handle))
    return HMM_V3(0.0f, 0.0f, 0.0f);

  b3Vec3 velocity = b3Body_GetLinearVelocity(body->handle);
  return HMM_V3(velocity.x, velocity.y, velocity.z);
}

void BLB_Physics3D_BodySetAngularVelocity(BLB_RigidBody3D *body, HMM_Vec3 velocity) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Body_SetAngularVelocity(body->handle, (b3Vec3){velocity.x, velocity.y, velocity.z});
}

HMM_Vec3 BLB_Physics3D_BodyGetAngularVelocity(const BLB_RigidBody3D *body) {
  if (!body || !b3Body_IsValid(body->handle))
    return HMM_V3(0.0f, 0.0f, 0.0f);

  b3Vec3 velocity = b3Body_GetAngularVelocity(body->handle);
  return HMM_V3(velocity.x, velocity.y, velocity.z);
}

void BLB_Physics3D_BodyApplyForce(BLB_RigidBody3D *body, HMM_Vec3 force) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Body_ApplyForceToCenter(body->handle, (b3Vec3){force.x, force.y, force.z}, true);
}

void BLB_Physics3D_BodyApplyImpulse(BLB_RigidBody3D *body, HMM_Vec3 impulse) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  b3Body_ApplyLinearImpulseToCenter(body->handle, (b3Vec3){impulse.x, impulse.y, impulse.z}, true);
}

void BLB_Physics3D_BodySetGravityScale(BLB_RigidBody3D *body, float scale) {
  if (body && b3Body_IsValid(body->handle))
    b3Body_SetGravityScale(body->handle, scale);
}

void BLB_Physics3D_BodySetLinearDamping(BLB_RigidBody3D *body, float damping) {
  if (body && b3Body_IsValid(body->handle))
    b3Body_SetLinearDamping(body->handle, damping);
}

void BLB_Physics3D_BodySetAngularDamping(BLB_RigidBody3D *body, float damping) {
  if (body && b3Body_IsValid(body->handle))
    b3Body_SetAngularDamping(body->handle, damping);
}

void BLB_Physics3D_BodySetActive(BLB_RigidBody3D *body, bool active) {
  if (!body || !b3Body_IsValid(body->handle))
    return;

  if (active)
    b3Body_Enable(body->handle);
  else
    b3Body_Disable(body->handle);
}

bool BLB_Physics3D_BodyIsActive(const BLB_RigidBody3D *body) {
  return body && b3Body_IsValid(body->handle) && b3Body_IsEnabled(body->handle);
}

BLB_Collider3D *BLB_Physics3D_ColliderCreate(BLB_RigidBody3D *body, const BLB_Physics3DShape *shape) {
  if (!body || !shape || !b3Body_IsValid(body->handle))
    return NULL;

  BLB_Collider3D *collider = calloc(1, sizeof(*collider));
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

  b3ShapeDef def = b3DefaultShapeDef();
  def.filter = make_b3_filter(collider);
  def.userData = collider->user_data;

  b3ShapeId handle = create_backend_shape(collider, &def);
  if (!b3Shape_IsValid(handle)) {
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  collider->handle = handle;
  collider->density = b3Shape_GetDensity(handle);
  collider->friction = b3Shape_GetFriction(handle);
  collider->restitution = b3Shape_GetRestitution(handle);

  b3Shape_SetUserData(handle, collider->user_data);

  BLB_Physics3DWorld *world = body->world;
  BLB_Box3DBackend *backend = get_backend(world);

  if (!append_body_collider(body, collider)) {
    b3DestroyShape(handle, true);
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  if (!append_collider(backend, collider, world->collider_count)) {
    remove_body_collider(body, collider);
    b3DestroyShape(handle, true);
    free_shape_copy(&collider->shape);
    free(collider);
    return NULL;
  }

  ++world->collider_count;
  return collider;
}

void BLB_Physics3D_ColliderDestroy(BLB_Collider3D *collider) {
  if (!collider || !collider->body || !collider->body->world)
    return;

  BLB_Physics3DWorld *world = collider->body->world;
  BLB_Box3DBackend *backend = get_backend(world);

  if (b3Shape_IsValid(collider->handle))
    b3DestroyShape(collider->handle, true);

  remove_body_collider(collider->body, collider);
  remove_collider(backend, collider, world->collider_count);

  if (world->collider_count > 0)
    --world->collider_count;

  free_shape_copy(&collider->shape);
  free(collider);
}

BLB_Physics3DColliderId BLB_Physics3D_ColliderGetId(const BLB_Collider3D *collider) {
  return collider ? collider->id : 0;
}

void BLB_Physics3D_ColliderSetFilter(BLB_Collider3D *collider, BLB_PhysicsFilter filter) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return;

  collider->filter = filter;
  b3Shape_SetFilter(collider->handle, make_b3_filter(collider), true);
}

BLB_PhysicsFilter BLB_Physics3D_ColliderGetFilter(const BLB_Collider3D *collider) {
  BLB_PhysicsFilter filter = {0, 0, 0};
  if (collider)
    filter = collider->filter;
  return filter;
}

void BLB_Physics3D_ColliderSetLayer(BLB_Collider3D *collider, uint8_t layer) {
  if (!collider || layer >= 64)
    return;

  collider->filter.layer = layer;
  if (b3Shape_IsValid(collider->handle))
    b3Shape_SetFilter(collider->handle, make_b3_filter(collider), true);
}

void BLB_Physics3D_ColliderSetMask(BLB_Collider3D *collider, BLB_PhysicsLayerMask mask) {
  if (!collider)
    return;

  collider->filter.mask = mask;
  if (b3Shape_IsValid(collider->handle))
    b3Shape_SetFilter(collider->handle, make_b3_filter(collider), true);
}

void BLB_Physics3D_ColliderSetGroup(BLB_Collider3D *collider, int32_t group) {
  if (!collider)
    return;

  collider->filter.group = group;
  if (b3Shape_IsValid(collider->handle))
    b3Shape_SetFilter(collider->handle, make_b3_filter(collider), true);
}

void BLB_Physics3D_ColliderSetDensity(BLB_Collider3D *collider, float density) {
  if (!collider || density < 0.0f || !b3Shape_IsValid(collider->handle))
    return;

  collider->density = density;
  b3Shape_SetDensity(collider->handle, density, true);
}

float BLB_Physics3D_ColliderGetDensity(const BLB_Collider3D *collider) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return 0.0f;

  return b3Shape_GetDensity(collider->handle);
}

void BLB_Physics3D_ColliderSetFriction(BLB_Collider3D *collider, float friction) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return;

  collider->friction = friction;
  b3Shape_SetFriction(collider->handle, friction);
}

float BLB_Physics3D_ColliderGetFriction(const BLB_Collider3D *collider) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return 0.0f;

  return b3Shape_GetFriction(collider->handle);
}

void BLB_Physics3D_ColliderSetRestitution(BLB_Collider3D *collider, float restitution) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return;

  collider->restitution = restitution;
  b3Shape_SetRestitution(collider->handle, restitution);
}

float BLB_Physics3D_ColliderGetRestitution(const BLB_Collider3D *collider) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return 0.0f;

  return b3Shape_GetRestitution(collider->handle);
}

void BLB_Physics3D_ColliderSetTrigger(BLB_Collider3D *collider, bool trigger) {
  if (!collider || collider->trigger == trigger)
    return;

  recreate_shape(collider, trigger);
}

bool BLB_Physics3D_ColliderIsTrigger(const BLB_Collider3D *collider) {
  return collider ? collider->trigger : false;
}

void BLB_Physics3D_ColliderSetActive(BLB_Collider3D *collider, bool active) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return;

  collider->active = active;
  b3Shape_SetFilter(collider->handle, make_b3_filter(collider), true);
}

bool BLB_Physics3D_ColliderIsActive(const BLB_Collider3D *collider) {
  return collider ? collider->active : false;
}
