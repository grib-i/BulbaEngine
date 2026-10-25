#include "box3d/box3d.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics/physics3d.h"
#include "bulba/core/utils/object.h"
#include "physics3d.h"
#include <stdlib.h>

void BLB_Physics3D_SetBody(BLB_Physics3DWorld *world, BLB_Object3D *object,
                           BLB_Physics3DBodyType type) {
  if (!object || object->rigid_body || !world || !blb_physics3d_valid_world(world))
    return;

  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);
  b3BodyDef def = b3DefaultBodyDef();
  def.type = blb_physics3d_to_b3_body_type(type);

  b3BodyId handle = b3CreateBody(backend->world_id, &def);

  if (!b3Body_IsValid(handle))
    return;

  object->rigid_body = calloc(1, sizeof(*object->rigid_body));

  if (!object->rigid_body) {
    b3DestroyBody(handle);

    return;
  }

  object->rigid_body->object = object;
  object->rigid_body->id = world->next_body_id++;
  object->rigid_body->handle = handle;
  object->rigid_body->world = world;
  object->rigid_body->type = type;
  object->rigid_body->user_data = NULL;

  b3Body_SetUserData(handle, object->rigid_body);

  if (!blb_physics3d_append_body(backend, object->rigid_body, world->body_count)) {
    b3DestroyBody(handle);
    free(object->rigid_body);
    object->rigid_body = NULL;

    return;
  }

  b3Body_SetTransform(handle,
                      (b3Pos){object->position.x, object->position.y, object->position.z},
                      blb_to_b3_quat(BLB_QuatFromEuler(
                          BLB_MulV3F(object->rotation, (float)BLB_PI / 180.0f))));
  ++world->body_count;
}

void BLB_Physics3D_BodyDestroy(BLB_Physics3DWorld *world, BLB_Object3D *object) {
  if (!object || !world || !object->rigid_body || object->rigid_body->world != world)
    return;

  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);

  if (b3Body_IsValid(object->rigid_body->handle))
    b3DestroyBody(object->rigid_body->handle);

  while (object->rigid_body->collider_count > 0) {
    BLB_Collider3D *collider =
        object->rigid_body->colliders[object->rigid_body->collider_count - 1];
    blb_physics3d_remove_collider(backend, collider, world->collider_count);

    if (world->collider_count > 0)
      --world->collider_count;
    blb_physics3d_free_shape_copy(&collider->shape);

    if (collider->mesh_data)
      b3DestroyMesh(collider->mesh_data);
    free(collider);
    --object->rigid_body->collider_count;
  }

  free(object->rigid_body->colliders);
  object->rigid_body->colliders = NULL;

  blb_physics3d_remove_body(backend, object->rigid_body, world->body_count);

  if (world->body_count > 0)
    --world->body_count;

  free(object->rigid_body);
  object->rigid_body = NULL;
  object->collider = NULL;
}

BLB_Physics3DBodyId BLB_Physics3D_BodyGetId(const BLB_Object3D *object) {
  return object && object->rigid_body ? object->rigid_body->id : 0;
}

BLB_RigidBody3D *BLB_Physics3D_GetBody(BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return NULL;

  return object->rigid_body;
}

void BLB_Physics3D_BodySetType(BLB_Object3D *object, BLB_Physics3DBodyType type) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  b3Body_SetType(object->rigid_body->handle, blb_physics3d_to_b3_body_type(type));
  object->rigid_body->type = type;
}

void BLB_Physics3D_BodySetFixedRotation(BLB_Object3D *object, bool angularX,
                                        bool angularY, bool angularZ) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  b3MotionLocks currentLocks = b3Body_GetMotionLocks(object->rigid_body->handle);
  currentLocks.angularX = angularX;
  currentLocks.angularY = angularY;
  currentLocks.angularZ = angularZ;

  b3Body_SetMotionLocks(object->rigid_body->handle, currentLocks);
}

BLB_Physics3DBodyType BLB_Physics3D_BodyGetType(const BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return BLB_PHYSICS3D_STATIC;

  return blb_physics3d_from_b3_body_type(b3Body_GetType(object->rigid_body->handle));
}

void BLB_Physics3D_BodySetPosition(BLB_Object3D *object, BLB_Vec3 position) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle) ||
      !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z))
    return;

  b3Body_SetTransform(object->rigid_body->handle,
                      (b3Pos){position.x, position.y, position.z},
                      b3Body_GetRotation(object->rigid_body->handle));
  blb_object3d_sync_from_body(object);
}

BLB_Vec3 BLB_Physics3D_BodyGetPosition(const BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return BLB_V3(0.0f, 0.0f, 0.0f);

  b3Pos position = b3Body_GetPosition(object->rigid_body->handle);

  return BLB_V3((float)position.x, (float)position.y, (float)position.z);
}

void BLB_Physics3D_BodySetRotation(BLB_Object3D *object, BLB_Quat rotation) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle) ||
      !isfinite(rotation.x) || !isfinite(rotation.y) || !isfinite(rotation.z) ||
      !isfinite(rotation.w))
    return;

  if (BLB_DotQ(rotation, rotation) < 0.000001f)
    return;

  b3Body_SetTransform(object->rigid_body->handle,
                      b3Body_GetPosition(object->rigid_body->handle),
                      blb_to_b3_quat(BLB_NormQ(rotation)));
  blb_object3d_sync_from_body(object);
}

BLB_Quat BLB_Physics3D_BodyGetRotation(const BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle)) {
    BLB_Quat identity = {0};
    identity.w = 1.0f;

    return identity;
  }

  return blb_from_b3_quat(b3Body_GetRotation(object->rigid_body->handle));
}

void BLB_Physics3D_BodySetVelocity(BLB_Object3D *object, BLB_Vec3 velocity) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  b3Body_SetLinearVelocity(object->rigid_body->handle,
                           (b3Vec3){velocity.x, velocity.y, velocity.z});
}

BLB_Vec3 BLB_Physics3D_BodyGetVelocity(const BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return BLB_V3(0.0f, 0.0f, 0.0f);

  b3Vec3 velocity = b3Body_GetLinearVelocity(object->rigid_body->handle);

  return BLB_V3(velocity.x, velocity.y, velocity.z);
}

void BLB_Physics3D_BodySetAngularVelocity(BLB_Object3D *object, BLB_Vec3 velocity) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  b3Body_SetAngularVelocity(object->rigid_body->handle,
                            (b3Vec3){velocity.x, velocity.y, velocity.z});
}

BLB_Vec3 BLB_Physics3D_BodyGetAngularVelocity(const BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return BLB_V3(0.0f, 0.0f, 0.0f);

  b3Vec3 velocity = b3Body_GetAngularVelocity(object->rigid_body->handle);

  return BLB_V3(velocity.x, velocity.y, velocity.z);
}

void BLB_Physics3D_BodyApplyForce(BLB_Object3D *object, BLB_Vec3 force) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  b3Body_ApplyForceToCenter(object->rigid_body->handle,
                            (b3Vec3){force.x, force.y, force.z}, true);
}

void BLB_Physics3D_BodyApplyImpulse(BLB_Object3D *object, BLB_Vec3 impulse) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  b3Body_ApplyLinearImpulseToCenter(object->rigid_body->handle,
                                    (b3Vec3){impulse.x, impulse.y, impulse.z}, true);
}

void BLB_Physics3D_BodySetGravityScale(BLB_Object3D *object, float scale) {
  if (object->rigid_body && b3Body_IsValid(object->rigid_body->handle))
    b3Body_SetGravityScale(object->rigid_body->handle, scale);
}

void BLB_Physics3D_BodySetLinearDamping(BLB_Object3D *object, float damping) {
  if (object->rigid_body && b3Body_IsValid(object->rigid_body->handle))
    b3Body_SetLinearDamping(object->rigid_body->handle, damping);
}

void BLB_Physics3D_BodySetAngularDamping(BLB_Object3D *object, float damping) {
  if (object->rigid_body && b3Body_IsValid(object->rigid_body->handle))
    b3Body_SetAngularDamping(object->rigid_body->handle, damping);
}

void BLB_Physics3D_BodySetActive(BLB_Object3D *object, bool active) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return;

  if (active)
    b3Body_Enable(object->rigid_body->handle);
  else
    b3Body_Disable(object->rigid_body->handle);
}

bool BLB_Physics3D_BodyIsActive(const BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return -1;

  return b3Body_IsEnabled(object->rigid_body->handle);
}
