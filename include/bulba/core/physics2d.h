#ifndef BULBA_CORE_PHYSICS2D_H
#define BULBA_CORE_PHYSICS2D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/physics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t BLB_Physics2DBodyId;
typedef uint64_t BLB_Physics2DColliderId;

typedef enum { BLB_PHYSICS2D_STATIC = 0, BLB_PHYSICS2D_DYNAMIC = 1, BLB_PHYSICS2D_KINEMATIC = 2 } BLB_Physics2DBodyType;

typedef enum {
  BLB_PHYSICS2D_SHAPE_BOX = 0,
  BLB_PHYSICS2D_SHAPE_CIRCLE = 1,
  BLB_PHYSICS2D_SHAPE_CAPSULE = 2,
  BLB_PHYSICS2D_SHAPE_SEGMENT = 3,
  BLB_PHYSICS2D_SHAPE_POLYGON = 4,
  BLB_PHYSICS2D_SHAPE_CHAIN = 5
} BLB_Physics2DShapeType;

typedef struct {
  HMM_Vec2 half_extents;
} BLB_BoxShape2D;

typedef struct {
  float radius;
} BLB_CircleShape2D;

typedef struct {
  float radius;
  float half_height;
} BLB_CapsuleShape2D;

typedef struct {
  HMM_Vec2 point1;
  HMM_Vec2 point2;
} BLB_SegmentShape2D;

typedef struct {
  HMM_Vec2 *vertices;
  size_t vertex_count;
} BLB_PolygonShape2D;

typedef struct {
  HMM_Vec2 *vertices;
  size_t vertex_count;
  bool loop;
} BLB_ChainShape2D;

typedef struct {
  BLB_Physics2DShapeType type;

  union {
    BLB_BoxShape2D box;
    BLB_CircleShape2D circle;
    BLB_CapsuleShape2D capsule;
    BLB_SegmentShape2D segment;
    BLB_PolygonShape2D polygon;
    BLB_ChainShape2D chain;
  } data;

  void *user_data;
} BLB_Physics2DShape;

typedef struct BLB_RigidBody2D BLB_RigidBody2D;
typedef struct BLB_Collider2D BLB_Collider2D;
typedef struct BLB_Physics2DComponent BLB_Physics2DComponent;

typedef struct BLB_Physics2DWorld {
  HMM_Vec2 gravity;

  float fixed_timestep;
  float accumulator;
  float interpolation;

  int substeps;

  size_t body_count;
  size_t collider_count;

  BLB_Physics2DBodyId next_body_id;
  BLB_Physics2DColliderId next_collider_id;

  void *backend;
} BLB_Physics2DWorld;

BLB_Physics2DWorld *BLB_Physics2DWorld_Create(void);
void BLB_Physics2DWorld_Destroy(BLB_Physics2DWorld *world);

void BLB_Physics2DWorld_Step(BLB_Physics2DWorld *world, float delta_time);

void BLB_Physics2DWorld_SetGravity(BLB_Physics2DWorld *world, HMM_Vec2 gravity);

void BLB_Physics2DWorld_SetFixedTimestep(BLB_Physics2DWorld *world, float timestep);

void BLB_Physics2DWorld_SetLayerCollision(BLB_Physics2DWorld *world, uint8_t layer_a, uint8_t layer_b, bool enabled);

BLB_RigidBody2D *BLB_Physics2D_BodyCreate(BLB_Physics2DWorld *world, BLB_Physics2DBodyType type);

void BLB_Physics2D_BodyDestroy(BLB_Physics2DWorld *world, BLB_RigidBody2D *body);

BLB_Physics2DBodyId BLB_Physics2D_BodyGetId(const BLB_RigidBody2D *body);

void BLB_Physics2D_BodySetType(BLB_RigidBody2D *body, BLB_Physics2DBodyType type);

BLB_Physics2DBodyType BLB_Physics2D_BodyGetType(const BLB_RigidBody2D *body);

void BLB_Physics2D_BodySetPosition(BLB_RigidBody2D *body, HMM_Vec2 position);

HMM_Vec2 BLB_Physics2D_BodyGetPosition(const BLB_RigidBody2D *body);

void BLB_Physics2D_BodySetRotation(BLB_RigidBody2D *body, float rotation);

float BLB_Physics2D_BodyGetRotation(const BLB_RigidBody2D *body);

void BLB_Physics2D_BodySetVelocity(BLB_RigidBody2D *body, HMM_Vec2 velocity);

HMM_Vec2 BLB_Physics2D_BodyGetVelocity(const BLB_RigidBody2D *body);

void BLB_Physics2D_BodyApplyForce(BLB_RigidBody2D *body, HMM_Vec2 force);

void BLB_Physics2D_BodyApplyImpulse(BLB_RigidBody2D *body, HMM_Vec2 impulse);

void BLB_Physics2D_BodySetGravityScale(BLB_RigidBody2D *body, float scale);

void BLB_Physics2D_BodySetLinearDamping(BLB_RigidBody2D *body, float damping);

void BLB_Physics2D_BodySetAngularDamping(BLB_RigidBody2D *body, float damping);

void BLB_Physics2D_BodySetActive(BLB_RigidBody2D *body, bool active);

bool BLB_Physics2D_BodyIsActive(const BLB_RigidBody2D *body);

BLB_Collider2D *BLB_Physics2D_ColliderCreate(BLB_RigidBody2D *body, const BLB_Physics2DShape *shape);

void BLB_Physics2D_ColliderDestroy(BLB_Collider2D *collider);

BLB_Physics2DColliderId BLB_Physics2D_ColliderGetId(const BLB_Collider2D *collider);

void BLB_Physics2D_ColliderSetFilter(BLB_Collider2D *collider, BLB_PhysicsFilter filter);

BLB_PhysicsFilter BLB_Physics2D_ColliderGetFilter(const BLB_Collider2D *collider);

void BLB_Physics2D_ColliderSetLayer(BLB_Collider2D *collider, uint8_t layer);

void BLB_Physics2D_ColliderSetMask(BLB_Collider2D *collider, BLB_PhysicsLayerMask mask);

void BLB_Physics2D_ColliderSetGroup(BLB_Collider2D *collider, int32_t group);

void BLB_Physics2D_ColliderSetFriction(BLB_Collider2D *collider, float friction);

float BLB_Physics2D_ColliderGetFriction(const BLB_Collider2D *collider);

void BLB_Physics2D_ColliderSetRestitution(BLB_Collider2D *collider, float restitution);

float BLB_Physics2D_ColliderGetRestitution(const BLB_Collider2D *collider);

void BLB_Physics2D_ColliderSetTrigger(BLB_Collider2D *collider, bool trigger);

bool BLB_Physics2D_ColliderIsTrigger(const BLB_Collider2D *collider);

void BLB_Physics2D_ColliderSetActive(BLB_Collider2D *collider, bool active);

bool BLB_Physics2D_ColliderIsActive(const BLB_Collider2D *collider);

void BLB_Physics2D_ColliderSetDensity(BLB_Collider2D *collider, float density);

void BLB_Physics2DWorld_SetSubsteps(BLB_Physics2DWorld *world, int substeps);

#endif
