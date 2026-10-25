#ifndef BULBA_CORE_MATH3V_PHYSICS2D_INTERNAL_H
#define BULBA_CORE_MATH3V_PHYSICS2D_INTERNAL_H

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/physics/physics2d.h"
#include <box2d/box2d.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLB_PHYSICS_DEFAULT_FIXED_TIMESTEP (1.0f / 60.0f)
#define BLB_PHYSICS_DEFAULT_SUBSTEPS 4

struct BLB_Collider2D;
struct BLB_RigidBody2D {
  BLB_Object2D *object;
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
  BLB_Vec2 sync_scale;
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

bool blb_physics2d_valid_world(const BLB_Physics2DWorld *world);
BLB_Box2DBackend *blb_physics2d_get_backend(BLB_Physics2DWorld *world);
const BLB_Box2DBackend *blb_physics2d_get_backend_const(const BLB_Physics2DWorld *world);
BLB_PhysicsLayerMask blb_physics2d_layer_bit(uint8_t layer);
b2BodyType blb_physics2d_to_b2_body_type(BLB_Physics2DBodyType type);
BLB_Physics2DBodyType blb_physics2d_from_b2_body_type(b2BodyType type);
b2Filter blb_physics2d_make_filter(const BLB_Collider2D *collider);
bool blb_physics2d_append_body(BLB_Box2DBackend *backend, BLB_RigidBody2D *body, size_t count);
bool blb_physics2d_append_collider(BLB_Box2DBackend *backend, BLB_Collider2D *collider, size_t count);
bool blb_physics2d_append_body_collider(BLB_RigidBody2D *body, BLB_Collider2D *collider);
void blb_physics2d_remove_body(BLB_Box2DBackend *backend, BLB_RigidBody2D *body, size_t count);
void blb_physics2d_remove_collider(BLB_Box2DBackend *backend, BLB_Collider2D *collider, size_t count);
void blb_physics2d_remove_body_collider(BLB_RigidBody2D *body, BLB_Collider2D *collider);
void blb_physics2d_free_shape_copy(BLB_Physics2DShape *shape);
bool blb_physics2d_copy_shape(BLB_Physics2DShape *dst, const BLB_Physics2DShape *src);
b2ShapeId blb_physics2d_create_backend_shape(const BLB_Collider2D *collider, const b2ShapeDef *def);
bool blb_physics2d_recreate_shape(BLB_Collider2D *collider, bool trigger);

BLB_RigidBody2D *blb_physics2d_object_body(const BLB_Object2D *object);
BLB_Collider2D *blb_physics2d_object_collider(const BLB_Object2D *object);
void blb_object2d_sync_from_body(BLB_Object2D *object);
void blb_object2d_sync_to_body(BLB_Object2D *object);

#endif
