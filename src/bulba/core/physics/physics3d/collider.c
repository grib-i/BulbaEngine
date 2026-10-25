#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics/physics3d.h"
#include "physics3d.h"

BLB_Collider3D *BLB_Physics3D_SetCollider(BLB_Object3D *object, const BLB_Physics3DShape *shape) {
  if (!object || !object->rigid_body || !shape || !b3Body_IsValid(object->rigid_body->handle))
    return NULL;

  if (object->collider) {
    BLB_Object3D temporary = *object;
    temporary.collider = NULL;

    BLB_Collider3D *replacement = BLB_Physics3D_SetCollider(&temporary, shape);

    if (!replacement)
      return NULL;

    BLB_Physics3D_ColliderDestroy(object);
    object->collider = replacement;

    return replacement;
  }

  object->collider = calloc(1, sizeof(*object->collider));

  if (!object || !object->collider)
    return NULL;

  if (!blb_physics3d_copy_shape(&object->collider->shape, shape)) {
    blb_physics3d_free_shape_copy(&object->collider->shape);

    if (object->collider->mesh_data)
      b3DestroyMesh(object->collider->mesh_data);

    free(object->collider);
    object->collider = NULL;

    return NULL;
  }

  object->collider->body = object->rigid_body;
  object->collider->id = object->rigid_body->world->next_collider_id++;
  object->collider->filter.layer = 0;
  object->collider->filter.mask = UINT64_MAX;
  object->collider->filter.group = 0;
  object->collider->sync_scale = object->scale;
  object->collider->trigger = false;
  object->collider->active = true;
  object->collider->user_data = shape->user_data;

  b3ShapeDef def = b3DefaultShapeDef();
  def.filter = blb_physics3d_make_filter(object->collider);
  def.userData = object->collider->user_data;

  b3ShapeId handle = blb_physics3d_create_backend_shape(object->collider, &def);

  if (!b3Shape_IsValid(handle)) {
    blb_physics3d_free_shape_copy(&object->collider->shape);

    if (object->collider->mesh_data)
      b3DestroyMesh(object->collider->mesh_data);

    free(object->collider);
    object->collider = NULL;

    return NULL;
  }

  object->collider->handle = handle;
  object->collider->density = b3Shape_GetDensity(handle);
  object->collider->friction = b3Shape_GetFriction(handle);
  object->collider->restitution = b3Shape_GetRestitution(handle);

  b3Shape_SetUserData(handle, object->collider->user_data);

  BLB_Physics3DWorld *world = object->rigid_body->world;
  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);

  if (!blb_physics3d_append_body_collider(object->rigid_body, object->collider)) {
    b3DestroyShape(handle, true);
    blb_physics3d_free_shape_copy(&object->collider->shape);

    if (object->collider->mesh_data)
      b3DestroyMesh(object->collider->mesh_data);

    free(object->collider);
    object->collider = NULL;

    return NULL;
  }

  if (!blb_physics3d_append_collider(backend, object->collider, world->collider_count)) {
    blb_physics3d_remove_body_collider(object->rigid_body, object->collider);
    b3DestroyShape(handle, true);
    blb_physics3d_free_shape_copy(&object->collider->shape);

    if (object->collider->mesh_data)
      b3DestroyMesh(object->collider->mesh_data);

    free(object->collider);
    object->collider = NULL;

    return NULL;
  }

  ++world->collider_count;

  return object->collider;
}

void BLB_Physics3D_ColliderDestroy(BLB_Object3D *object) {
  if (!object || !object->collider || !object->collider->body || !object->collider->body->world)
    return;

  BLB_Physics3DWorld *world = object->collider->body->world;
  BLB_Box3DBackend *backend = blb_physics3d_get_backend(world);

  if (b3Shape_IsValid(object->collider->handle))
    b3DestroyShape(object->collider->handle, true);

  blb_physics3d_remove_body_collider(object->collider->body, object->collider);
  blb_physics3d_remove_collider(backend, object->collider, world->collider_count);

  if (world->collider_count > 0)
    --world->collider_count;

  blb_physics3d_free_shape_copy(&object->collider->shape);

  if (object->collider->mesh_data)
    b3DestroyMesh(object->collider->mesh_data);

  free(object->collider);
  object->collider = NULL;
}

BLB_Collider3D *BLB_Physics3D_GetCollider(BLB_Object3D *object) {
  if (!object || !object->rigid_body || !b3Body_IsValid(object->rigid_body->handle))
    return NULL;

  return object->collider;
}

BLB_Physics3DColliderId BLB_Physics3D_ColliderGetId(const BLB_Object3D *object) { return object && object->collider ? object->collider->id : 0; }

void BLB_Physics3D_ColliderSetFilter(BLB_Object3D *object, BLB_PhysicsFilter filter) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return;

  object->collider->filter = filter;
  b3Shape_SetFilter(object->collider->handle, blb_physics3d_make_filter(object->collider), true);
}

BLB_PhysicsFilter BLB_Physics3D_ColliderGetFilter(const BLB_Object3D *object) {

  BLB_PhysicsFilter filter = {0, 0, 0};

  if (object && object->collider)
    filter = object->collider->filter;

  return filter;
}

void BLB_Physics3D_ColliderSetLayer(BLB_Object3D *object, uint8_t layer) {
  if (!object || !object->collider || layer >= 64)
    return;

  object->collider->filter.layer = layer;

  if (b3Shape_IsValid(object->collider->handle))
    b3Shape_SetFilter(object->collider->handle, blb_physics3d_make_filter(object->collider), true);
}

void BLB_Physics3D_ColliderSetMask(BLB_Object3D *object, BLB_PhysicsLayerMask mask) {
  if (!object || !object->collider)
    return;

  object->collider->filter.mask = mask;

  if (b3Shape_IsValid(object->collider->handle))
    b3Shape_SetFilter(object->collider->handle, blb_physics3d_make_filter(object->collider), true);
}

void BLB_Physics3D_ColliderSetGroup(BLB_Object3D *object, int32_t group) {
  if (!object || !object->collider)
    return;

  object->collider->filter.group = group;

  if (b3Shape_IsValid(object->collider->handle))
    b3Shape_SetFilter(object->collider->handle, blb_physics3d_make_filter(object->collider), true);
}

void BLB_Physics3D_ColliderSetDensity(BLB_Object3D *object, float density) {
  if (!object || !object->collider || density < 0.0f || !b3Shape_IsValid(object->collider->handle))
    return;

  object->collider->density = density;
  b3Shape_SetDensity(object->collider->handle, density, true);
}

void BLB_Physics3D_ColliderSetScale(BLB_Object3D *object, BLB_Vec3 scale) {
  BLB_Collider3D *collider = object ? object->collider : NULL;

  if (!collider || !isfinite(scale.x) || scale.x <= 0.0f || collider->sync_scale.x <= 0.0f || !isfinite(scale.y) || scale.y <= 0.0f ||
      collider->sync_scale.y <= 0.0f || !isfinite(scale.z) || scale.z <= 0.0f || collider->sync_scale.z <= 0.0f)
    return;

  if (!memcmp(&scale, &collider->sync_scale, sizeof(scale))) {
    blb_object3d_sync_from_body(object);

    return;
  }

  BLB_Physics3DShape next = {0};

  if (!blb_physics3d_copy_shape(&next, &collider->shape)) {
    blb_physics3d_free_shape_copy(&next);

    return;
  }

  BLB_Vec3 ratio = BLB_DivV3(scale, collider->sync_scale);

  switch (next.type) {
  case BLB_PHYSICS3D_SHAPE_BOX:
    next.data.box.scale = BLB_MulV3(next.data.box.scale, ratio);
    break;
  case BLB_PHYSICS3D_SHAPE_SPHERE:
    next.data.sphere.radius *=
        fminf(scale.x, fminf(scale.y, scale.z)) / fminf(collider->sync_scale.x, fminf(collider->sync_scale.y, collider->sync_scale.z));
    break;
  case BLB_PHYSICS3D_SHAPE_CAPSULE:
    next.data.capsule.radius *= fminf(ratio.x, ratio.z);
    next.data.capsule.height *= ratio.y;
    break;
  case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH:
    next.data.triangle_mesh.scale = BLB_MulV3(next.data.triangle_mesh.scale, ratio);
    break;
  case BLB_PHYSICS3D_SHAPE_CONVEX_HULL:
    for (size_t i = 0; i < next.data.convex_hull.vertex_count; ++i)
      next.data.convex_hull.vertices[i] = BLB_MulV3(next.data.convex_hull.vertices[i], ratio);
    break;
  default:
    blb_physics3d_free_shape_copy(&next);

    return;
  }

  BLB_Physics3DShape previous = collider->shape;
  collider->shape = next;

  if (!blb_physics3d_recreate_shape(collider, collider->trigger)) {
    collider->shape = previous;
    blb_physics3d_free_shape_copy(&next);

    return;
  }

  blb_physics3d_free_shape_copy(&previous);
  collider->sync_scale = scale;
  blb_object3d_sync_from_body(object);
}

void BLB_Physics3D_ColliderScale(BLB_Object3D *object, BLB_Vec3 scale) {
  if (object && object->collider)
    BLB_Physics3D_ColliderSetScale(object, BLB_MulV3(object->collider->sync_scale, scale));
}

float BLB_Physics3D_ColliderGetDensity(const BLB_Object3D *object) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return 0.0f;

  return b3Shape_GetDensity(object->collider->handle);
}

const BLB_Physics3DShape *BLB_Physics3D_ColliderGetShape(const BLB_Object3D *object) {
  return object && object->collider ? &object->collider->shape : NULL;
}

void BLB_Physics3D_ColliderSetFriction(BLB_Object3D *object, float friction) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return;

  object->collider->friction = friction;
  b3Shape_SetFriction(object->collider->handle, friction);
}

float BLB_Physics3D_ColliderGetFriction(const BLB_Object3D *object) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return 0.0f;

  return b3Shape_GetFriction(object->collider->handle);
}

void BLB_Physics3D_ColliderSetRestitution(BLB_Object3D *object, float restitution) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return;

  object->collider->restitution = restitution;
  b3Shape_SetRestitution(object->collider->handle, restitution);
}

float BLB_Physics3D_ColliderGetRestitution(const BLB_Object3D *object) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return 0.0f;

  return b3Shape_GetRestitution(object->collider->handle);
}

void BLB_Physics3D_ColliderSetTrigger(BLB_Object3D *object, bool trigger) {
  if (!object || !object->collider || object->collider->trigger == trigger)
    return;

  blb_physics3d_recreate_shape(object->collider, trigger);
}

bool BLB_Physics3D_ColliderIsTrigger(const BLB_Object3D *object) { return object && object->collider ? object->collider->trigger : false; }

void BLB_Physics3D_ColliderSetActive(BLB_Object3D *object, bool active) {
  if (!object || !object->collider || !b3Shape_IsValid(object->collider->handle))
    return;

  object->collider->active = active;
  b3Shape_SetFilter(object->collider->handle, blb_physics3d_make_filter(object->collider), true);
}

bool BLB_Physics3D_ColliderIsActive(const BLB_Object3D *object) { return object && object->collider ? object->collider->active : false; }
