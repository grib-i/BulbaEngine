#ifndef BULBA_CORE_PHYSICS_H
#define BULBA_CORE_PHYSICS_H

#include "bulba/core/math3v/math3v.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t BLB_PhysicsBodyId;
typedef uint64_t BLB_ColliderId;

typedef enum {
  BLB_PHYSICS_STATIC = 0,
  BLB_PHYSICS_DYNAMIC = 1,
  BLB_PHYSICS_KINEMATIC = 2
} BLB_PhysicsBodyType;

typedef enum {
  BLB_SHAPE_BOX = 0,
  BLB_SHAPE_SPHERE = 1,
  BLB_SHAPE_CAPSULE = 2,
  BLB_SHAPE_PLANE = 3,
  BLB_SHAPE_CONVEX_HULL = 4,
  BLB_SHAPE_TRIANGLE_MESH = 5,
  BLB_SHAPE_COMPOUND = 6
} BLB_PhysicsShapeType;

typedef struct {
  HMM_Vec3 half_extents;
} BLB_BoxShape;

typedef struct {
  float radius;
} BLB_SphereShape;

typedef struct {
  float radius;
  float half_height;
} BLB_CapsuleShape;

typedef struct {
  BLB_PhysicsShapeType type;
  union {
    BLB_BoxShape box;
    BLB_SphereShape sphere;
    BLB_CapsuleShape capsule;
    HMM_Vec3 plane_normal;
  } data;
  void *user_data;
} BLB_PhysicsShape;

typedef struct BLB_RigidBody {
  BLB_PhysicsBodyId id;
  BLB_PhysicsBodyType type;
  float mass;
  float inverse_mass;
  HMM_Vec3 position;
  HMM_Quat rotation;
  HMM_Vec3 linear_velocity;
  HMM_Vec3 angular_velocity;
  HMM_Vec3 force;
  HMM_Vec3 torque;
  HMM_Vec3 inertia;
  HMM_Vec3 inverse_inertia;
  float linear_damping;
  float angular_damping;
  float gravity_scale;
  bool active;
  bool sleeping;
  void *user_data;
} BLB_RigidBody;

typedef struct BLB_Collider {
  BLB_ColliderId id;
  BLB_PhysicsBodyId body_id;
  BLB_PhysicsShape shape;
  HMM_Vec3 local_position;
  HMM_Quat local_rotation;
  float friction;
  float restitution;
  bool trigger;
  bool active;
  void *user_data;
} BLB_Collider;

typedef struct BLB_PhysicsWorld {
  HMM_Vec3 gravity;
  float fixed_timestep;
  float accumulator;
  float interpolation;
  BLB_RigidBody **bodies;
  size_t body_count;
  BLB_Collider **colliders;
  size_t collider_count;
  BLB_PhysicsBodyId next_body_id;
  BLB_ColliderId next_collider_id;
} BLB_PhysicsWorld;

BLB_PhysicsWorld *BLB_PhysicsWorld_Create(void);
void BLB_PhysicsWorld_Destroy(BLB_PhysicsWorld *world);
BLB_RigidBody *BLB_PhysicsWorld_CreateBody(BLB_PhysicsWorld *world, BLB_PhysicsBodyType type);
BLB_Collider *BLB_PhysicsWorld_CreateCollider(BLB_PhysicsWorld *world, BLB_PhysicsBodyId body_id, const BLB_PhysicsShape *shape);
int BLB_PhysicsWorld_RemoveBody(BLB_PhysicsWorld *world, BLB_PhysicsBodyId id);
int BLB_PhysicsWorld_RemoveCollider(BLB_PhysicsWorld *world, BLB_ColliderId id);
void BLB_PhysicsWorld_Step(BLB_PhysicsWorld *world, float delta_time);
void BLB_PhysicsWorld_SetGravity(BLB_PhysicsWorld *world, HMM_Vec3 gravity);
void BLB_PhysicsWorld_SetFixedTimestep(BLB_PhysicsWorld *world, float timestep);

#endif
