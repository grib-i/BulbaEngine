#ifndef BULBA_CORE_PHYSICS_PHYSICS2D_H
#define BULBA_CORE_PHYSICS_PHYSICS2D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/physics/physics.h"
#include "bulba/core/utils/object.h"

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
  BLB_Vec2 half_extents;
} BLB_BoxShape2D;

typedef struct {
  float radius;
} BLB_CircleShape2D;

typedef struct {
  float radius;
  float half_height;
} BLB_CapsuleShape2D;

typedef struct {
  BLB_Vec2 point1;
  BLB_Vec2 point2;
} BLB_SegmentShape2D;

typedef struct {
  BLB_Vec2 *vertices;
  size_t vertex_count;
} BLB_PolygonShape2D;

typedef struct {
  BLB_Vec2 *vertices;
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

typedef struct {
  const BLB_Physics2DShape *shape;
  BLB_Vec2 position;
  float rotation;
  bool active;
  bool trigger;
} BLB_Physics2DDebugCollider;

typedef struct BLB_Physics2DWorld {
  BLB_Vec2 gravity;

  float fixed_timestep;
  float accumulator;
  float interpolation;

  int substeps;

  size_t body_count;
  size_t collider_count;
  bool debug_draw_enabled;

  BLB_Physics2DBodyId next_body_id;
  BLB_Physics2DColliderId next_collider_id;

  void *backend;
} BLB_Physics2DWorld;

BLB_Physics2DWorld *BLB_Physics2D_WorldCreate(void);
void BLB_Physics2D_WorldDestroy(BLB_Physics2DWorld *world);

void BLB_Physics2D_WorldUpdate(BLB_Physics2DWorld *world);

void BLB_Physics2D_WorldSetGravity(BLB_Physics2DWorld *world, BLB_Vec2 gravity);

void BLB_Physics2D_WorldSetFixedTimestep(BLB_Physics2DWorld *world, float timestep);

void BLB_Physics2D_WorldSetLayerCollision(BLB_Physics2DWorld *world, uint8_t layer_a, uint8_t layer_b, bool enabled);

void BLB_Physics2D_WorldSetSubsteps(BLB_Physics2DWorld *world, int substeps);
void BLB_Physics2D_WorldSetDebugDraw(BLB_Physics2DWorld *world, bool enabled);
bool BLB_Physics2D_WorldGetDebugDraw(const BLB_Physics2DWorld *world);
size_t BLB_Physics2D_WorldGetDebugColliderCount(const BLB_Physics2DWorld *world);
bool BLB_Physics2D_WorldGetDebugCollider(const BLB_Physics2DWorld *world, size_t index, BLB_Physics2DDebugCollider *out_collider);

// body
void BLB_Physics2D_SetBody(BLB_Physics2DWorld *world, BLB_Object2D *object, BLB_Physics2DBodyType type);

BLB_RigidBody2D *BLB_Physics2D_GetBody(BLB_Object2D *object);

void BLB_Physics2D_BodyDestroy(BLB_Physics2DWorld *world, BLB_Object2D *object);

BLB_Physics2DBodyId BLB_Physics2D_BodyGetId(const BLB_Object2D *object);

void BLB_Physics2D_BodySetType(BLB_Object2D *object, BLB_Physics2DBodyType type);

void BLB_Physics2D_BodySetFixedRotation(BLB_Object2D *object, bool flag);

BLB_Physics2DBodyType BLB_Physics2D_BodyGetType(const BLB_Object2D *object);

void BLB_Physics2D_BodySetPosition(BLB_Object2D *object, BLB_Vec2 position);

BLB_Vec2 BLB_Physics2D_BodyGetPosition(const BLB_Object2D *object);

void BLB_Physics2D_BodySetRotation(BLB_Object2D *object, float rotation);

float BLB_Physics2D_BodyGetRotation(const BLB_Object2D *object);

void BLB_Physics2D_BodySetVelocity(BLB_Object2D *object, BLB_Vec2 velocity);

BLB_Vec2 BLB_Physics2D_BodyGetVelocity(const BLB_Object2D *object);

void BLB_Physics2D_BodySetAngularVelocity(BLB_Object2D *object, float velocity);
float BLB_Physics2D_BodyGetAngularVelocity(const BLB_Object2D *object);

void BLB_Physics2D_BodyApplyForce(BLB_Object2D *object, BLB_Vec2 force);

void BLB_Physics2D_BodyApplyImpulse(BLB_Object2D *object, BLB_Vec2 impulse);

void BLB_Physics2D_BodySetGravityScale(BLB_Object2D *object, float scale);

void BLB_Physics2D_BodySetLinearDamping(BLB_Object2D *object, float damping);

void BLB_Physics2D_BodySetAngularDamping(BLB_Object2D *object, float damping);

void BLB_Physics2D_BodySetActive(BLB_Object2D *object, bool active);

bool BLB_Physics2D_BodyIsActive(const BLB_Object2D *object);

BLB_Physics2DShape *BLB_Physics2D_CreateShape(BLB_Physics2DShapeType type, BLB_Vec2 scale);
BLB_Physics2DShape *BLB_Physics2D_CreateShapeFromObject2D(const BLB_Object2D *object);
void BLB_Physics2D_DestroyShape(BLB_Physics2DShape *shape);

BLB_Collider2D *BLB_Physics2D_SetCollider(BLB_Object2D *object, const BLB_Physics2DShape *shape);

void BLB_Physics2D_ColliderDestroy(BLB_Object2D *object);

BLB_Collider2D *BLB_Physics2D_GetCollider(BLB_Object2D *object);

BLB_Physics2DColliderId BLB_Physics2D_ColliderGetId(const BLB_Object2D *object);

void BLB_Physics2D_ColliderSetFilter(BLB_Object2D *object, BLB_PhysicsFilter filter);

BLB_PhysicsFilter BLB_Physics2D_ColliderGetFilter(const BLB_Object2D *object);

void BLB_Physics2D_ColliderSetLayer(BLB_Object2D *object, uint8_t layer);

void BLB_Physics2D_ColliderSetMask(BLB_Object2D *object, BLB_PhysicsLayerMask mask);

void BLB_Physics2D_ColliderSetGroup(BLB_Object2D *object, int32_t group);

void BLB_Physics2D_ColliderSetFriction(BLB_Object2D *object, float friction);

float BLB_Physics2D_ColliderGetFriction(const BLB_Object2D *object);

void BLB_Physics2D_ColliderSetRestitution(BLB_Object2D *object, float restitution);

float BLB_Physics2D_ColliderGetRestitution(const BLB_Object2D *object);

void BLB_Physics2D_ColliderSetTrigger(BLB_Object2D *object, bool trigger);

bool BLB_Physics2D_ColliderIsTrigger(const BLB_Object2D *object);

void BLB_Physics2D_ColliderSetActive(BLB_Object2D *object, bool active);

bool BLB_Physics2D_ColliderIsActive(const BLB_Object2D *object);

void BLB_Physics2D_ColliderSetDensity(BLB_Object2D *object, float density);

const BLB_Physics2DShape *BLB_Physics2D_ColliderGetShape(const BLB_Object2D *object);

float BLB_Physics2D_ColliderGetDensity(const BLB_Object2D *object);
void BLB_Physics2D_ColliderSetScale(BLB_Object2D *object, BLB_Vec2 scale);

#endif
