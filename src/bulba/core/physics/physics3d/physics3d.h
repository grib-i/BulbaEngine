#ifndef BULBA_CORE_MATH3V_PHYSICS3D_INTERNAL_H
#define BULBA_CORE_MATH3V_PHYSICS3D_INTERNAL_H

#include "bulba/core/math3v/polygon.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics/physics3d.h"
#include <box3d/box3d.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BLB_PHYSICS_DEFAULT_FIXED_TIMESTEP (1.0f / 60.0f)
#define BLB_PHYSICS_DEFAULT_SUBSTEPS 4

struct BLB_Collider3D;
struct BLB_RigidBody3D {
  BLB_Object3D *object;
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
  b3MeshData *mesh_data;
  BLB_RigidBody3D *body;
  BLB_Physics3DShape shape;
  BLB_Vec3 sync_scale;
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

bool blb_physics3d_valid_world(const BLB_Physics3DWorld *world);
BLB_Box3DBackend *blb_physics3d_get_backend(BLB_Physics3DWorld *world);
const BLB_Box3DBackend *blb_physics3d_get_backend_const(const BLB_Physics3DWorld *world);
BLB_PhysicsLayerMask blb_physics3d_layer_bit(uint8_t layer);
b3BodyType blb_physics3d_to_b3_body_type(BLB_Physics3DBodyType type);
BLB_Physics3DBodyType blb_physics3d_from_b3_body_type(b3BodyType type);
b3Filter blb_physics3d_make_filter(const BLB_Collider3D *collider);
bool blb_physics3d_append_body(BLB_Box3DBackend *backend, BLB_RigidBody3D *body,
                               size_t count);
bool blb_physics3d_append_collider(BLB_Box3DBackend *backend, BLB_Collider3D *collider,
                                   size_t count);
bool blb_physics3d_append_body_collider(BLB_RigidBody3D *body, BLB_Collider3D *collider);
void blb_physics3d_remove_body(BLB_Box3DBackend *backend, BLB_RigidBody3D *body,
                               size_t count);
void blb_physics3d_remove_collider(BLB_Box3DBackend *backend, BLB_Collider3D *collider,
                                   size_t count);
void blb_physics3d_remove_body_collider(BLB_RigidBody3D *body, BLB_Collider3D *collider);
void blb_physics3d_free_shape_copy(BLB_Physics3DShape *shape);
bool blb_physics3d_copy_shape(BLB_Physics3DShape *dst, const BLB_Physics3DShape *src);
b3ShapeId blb_physics3d_create_backend_shape(const BLB_Collider3D *collider,
                                             const b3ShapeDef *def);

bool blb_physics3d_recreate_shape(BLB_Collider3D *collider, bool trigger);
b3Quat blb_to_b3_quat(BLB_Quat rotation);
BLB_Quat blb_from_b3_quat(b3Quat rotation);

void blb_object3d_sync_from_body(BLB_Object3D *object);
void blb_object3d_sync_to_body(BLB_Object3D *object);

#endif
