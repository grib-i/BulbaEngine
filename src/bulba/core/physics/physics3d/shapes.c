#include "bulba/core/math3v/vec_math.h"
#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/physics/physics3d.h"
#include "physics3d.h"
#include <limits.h>
#include <stdint.h>
#include <time.h>

void blb_physics3d_free_shape_copy(BLB_Physics3DShape *shape) {
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
  } else if (shape->type == BLB_PHYSICS3D_SHAPE_COMPOUND) {
    free(shape->data.compound.shapes);
    shape->data.compound.shapes = NULL;
    shape->data.compound.shape_count = 0;
  }
}

bool blb_physics3d_copy_shape(BLB_Physics3DShape *dst, const BLB_Physics3DShape *src) {
  if (!dst || !src)
    return false;

  *dst = *src;

  if (src->type == BLB_PHYSICS3D_SHAPE_CONVEX_HULL) {
    dst->data.convex_hull.vertices = NULL;

    if (!src->data.convex_hull.vertices || src->data.convex_hull.vertex_count < 4 ||
        src->data.convex_hull.vertex_count > INT_MAX ||
        src->data.convex_hull.vertex_count > SIZE_MAX / sizeof(BLB_Vec3))
      return false;

    dst->data.convex_hull.vertices = malloc(src->data.convex_hull.vertex_count *
                                            sizeof(*dst->data.convex_hull.vertices));

    if (!dst->data.convex_hull.vertices)
      return false;

    memcpy(dst->data.convex_hull.vertices, src->data.convex_hull.vertices,
           src->data.convex_hull.vertex_count * sizeof(*dst->data.convex_hull.vertices));
    for (size_t i = 0; i < src->data.convex_hull.vertex_count; ++i) {
      BLB_Vec3 v = dst->data.convex_hull.vertices[i];
      if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z)) {
        blb_physics3d_free_shape_copy(dst);
        return false;
      }
    }
  } else if (src->type == BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH) {

    dst->data.triangle_mesh.vertices = NULL;
    dst->data.triangle_mesh.indices = NULL;

    if (!src->data.triangle_mesh.vertices || !src->data.triangle_mesh.indices ||
        src->data.triangle_mesh.vertex_count < 3 ||
        src->data.triangle_mesh.index_count < 3 ||
        src->data.triangle_mesh.index_count % 3 != 0 ||
        src->data.triangle_mesh.vertex_count > SIZE_MAX / sizeof(BLB_Vec3) ||
        src->data.triangle_mesh.index_count > SIZE_MAX / sizeof(uint32_t))
      return false;

    for (size_t i = 0; i < src->data.triangle_mesh.index_count; ++i)
      if (src->data.triangle_mesh.indices[i] >= src->data.triangle_mesh.vertex_count)
        return false;

    if (src->data.triangle_mesh.vertex_count > INT_MAX ||
        src->data.triangle_mesh.index_count / 3 > INT_MAX)
      return false;

    dst->data.triangle_mesh.vertices = malloc(src->data.triangle_mesh.vertex_count *
                                              sizeof(*dst->data.triangle_mesh.vertices));

    if (!dst->data.triangle_mesh.vertices)
      return false;

    dst->data.triangle_mesh.indices = malloc(src->data.triangle_mesh.index_count *
                                             sizeof(*dst->data.triangle_mesh.indices));

    if (!dst->data.triangle_mesh.indices) {
      free(dst->data.triangle_mesh.vertices);
      dst->data.triangle_mesh.vertices = NULL;

      return false;
    }

    memcpy(dst->data.triangle_mesh.vertices, src->data.triangle_mesh.vertices,
           src->data.triangle_mesh.vertex_count *
               sizeof(*dst->data.triangle_mesh.vertices));

    memcpy(dst->data.triangle_mesh.indices, src->data.triangle_mesh.indices,
           src->data.triangle_mesh.index_count *
               sizeof(*dst->data.triangle_mesh.indices));
    for (size_t i = 0; i < src->data.triangle_mesh.vertex_count; ++i) {
      BLB_Vec3 v = dst->data.triangle_mesh.vertices[i];
      if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z)) {
        blb_physics3d_free_shape_copy(dst);
        return false;
      }
    }
    BLB_Vec3 scale = src->data.triangle_mesh.scale;
    if (!isfinite(scale.x) || !isfinite(scale.y) || !isfinite(scale.z) ||
        scale.x <= 0.0f || scale.y <= 0.0f || scale.z <= 0.0f) {
      blb_physics3d_free_shape_copy(dst);
      return false;
    }
  } else if (src->type == BLB_PHYSICS3D_SHAPE_COMPOUND) {
    dst->data.compound.shapes = NULL;
    if (!src->data.compound.shapes || src->data.compound.shape_count == 0 ||
        src->data.compound.shape_count > INT_MAX ||
        src->data.compound.shape_count > SIZE_MAX / sizeof(BLB_Physics3DShape))
      return false;

    const BLB_Physics3DShape *children = src->data.compound.shapes;
    for (size_t i = 0; i < src->data.compound.shape_count; ++i) {
      if (children[i].type != BLB_PHYSICS3D_SHAPE_BOX &&
          children[i].type != BLB_PHYSICS3D_SHAPE_SPHERE &&
          children[i].type != BLB_PHYSICS3D_SHAPE_CAPSULE)
        return false;
      if (children[i].type == BLB_PHYSICS3D_SHAPE_BOX) {
        BLB_Vec3 v = children[i].data.box.scale;
        if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z) ||
            v.x <= 0.0f || v.y <= 0.0f || v.z <= 0.0f)
          return false;
      } else if (children[i].type == BLB_PHYSICS3D_SHAPE_SPHERE) {
        if (!isfinite(children[i].data.sphere.radius) || children[i].data.sphere.radius <= 0.0f)
          return false;
      } else if (!isfinite(children[i].data.capsule.radius) ||
                 !isfinite(children[i].data.capsule.height) ||
                 children[i].data.capsule.radius <= 0.0f || children[i].data.capsule.height < 0.0f) {
        return false;
      }
    }

    dst->data.compound.shapes = malloc(src->data.compound.shape_count * sizeof(BLB_Physics3DShape));
    if (!dst->data.compound.shapes)
      return false;
    memcpy(dst->data.compound.shapes, children,
           src->data.compound.shape_count * sizeof(BLB_Physics3DShape));
  } else if (src->type == BLB_PHYSICS3D_SHAPE_BOX) {
    BLB_Vec3 v = src->data.box.scale;
    if (!isfinite(v.x) || !isfinite(v.y) || !isfinite(v.z) ||
        v.x <= 0.0f || v.y <= 0.0f || v.z <= 0.0f)
      return false;
  } else if (src->type == BLB_PHYSICS3D_SHAPE_SPHERE) {
    if (!isfinite(src->data.sphere.radius) || src->data.sphere.radius <= 0.0f)
      return false;
  } else if (src->type == BLB_PHYSICS3D_SHAPE_CAPSULE) {
    if (!isfinite(src->data.capsule.radius) || !isfinite(src->data.capsule.height) ||
        src->data.capsule.radius <= 0.0f || src->data.capsule.height < 0.0f)
      return false;
  } else if (src->type != BLB_PHYSICS3D_SHAPE_PLANE) {
    return false;
  }

  return true;
}

b3ShapeId blb_physics3d_create_backend_shape(const BLB_Collider3D *collider,
                                             const b3ShapeDef *def) {
  if (!collider || !collider->body || !def)
    return b3_nullShapeId;

  b3BodyId body = collider->body->handle;

  switch (collider->shape.type) {
  case BLB_PHYSICS3D_SHAPE_BOX: {
    BLB_Vec3 h = collider->shape.data.box.scale;
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
        .center1 = {0.0f, -collider->shape.data.capsule.height, 0.0f},
        .center2 = {0.0f, collider->shape.data.capsule.height, 0.0f},
        .radius = collider->shape.data.capsule.radius,
    };

    return b3CreateCapsuleShape(body, def, &capsule);
  }

  case BLB_PHYSICS3D_SHAPE_CONVEX_HULL: {
    if (!collider->shape.data.convex_hull.vertices ||
        collider->shape.data.convex_hull.vertex_count < 4)
      return b3_nullShapeId;

    b3HullData *hull =
        b3CreateHull((const b3Vec3 *)collider->shape.data.convex_hull.vertices,
                     (int)collider->shape.data.convex_hull.vertex_count,
                     (int)collider->shape.data.convex_hull.vertex_count);

    if (!hull)
      return b3_nullShapeId;

    b3ShapeId result = b3CreateHullShape(body, def, hull);
    b3DestroyHull(hull);

    return result;
  }

  case BLB_PHYSICS3D_SHAPE_PLANE:
    return b3_nullShapeId;

  case BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH: {
    const __typeof__(collider->shape.data.triangle_mesh) *mesh_data =
        &collider->shape.data.triangle_mesh;

    if (collider->body->type != BLB_PHYSICS3D_STATIC)
      return b3_nullShapeId;

    b3MeshDef mesh_def = {0};

    _Static_assert(sizeof(BLB_Vec3) == sizeof(b3Vec3), "Vector sizes differ");
    b3Vec3 *vertices = malloc(mesh_data->vertex_count * sizeof(*vertices));

    if (!vertices)
      return b3_nullShapeId;

    memcpy(vertices, mesh_data->vertices, mesh_data->vertex_count * sizeof(*vertices));
    mesh_def.vertices = vertices;

    mesh_def.vertexCount = mesh_data->vertex_count;
    mesh_def.indices = (int32_t *)mesh_data->indices;
    mesh_def.triangleCount = mesh_data->index_count / 3;
    mesh_def.weldVertices = true;
    mesh_def.identifyEdges = true;

    b3MeshData *mesh = b3CreateMesh(&mesh_def, NULL, 0);
    free(vertices);

    if (!mesh)
      return b3_nullShapeId;

    b3ShapeId result = b3CreateMeshShape(
        body, def, mesh,
        (b3Vec3){mesh_data->scale.x, mesh_data->scale.y, mesh_data->scale.z});

    if (b3Shape_IsValid(result))
      ((BLB_Collider3D *)collider)->mesh_data = mesh;
    else
      b3DestroyMesh(mesh);

    return result;
  }

  case BLB_PHYSICS3D_SHAPE_COMPOUND: {
    if (collider->body->type != BLB_PHYSICS3D_STATIC ||
        !collider->shape.data.compound.shapes ||
        collider->shape.data.compound.shape_count == 0 ||
        collider->shape.data.compound.shape_count > INT_MAX)
      return b3_nullShapeId;

    const BLB_Physics3DShape *children = collider->shape.data.compound.shapes;
    size_t child_count = collider->shape.data.compound.shape_count;
    size_t sphere_count = 0;
    size_t capsule_count = 0;
    size_t hull_count = 0;
    for (size_t i = 0; i < child_count; ++i) {
      switch (children[i].type) {
      case BLB_PHYSICS3D_SHAPE_SPHERE: ++sphere_count; break;
      case BLB_PHYSICS3D_SHAPE_CAPSULE: ++capsule_count; break;
      case BLB_PHYSICS3D_SHAPE_BOX: ++hull_count; break;
      default: return b3_nullShapeId;
      }
    }

    b3CompoundSphereDef *spheres = sphere_count ? calloc(sphere_count, sizeof(*spheres)) : NULL;
    b3CompoundCapsuleDef *capsules = capsule_count ? calloc(capsule_count, sizeof(*capsules)) : NULL;
    b3CompoundHullDef *hulls = hull_count ? calloc(hull_count, sizeof(*hulls)) : NULL;
    b3BoxHull *box_hulls = hull_count ? calloc(hull_count, sizeof(*box_hulls)) : NULL;
    if ((sphere_count && !spheres) || (capsule_count && !capsules) ||
        (hull_count && (!hulls || !box_hulls))) {
      free(spheres);
      free(capsules);
      free(hulls);
      free(box_hulls);
      return b3_nullShapeId;
    }

    b3SurfaceMaterial material = b3DefaultSurfaceMaterial();
    material.friction = collider->friction;
    material.restitution = collider->restitution;
    size_t sphere_index = 0;
    size_t capsule_index = 0;
    size_t hull_index = 0;
    for (size_t i = 0; i < child_count; ++i) {
      if (children[i].type == BLB_PHYSICS3D_SHAPE_SPHERE) {
        spheres[sphere_index].sphere.center = (b3Vec3){0.0f, 0.0f, 0.0f};
        spheres[sphere_index].sphere.radius = children[i].data.sphere.radius;
        spheres[sphere_index++].material = material;
      } else if (children[i].type == BLB_PHYSICS3D_SHAPE_CAPSULE) {
        capsules[capsule_index].capsule.center1 =
            (b3Vec3){0.0f, -children[i].data.capsule.height, 0.0f};
        capsules[capsule_index].capsule.center2 =
            (b3Vec3){0.0f, children[i].data.capsule.height, 0.0f};
        capsules[capsule_index].capsule.radius = children[i].data.capsule.radius;
        capsules[capsule_index++].material = material;
      } else {
        BLB_Vec3 h = children[i].data.box.scale;
        box_hulls[hull_index] = b3MakeBoxHull(h.x, h.y, h.z);
        hulls[hull_index].hull = &box_hulls[hull_index].base;
        hulls[hull_index].transform.p = (b3Vec3){0.0f, 0.0f, 0.0f};
        hulls[hull_index].transform.q = (b3Quat){0.0f, 0.0f, 0.0f, 1.0f};
        hulls[hull_index++].material = material;
      }
    }

    b3CompoundDef compound_def = {0};
    compound_def.spheres = spheres;
    compound_def.sphereCount = (int)sphere_count;
    compound_def.capsules = capsules;
    compound_def.capsuleCount = (int)capsule_count;
    compound_def.hulls = hulls;
    compound_def.hullCount = (int)hull_count;
    b3CompoundData *compound = b3CreateCompound(&compound_def);
    free(spheres);
    free(capsules);
    free(hulls);
    free(box_hulls);
    if (!compound)
      return b3_nullShapeId;

    b3ShapeDef mutable_def = *def;
    b3ShapeId result = b3CreateCompoundShape(body, &mutable_def, compound);
    b3DestroyCompound(compound);
    return result;
  }

  default:
    return b3_nullShapeId;
  }
}

bool blb_physics3d_recreate_shape(BLB_Collider3D *collider, bool trigger) {
  if (!collider || !b3Shape_IsValid(collider->handle))
    return false;

  b3ShapeDef def = b3DefaultShapeDef();
  def.density = collider->density;
  def.baseMaterial.friction = collider->friction;
  def.baseMaterial.restitution = collider->restitution;
  def.filter = blb_physics3d_make_filter(collider);
  def.isSensor = trigger;
  def.updateBodyMass = false;

  b3MeshData *old_mesh = collider->mesh_data;
  b3ShapeId replacement = blb_physics3d_create_backend_shape(collider, &def);

  if (!b3Shape_IsValid(replacement))
    return false;
  b3ShapeId old = collider->handle;

  collider->handle = replacement;
  collider->trigger = trigger;

  if (collider->shape.type != BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH)
    collider->mesh_data = NULL;

  b3Shape_SetUserData(replacement, collider->user_data);
  b3DestroyShape(old, false);

  if (old_mesh)
    b3DestroyMesh(old_mesh);

  b3Body_ApplyMassFromShapes(collider->body->handle);

  return true;
}

BLB_Physics3DShape *BLB_Physics3D_CreateShape(BLB_Physics3DShapeType type,
                                              BLB_Vec3 scale) {
  if (!isfinite(scale.x) || !isfinite(scale.y) || !isfinite(scale.z) || scale.x <= 0 ||
      scale.y <= 0 || scale.z <= 0)
    return NULL;

  if (type != BLB_PHYSICS3D_SHAPE_BOX && type != BLB_PHYSICS3D_SHAPE_SPHERE &&
      type != BLB_PHYSICS3D_SHAPE_CAPSULE)
    return NULL;

  BLB_Physics3DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  shape->type = type;
  BLB_Vec3 half = BLB_MulV3F(scale, 0.5f);

  if (type == BLB_PHYSICS3D_SHAPE_BOX) {
    shape->data.box.scale = half;

  } else if (type == BLB_PHYSICS3D_SHAPE_SPHERE) {
    shape->data.sphere.radius = fminf(half.x, fminf(half.y, half.z));

  } else {
    shape->data.capsule.radius = fminf(half.x, half.z);
    shape->data.capsule.height = fmaxf(0.0f, half.y - shape->data.capsule.radius);
  }

  return shape;
}

BLB_Physics3DShape *BLB_Physics3D_CreateShapeCompound(BLB_Physics3DShape *shapes,
                                                      size_t count) {
  if (!shapes || count == 0 || count > INT_MAX ||
      count > SIZE_MAX / sizeof(BLB_Physics3DShape))
    return NULL;

  BLB_Physics3DShape source = {.type = BLB_PHYSICS3D_SHAPE_COMPOUND};
  source.data.compound.shapes = shapes;
  source.data.compound.shape_count = count;

  BLB_Physics3DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  if (!blb_physics3d_copy_shape(shape, &source)) {
    blb_physics3d_free_shape_copy(shape);
    free(shape);

    return NULL;
  }

  return shape;
}

BLB_Physics3DShape *BLB_Physics3D_CreateTriangleMeshShape(BLB_Vec3 scale,
                                                          BLB_Vec3 *vertices,
                                                          size_t vertex_count,
                                                          uint32_t *indices,
                                                          size_t index_count) {
  BLB_Physics3DShape source = {.type = BLB_PHYSICS3D_SHAPE_TRIANGLE_MESH};
  source.data.triangle_mesh.vertices = vertices;
  source.data.triangle_mesh.vertex_count = vertex_count;
  source.data.triangle_mesh.indices = indices;
  source.data.triangle_mesh.index_count = index_count;
  source.data.triangle_mesh.scale = scale;

  BLB_Physics3DShape *shape = calloc(1, sizeof(*shape));

  if (!shape)
    return NULL;

  if (!blb_physics3d_copy_shape(shape, &source)) {
    blb_physics3d_free_shape_copy(shape);
    free(shape);

    return NULL;
  }

  return shape;
}

BLB_Physics3DShape *BLB_Physics3D_CreateShapeFromObject3D(const BLB_Object3D *object) {
  if (!object || !object->polygon)
    return NULL;

  return BLB_Physics3D_CreateTriangleMeshShape(
      object->scale, object->polygon->vertices, object->polygon->vertex_count,
      object->polygon->indices, object->polygon->index_count);
}

void BLB_Physics3D_DestroyShape(BLB_Physics3DShape *shape) {
  if (!shape)
    return;

  blb_physics3d_free_shape_copy(shape);
  free(shape);
}
