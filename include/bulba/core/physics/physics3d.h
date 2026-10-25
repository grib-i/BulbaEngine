#ifndef BULBA_CORE_PHYSICS_PHYSICS3D_H
#define BULBA_CORE_PHYSICS_PHYSICS3D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/physics/physics.h"
#include "bulba/core/utils/object.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t BLB_Physics3DBodyId;
typedef uint64_t BLB_Physics3DColliderId;

typedef enum { BLB_PHYSICS3D_STATIC = 0, BLB_PHYSICS3D_DYNAMIC = 1, BLB_PHYSICS3D_KINEMATIC = 2 } BLB_Physics3DBodyType;

typedef enum {
  BLB_PHYSICS3D_SHAPE_BOX = 0,
  BLB_PHYSICS3D_SHAPE_SPHERE = 1,
  BLB_PHYSICS3D_SHAPE_CAPSULE = 2,
  BLB_PHYSICS3D_SHAPE_PLANE = 3,
  BLB_PHYSICS3D_SHAPE_CONVEX_HULL = 4,
  BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH = 5,
  BLB_PHYSICS3D_SHAPE_COMPOUND = 6
} BLB_Physics3DShapeType;

typedef struct {
  BLB_Vec3 scale;
} BLB_BoxShape3D;

typedef struct {
  float radius;
} BLB_SphereShape3D;

typedef struct {
  float radius;
  float height;
} BLB_CapsuleShape3D;

typedef struct {
  BLB_Vec3 normal;
  float distance;
} BLB_PlaneShape3D;

typedef struct {
  BLB_Vec3 *vertices;
  size_t vertex_count;
} BLB_ConvexHullShape3D;

typedef struct {
  BLB_Vec3 *vertices;
  size_t vertex_count;

  uint32_t *indices;
  size_t index_count;

  BLB_Vec3 scale;
} BLB_TriangleMeshShape3D;

typedef struct {
  void *shapes;
  size_t shape_count;
} BLB_CompoundShape3D;

typedef struct {
  BLB_Physics3DShapeType type;

  union {
    BLB_BoxShape3D box;
    BLB_SphereShape3D sphere;
    BLB_CapsuleShape3D capsule;
    BLB_PlaneShape3D plane;
    BLB_ConvexHullShape3D convex_hull;
    BLB_TriangleMeshShape3D triangle_mesh;
    BLB_CompoundShape3D compound;
  } data;

  void *user_data;
} BLB_Physics3DShape;

typedef struct BLB_RigidBody3D BLB_RigidBody3D;
typedef struct BLB_Collider3D BLB_Collider3D;
typedef struct BLB_Physics3DComponent BLB_Physics3DComponent;

typedef struct {
  const BLB_Physics3DShape *shape;
  BLB_Vec3 position;
  BLB_Quat rotation;
  bool active;
  bool trigger;
} BLB_Physics3DDebugCollider;

typedef struct BLB_Physics3DWorld {
  BLB_Vec3 gravity;

  float fixed_timestep;
  float accumulator;
  float interpolation;

  int substeps;

  size_t body_count;
  size_t collider_count;
  bool debug_draw_enabled;

  BLB_Physics3DBodyId next_body_id;
  BLB_Physics3DColliderId next_collider_id;

  void *backend;
} BLB_Physics3DWorld;

// world
BLB_Physics3DWorld *BLB_Physics3D_WorldCreate(void);
void BLB_Physics3D_WorldDestroy(BLB_Physics3DWorld *world);

void BLB_Physics3D_WorldUpdate(BLB_Physics3DWorld *world);

void BLB_Physics3D_WorldSetGravity(BLB_Physics3DWorld *world, BLB_Vec3 gravity);

void BLB_Physics3D_WorldSetFixedTimestep(BLB_Physics3DWorld *world, float timestep);

void BLB_Physics3D_WorldSetLayerCollision(BLB_Physics3DWorld *world, uint8_t layer_a, uint8_t layer_b, bool enabled);

void BLB_Physics3D_WorldSetSubsteps(BLB_Physics3DWorld *world, int substeps);
void BLB_Physics3D_WorldSetDebugDraw(BLB_Physics3DWorld *world, bool enabled);
bool BLB_Physics3D_WorldGetDebugDraw(const BLB_Physics3DWorld *world);
size_t BLB_Physics3D_WorldGetDebugColliderCount(const BLB_Physics3DWorld *world);
bool BLB_Physics3D_WorldGetDebugCollider(const BLB_Physics3DWorld *world, size_t index, BLB_Physics3DDebugCollider *out_collider);

// body
void BLB_Physics3D_SetBody(BLB_Physics3DWorld *world, BLB_Object3D *object, BLB_Physics3DBodyType type);

void BLB_Physics3D_BodyDestroy(BLB_Physics3DWorld *world, BLB_Object3D *object);

BLB_RigidBody3D *BLB_Physics3D_GetBody(BLB_Object3D *object);

BLB_Physics3DBodyId BLB_Physics3D_BodyGetId(const BLB_Object3D *object);

void BLB_Physics3D_BodySetType(BLB_Object3D *object, BLB_Physics3DBodyType type);

void BLB_Physics3D_BodySetFixedRotation(BLB_Object3D *object, bool angularX, bool angularY, bool angularZ);

BLB_Physics3DBodyType BLB_Physics3D_BodyGetType(const BLB_Object3D *object);

void BLB_Physics3D_BodySetPosition(BLB_Object3D *object, BLB_Vec3 position);

BLB_Vec3 BLB_Physics3D_BodyGetPosition(const BLB_Object3D *object);

void BLB_Physics3D_BodySetRotation(BLB_Object3D *object, BLB_Quat rotation);

BLB_Quat BLB_Physics3D_BodyGetRotation(const BLB_Object3D *object);

void BLB_Physics3D_BodySetVelocity(BLB_Object3D *object, BLB_Vec3 velocity);

BLB_Vec3 BLB_Physics3D_BodyGetVelocity(const BLB_Object3D *object);

void BLB_Physics3D_BodySetAngularVelocity(BLB_Object3D *object, BLB_Vec3 angular_velocity);

BLB_Vec3 BLB_Physics3D_BodyGetAngularVelocity(const BLB_Object3D *object);

void BLB_Physics3D_BodyApplyForce(BLB_Object3D *object, BLB_Vec3 force);

void BLB_Physics3D_BodyApplyImpulse(BLB_Object3D *object, BLB_Vec3 impulse);

void BLB_Physics3D_BodySetGravityScale(BLB_Object3D *object, float scale);

void BLB_Physics3D_BodySetLinearDamping(BLB_Object3D *object, float damping);

void BLB_Physics3D_BodySetAngularDamping(BLB_Object3D *object, float damping);

void BLB_Physics3D_BodySetActive(BLB_Object3D *object, bool active);

bool BLB_Physics3D_BodyIsActive(const BLB_Object3D *object);

// shape
BLB_Physics3DShape *BLB_Physics3D_CreateShape(BLB_Physics3DShapeType type, BLB_Vec3 scale);

BLB_Physics3DShape *BLB_Physics3D_CreateShapeCompound(BLB_Physics3DShape *shapes, size_t shape_count);

BLB_Physics3DShape *BLB_Physics3D_CreateTriangleMeshShape(BLB_Vec3 scale, BLB_Vec3 *vertices, size_t vertex_count, uint32_t *indices,
                                                          size_t index_count);

BLB_Physics3DShape *BLB_Physics3D_CreateShapeFromObject3D(const BLB_Object3D *object);

void BLB_Physics3D_DestroyShape(BLB_Physics3DShape *shape);

// colider
BLB_Collider3D *BLB_Physics3D_SetCollider(BLB_Object3D *object, const BLB_Physics3DShape *shape);

void BLB_Physics3D_ColliderDestroy(BLB_Object3D *object);

BLB_Collider3D *BLB_Physics3D_GetCollider(BLB_Object3D *object);

BLB_Physics3DColliderId BLB_Physics3D_ColliderGetId(const BLB_Object3D *object);

void BLB_Physics3D_ColliderSetFilter(BLB_Object3D *object, BLB_PhysicsFilter filter);

BLB_PhysicsFilter BLB_Physics3D_ColliderGetFilter(const BLB_Object3D *object);

void BLB_Physics3D_ColliderSetLayer(BLB_Object3D *object, uint8_t layer);

void BLB_Physics3D_ColliderSetMask(BLB_Object3D *object, BLB_PhysicsLayerMask mask);

void BLB_Physics3D_ColliderSetGroup(BLB_Object3D *object, int32_t group);

void BLB_Physics3D_ColliderSetFriction(BLB_Object3D *object, float friction);

float BLB_Physics3D_ColliderGetFriction(const BLB_Object3D *object);

void BLB_Physics3D_ColliderSetRestitution(BLB_Object3D *object, float restitution);

float BLB_Physics3D_ColliderGetRestitution(const BLB_Object3D *object);

void BLB_Physics3D_ColliderSetTrigger(BLB_Object3D *object, bool trigger);

bool BLB_Physics3D_ColliderIsTrigger(const BLB_Object3D *object);

void BLB_Physics3D_ColliderSetActive(BLB_Object3D *object, bool active);

bool BLB_Physics3D_ColliderIsActive(const BLB_Object3D *object);

void BLB_Physics3D_ColliderSetDensity(BLB_Object3D *object, float density);

void BLB_Physics3D_ColliderSetScale(BLB_Object3D *object, BLB_Vec3 scale);

const BLB_Physics3DShape *BLB_Physics3D_ColliderGetShape(const BLB_Object3D *object);
float BLB_Physics3D_ColliderGetDensity(const BLB_Object3D *object);

#endif
