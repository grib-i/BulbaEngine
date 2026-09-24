#ifndef BULBA_CORE_PHYSICS3D_H
#define BULBA_CORE_PHYSICS3D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/physics.h"

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
  HMM_Vec3 half_extents;
} BLB_BoxShape3D;

typedef struct {
  float radius;
} BLB_SphereShape3D;

typedef struct {
  float radius;
  float half_height;
} BLB_CapsuleShape3D;

typedef struct {
  HMM_Vec3 normal;
  float distance;
} BLB_PlaneShape3D;

typedef struct {
  HMM_Vec3 *vertices;
  size_t vertex_count;
} BLB_ConvexHullShape3D;

typedef struct {
  HMM_Vec3 *vertices;
  size_t vertex_count;

  uint32_t *indices;
  size_t index_count;
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

typedef struct BLB_Physics3DWorld {
  HMM_Vec3 gravity;

  float fixed_timestep;
  float accumulator;
  float interpolation;

  int substeps;

  size_t body_count;
  size_t collider_count;

  BLB_Physics3DBodyId next_body_id;
  BLB_Physics3DColliderId next_collider_id;

  void *backend;
} BLB_Physics3DWorld;

BLB_Physics3DWorld *BLB_Physics3DWorld_Create(void);
void BLB_Physics3DWorld_Destroy(BLB_Physics3DWorld *world);

void BLB_Physics3DWorld_Step(BLB_Physics3DWorld *world, float delta_time);

void BLB_Physics3DWorld_SetGravity(BLB_Physics3DWorld *world, HMM_Vec3 gravity);

void BLB_Physics3DWorld_SetFixedTimestep(BLB_Physics3DWorld *world, float timestep);

void BLB_Physics3DWorld_SetLayerCollision(BLB_Physics3DWorld *world, uint8_t layer_a, uint8_t layer_b, bool enabled);

BLB_RigidBody3D *BLB_Physics3D_BodyCreate(BLB_Physics3DWorld *world, BLB_Physics3DBodyType type);

void BLB_Physics3D_BodyDestroy(BLB_Physics3DWorld *world, BLB_RigidBody3D *body);

BLB_Physics3DBodyId BLB_Physics3D_BodyGetId(const BLB_RigidBody3D *body);

void BLB_Physics3D_BodySetType(BLB_RigidBody3D *body, BLB_Physics3DBodyType type);

BLB_Physics3DBodyType BLB_Physics3D_BodyGetType(const BLB_RigidBody3D *body);

void BLB_Physics3D_BodySetPosition(BLB_RigidBody3D *body, HMM_Vec3 position);

HMM_Vec3 BLB_Physics3D_BodyGetPosition(const BLB_RigidBody3D *body);

void BLB_Physics3D_BodySetRotation(BLB_RigidBody3D *body, HMM_Quat rotation);

HMM_Quat BLB_Physics3D_BodyGetRotation(const BLB_RigidBody3D *body);

void BLB_Physics3D_BodySetVelocity(BLB_RigidBody3D *body, HMM_Vec3 velocity);

HMM_Vec3 BLB_Physics3D_BodyGetVelocity(const BLB_RigidBody3D *body);

void BLB_Physics3D_BodySetAngularVelocity(BLB_RigidBody3D *body, HMM_Vec3 angular_velocity);

HMM_Vec3 BLB_Physics3D_BodyGetAngularVelocity(const BLB_RigidBody3D *body);

void BLB_Physics3D_BodyApplyForce(BLB_RigidBody3D *body, HMM_Vec3 force);

void BLB_Physics3D_BodyApplyImpulse(BLB_RigidBody3D *body, HMM_Vec3 impulse);

void BLB_Physics3D_BodySetGravityScale(BLB_RigidBody3D *body, float scale);

void BLB_Physics3D_BodySetLinearDamping(BLB_RigidBody3D *body, float damping);

void BLB_Physics3D_BodySetAngularDamping(BLB_RigidBody3D *body, float damping);

void BLB_Physics3D_BodySetActive(BLB_RigidBody3D *body, bool active);

bool BLB_Physics3D_BodyIsActive(const BLB_RigidBody3D *body);

BLB_Collider3D *BLB_Physics3D_ColliderCreate(BLB_RigidBody3D *body, const BLB_Physics3DShape *shape);

void BLB_Physics3D_ColliderDestroy(BLB_Collider3D *collider);

BLB_Physics3DColliderId BLB_Physics3D_ColliderGetId(const BLB_Collider3D *collider);

void BLB_Physics3D_ColliderSetFilter(BLB_Collider3D *collider, BLB_PhysicsFilter filter);

BLB_PhysicsFilter BLB_Physics3D_ColliderGetFilter(const BLB_Collider3D *collider);

void BLB_Physics3D_ColliderSetLayer(BLB_Collider3D *collider, uint8_t layer);

void BLB_Physics3D_ColliderSetMask(BLB_Collider3D *collider, BLB_PhysicsLayerMask mask);

void BLB_Physics3D_ColliderSetGroup(BLB_Collider3D *collider, int32_t group);

void BLB_Physics3D_ColliderSetFriction(BLB_Collider3D *collider, float friction);

float BLB_Physics3D_ColliderGetFriction(const BLB_Collider3D *collider);

void BLB_Physics3D_ColliderSetRestitution(BLB_Collider3D *collider, float restitution);

float BLB_Physics3D_ColliderGetRestitution(const BLB_Collider3D *collider);

void BLB_Physics3D_ColliderSetTrigger(BLB_Collider3D *collider, bool trigger);

bool BLB_Physics3D_ColliderIsTrigger(const BLB_Collider3D *collider);

void BLB_Physics3D_ColliderSetActive(BLB_Collider3D *collider, bool active);

bool BLB_Physics3D_ColliderIsActive(const BLB_Collider3D *collider);

void BLB_Physics3D_ColliderSetDensity(BLB_Collider3D *collider, float density);

void BLB_Physics3DWorld_SetSubsteps(BLB_Physics3DWorld *world, int substeps);

#endif
