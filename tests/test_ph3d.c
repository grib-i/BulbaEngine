#include "test_common.h"
#include "tests.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#define PH3D_LAYER_DEFAULT 0

#define PH3D_GRAVITY_X 0.0f
#define PH3D_GRAVITY_Y -9.81f
#define PH3D_GRAVITY_Z 0.0f
#define PH3D_FIXED_TIMESTEP (1.0f / 60.0f)
#define PH3D_SUBSTEPS 4

typedef struct {
  BLB_Object3D *object;
  BLB_RigidBody3D *body;
  BLB_Collider3D *collider;
} BLB_PhysicsTestObject;

static HMM_Quat euler_degrees_to_quat(HMM_Vec3 rotation) {
  const float deg_to_rad = (float)HMM_PI / 180.0f;
  const float x = rotation.x * deg_to_rad * 0.5f;
  const float y = rotation.y * deg_to_rad * 0.5f;
  const float z = rotation.z * deg_to_rad * 0.5f;
  const float cx = cosf(x);
  const float sx = sinf(x);
  const float cy = cosf(y);
  const float sy = sinf(y);
  const float cz = cosf(z);
  const float sz = sinf(z);
  HMM_Quat q;
  q.w = cx * cy * cz + sx * sy * sz;
  q.x = sx * cy * cz - cx * sy * sz;
  q.y = cx * sy * cz + sx * cy * sz;
  q.z = cx * cy * sz - sx * sy * cz;
  return q;
}

static HMM_Vec3 quat_to_euler_degrees(HMM_Quat q) {
  HMM_Vec3 result;
  const float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
  const float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
  result.x = atan2f(sinr_cosp, cosr_cosp);
  const float sinp = 2.0f * (q.w * q.y - q.z * q.x);
  if (fabsf(sinp) >= 1.0f)
    result.y = copysignf((float)HMM_PI * 0.5f, sinp);
  else
    result.y = asinf(sinp);
  const float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
  const float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
  result.z = atan2f(siny_cosp, cosy_cosp);
  const float rad_to_deg = 180.0f / (float)HMM_PI;
  result.x *= rad_to_deg;
  result.y *= rad_to_deg;
  result.z *= rad_to_deg;
  return result;
}

static void configure_collider(BLB_Collider3D *collider, float density, float friction, float restitution) {
  if (!collider)
    return;

  BLB_PhysicsFilter filter = {
      .layer = PH3D_LAYER_DEFAULT,
      .mask = UINT64_MAX,
      .group = 0,
  };

  BLB_Physics3D_ColliderSetFilter(collider, filter);
  BLB_Physics3D_ColliderSetDensity(collider, density);
  BLB_Physics3D_ColliderSetFriction(collider, friction);
  BLB_Physics3D_ColliderSetRestitution(collider, restitution);
  BLB_Physics3D_ColliderSetTrigger(collider, false);
  BLB_Physics3D_ColliderSetActive(collider, true);
}

static int setup_box(BLB_Physics3DWorld *world, BLB_Object3D *object, BLB_Physics3DBodyType type, HMM_Vec3 half_extents, float density,
                     float friction, float restitution, BLB_RigidBody3D **out_body, BLB_Collider3D **out_collider) {
  if (!world || !object || !out_body)
    return -1;

  BLB_RigidBody3D *body = BLB_Physics3D_BodyCreate(world, type);
  if (!body)
    return -1;

  BLB_Physics3D_BodySetPosition(body, object->position);
  BLB_Physics3D_BodySetRotation(body, euler_degrees_to_quat(object->rotation));

  BLB_Physics3DShape shape = {0};
  shape.type = BLB_PHYSICS3D_SHAPE_BOX;
  shape.data.box.half_extents = half_extents;
  shape.user_data = object;

  BLB_Collider3D *collider = BLB_Physics3D_ColliderCreate(body, &shape);
  if (!collider) {
    BLB_Physics3D_BodyDestroy(world, body);
    return -1;
  }

  configure_collider(collider, density, friction, restitution);

  *out_body = body;
  if (out_collider)
    *out_collider = collider;
  return 0;
}

static int setup_sphere(BLB_Physics3DWorld *world, BLB_Object3D *object, float radius, float density, float friction, float restitution,
                        BLB_RigidBody3D **out_body, BLB_Collider3D **out_collider) {
  if (!world || !object || !out_body)
    return -1;

  BLB_RigidBody3D *body = BLB_Physics3D_BodyCreate(world, BLB_PHYSICS3D_DYNAMIC);
  if (!body)
    return -1;

  BLB_Physics3D_BodySetPosition(body, object->position);
  BLB_Physics3D_BodySetRotation(body, euler_degrees_to_quat(object->rotation));

  BLB_Physics3DShape shape = {0};
  shape.type = BLB_PHYSICS3D_SHAPE_SPHERE;
  shape.data.sphere.radius = radius;
  shape.user_data = object;

  BLB_Collider3D *collider = BLB_Physics3D_ColliderCreate(body, &shape);
  if (!collider) {
    BLB_Physics3D_BodyDestroy(world, body);
    return -1;
  }

  configure_collider(collider, density, friction, restitution);

  *out_body = body;
  if (out_collider)
    *out_collider = collider;
  return 0;
}

int BLB_Test_ph3d(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Physics 3D", HMM_V3(0.0f, 6.0f, 18.0f), 7, 9, 15) != 0)
    return -1;

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(0.0f, 8.0f, 8.0f), HMM_V3(0.0f, 0.0f, 0.0f), 18.0f, 0.025f, 1.0f, 32.0f);
  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-8.0f, 5.0f, 3.0f), HMM_V3(0.0f, 0.0f, 0.0f), 8.0f, 0.01f, 0.7f, 28.0f);

  BLB_Physics3DWorld *world = app.scene->physics_world3d;
  if (!world) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Physics3DWorld_SetGravity(world, HMM_V3(PH3D_GRAVITY_X, -PH3D_GRAVITY_Y, PH3D_GRAVITY_Z));
  BLB_Physics3DWorld_SetFixedTimestep(world, PH3D_FIXED_TIMESTEP);
  BLB_Physics3DWorld_SetSubsteps(world, PH3D_SUBSTEPS);

  BLB_Physics3DWorld_SetLayerCollision(world, 0, 0, true);

  BLB_Object3D *floor = BLB_CreateCube3D(HMM_V3(14.0f, 1.0f, 14.0f), HMM_V3(0.0f, 16.0f, 0.0f), NULL);
  BLB_Object3D *cube1 = BLB_CreateCube3D(HMM_V3(2.0f, 2.0f, 2.0f), HMM_V3(-3.5f, 7.0f, 0.0f), NULL);
  BLB_Object3D *cube2 = BLB_CreateCube3D(HMM_V3(2.0f, 2.0f, 2.0f), HMM_V3(0.0f, 11.0f, 0.0f), NULL);
  BLB_Object3D *cube3 = BLB_CreateCube3D(HMM_V3(2.0f, 2.0f, 2.0f), HMM_V3(3.5f, 15.0f, 0.0f), NULL);
  BLB_Object3D *sphere1 = BLB_CreateSphere3D(HMM_V3(1.8f, 1.8f, 1.8f), HMM_V3(2.0f, 8.5f, 0.0f), 4, NULL);
  BLB_Object3D *sphere2 = BLB_CreateSphere3D(HMM_V3(1.8f, 1.8f, 1.8f), HMM_V3(-2.0f, 13.0f, 0.0f), 4, NULL);

  BLB_RigidBody3D *floor_body = NULL;
  if (floor) {
    floor->rotation = HMM_V3(0.0f, 0.0f, 0.0f);
    if (setup_box(world, floor, BLB_PHYSICS3D_STATIC, HMM_V3(7.0f, 0.5f, 7.0f), 1.0f, 0.5f, 0.0f, &floor_body, NULL) == 0) {
      BLB_AddObject3D(app.scene, floor);
    } else {
      BLB_DestroyCube3D(floor);
      floor = NULL;
    }
  }

  BLB_PhysicsTestObject objects[] = {
      {cube1, NULL, NULL}, {cube2, NULL, NULL}, {cube3, NULL, NULL}, {sphere1, NULL, NULL}, {sphere2, NULL, NULL},
  };

  const size_t object_count = sizeof(objects) / sizeof(objects[0]);

  if (cube1)
    cube1->rotation = HMM_V3(15.0f, 20.0f, 10.0f);
  if (cube2)
    cube2->rotation = HMM_V3(25.0f, 0.0f, 30.0f);
  if (cube3)
    cube3->rotation = HMM_V3(0.0f, 35.0f, 20.0f);

  for (size_t i = 0; i < object_count; ++i) {
    BLB_PhysicsTestObject *test_object = &objects[i];
    if (!test_object->object)
      continue;

    int result;

    if (test_object->object == sphere1 || test_object->object == sphere2) {
      result = setup_sphere(world, test_object->object, 0.9f, 1.0f, 0.5f, 0.25f, &test_object->body, &test_object->collider);
    } else {
      result = setup_box(world, test_object->object, BLB_PHYSICS3D_DYNAMIC, HMM_V3(1.0f, 1.0f, 1.0f), 1.0f, 0.5f, 0.1f, &test_object->body,
                         &test_object->collider);
    }

    if (result == 0)
      BLB_AddObject3D(app.scene, test_object->object);
    else {
      if (test_object->object == sphere1 || test_object->object == sphere2)
        BLB_DestroySphere3D(test_object->object);
      else
        BLB_DestroyCube3D(test_object->object);
      test_object->object = NULL;
    }
  }

  while (!BLB_WindowShouldClose(app.window)) {
    float delta_time = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &delta_time);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    BLB_Physics3DWorld_Step(world, delta_time);

    for (size_t i = 0; i < object_count; ++i) {
      BLB_PhysicsTestObject *test_object = &objects[i];

      if (!test_object->object || !test_object->body)
        continue;

      test_object->object->position = BLB_Physics3D_BodyGetPosition(test_object->body);
      test_object->object->rotation = quat_to_euler_degrees(BLB_Physics3D_BodyGetRotation(test_object->body));
    }

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  for (size_t i = 0; i < object_count; ++i) {
    if (objects[i].body)
      BLB_Physics3D_BodyDestroy(world, objects[i].body);
  }

  if (floor_body)
    BLB_Physics3D_BodyDestroy(world, floor_body);

  BLB_DestroyCube3D(floor);
  BLB_DestroyCube3D(cube1);
  BLB_DestroyCube3D(cube2);
  BLB_DestroyCube3D(cube3);
  BLB_DestroySphere3D(sphere1);
  BLB_DestroySphere3D(sphere2);

  BLB_TestContext_Shutdown(&app);
  return 0;
}

#include <stdlib.h>

static void fit_physics_camera(BLB_TestContext *app, size_t columns, size_t rows, float spacing, float radius) {
  if (!app || !app->camera || !columns || !rows)
    return;

  float width = (float)(columns - 1) * spacing + radius * 2.0f;
  float height = (float)(rows - 1) * spacing + radius * 2.0f;
  float aspect = 1.0f;

  if (app->vk.swapchain_extent.height > 0)
    aspect = (float)app->vk.swapchain_extent.width / (float)app->vk.swapchain_extent.height;

  if (aspect <= 0.0f)
    aspect = 1.0f;

  float half_vertical_fov = app->camera->fov * 0.5f;
  float tan_vertical = tanf(half_vertical_fov);
  float horizontal_fov = 2.0f * atanf(tan_vertical * aspect);
  float tan_horizontal = tanf(horizontal_fov * 0.5f);

  float distance_vertical = height * 0.5f / tan_vertical;
  float distance_horizontal = width * 0.5f / tan_horizontal;
  float distance = fmaxf(distance_vertical, distance_horizontal) * 1.15f + radius;

  BLB_Camera_SetPosition(app->camera, HMM_V3(0.0f, 0.0f, distance));
  app->camera->far_plane = fmaxf(1000.0f, distance * 4.0f);
}

int BLB_TestPhysicsObjectCount(size_t count, BLB_TestObjectType type) {
  if (count == 0)
    return -1;

  if (type != BLB_TEST_OBJECT_CUBE && type != BLB_TEST_OBJECT_SPHERE)
    return -1;

  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Physics Object Count", HMM_V3(0.0f, 0.0f, 18.0f), 7, 9, 15) != 0)
    return -1;

  BLB_Physics3DWorld *world = app.scene->physics_world3d;
  if (!world) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Physics3DWorld_SetGravity(world, HMM_V3(0.0f, -PH3D_GRAVITY_Y, 0.0f));
  BLB_Physics3DWorld_SetFixedTimestep(world, PH3D_FIXED_TIMESTEP);
  BLB_Physics3DWorld_SetSubsteps(world, PH3D_SUBSTEPS);
  BLB_Physics3DWorld_SetLayerCollision(world, 0, 0, true);

  size_t columns = (size_t)ceil(sqrt((double)count));
  if (columns < 1)
    columns = 1;

  size_t rows = (count + columns - 1) / columns;

  float radius = 0.4f;
  float spacing = 2.0f;

  float width = (float)(columns - 1) * spacing + radius * 2.0f;
  float height = (float)(rows - 1) * spacing + radius * 2.0f;

  float floor_width = fmaxf(24.0f, width + 12.0f);
  float floor_depth = 24.0f;
  float floor_y = -height * 0.5f - 2.0f;

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(0.0f, height * 0.5f + 10.0f, 14.0f), HMM_V3(0.0f, 0.0f, 0.0f), 24.0f, 0.04f, 1.0f,
                           fmaxf(100.0f, (float)count * 0.08f));

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-width * 0.5f, 4.0f, 8.0f), HMM_V3(0.0f, 0.0f, 0.0f), 10.0f, 0.02f, 0.65f,
                           fmaxf(100.0f, (float)count * 0.08f));

  BLB_Material *shared_material = BLB_Material_Build(BLB_MATERIAL_3D, NULL);
  if (!shared_material) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Material_SetBaseColor(shared_material, 0.58f, 0.66f, 0.78f, 1.0f);
  BLB_Material_SetPBR(shared_material, 0.12f, 0.38f);
  BLB_Material_SetSpecular(shared_material, 0.85f, 0.92f, 0.96f, 1.0f);

  BLB_Object3D *floor = BLB_CreateCube3D(HMM_V3(floor_width, 1.0f, floor_depth), HMM_V3(0.0f, -floor_y, 0.0f), NULL);

  BLB_RigidBody3D *floor_body = NULL;

  if (!floor ||
      setup_box(world, floor, BLB_PHYSICS3D_STATIC, HMM_V3(floor_width * 0.5f, 0.5f, floor_depth * 0.5f), 1.0f, 0.5f, 0.0f, &floor_body, NULL) != 0) {
    if (floor)
      BLB_DestroyCube3D(floor);

    BLB_Material_Release(shared_material);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Object3D_SetMaterial(floor, shared_material);
  floor->layer = 1;

  if (BLB_AddObject3D(app.scene, floor) != 0) {
    BLB_Physics3D_BodyDestroy(world, floor_body);
    BLB_DestroyCube3D(floor);
    BLB_Material_Release(shared_material);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Object3D **objects = calloc(count, sizeof(*objects));
  BLB_RigidBody3D **bodies = calloc(count, sizeof(*bodies));

  if (!objects || !bodies) {
    free(objects);
    free(bodies);
    BLB_Physics3D_BodyDestroy(world, floor_body);
    BLB_DestroyCube3D(floor);
    BLB_Material_Release(shared_material);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  size_t created = 0;
  int error = 0;

  for (size_t i = 0; i < count; ++i) {
    size_t row = i / columns;
    size_t column = i % columns;

    float x = ((float)column - (float)(columns - 1) * 0.5f) * spacing;
    float y = floor_y + 2.0f + (float)row * spacing;

    BLB_Object3D *object = NULL;

    if (type == BLB_TEST_OBJECT_CUBE)
      object = BLB_CreateCube3D(HMM_V3(1.8f, 1.8f, 1.8f), HMM_V3(x, y, 0.0f), NULL);
    else
      object = BLB_CreateSphere3D(HMM_V3(1.8f, 1.8f, 1.8f), HMM_V3(x, y, 0.0f), 5, NULL);

    if (!object) {
      error = 1;
      break;
    }

    BLB_Object3D_SetMaterial(object, shared_material);
    object->layer = 1;

    if (type == BLB_TEST_OBJECT_CUBE) {
      object->rotation = HMM_V3((float)((i * 7) % 360), (float)((i * 13) % 360), (float)((i * 5) % 360));
    }

    BLB_RigidBody3D *body = NULL;
    int result = 0;

    if (type == BLB_TEST_OBJECT_CUBE) {
      result = setup_box(world, object, BLB_PHYSICS3D_DYNAMIC, HMM_V3(0.9f, 0.9f, 0.9f), 1.0f, 0.5f, 0.1f, &body, NULL);
    } else {
      result = setup_sphere(world, object, 0.9f, 1.0f, 0.5f, 0.25f, &body, NULL);
    }

    if (result != 0 || !body) {
      if (type == BLB_TEST_OBJECT_CUBE)
        BLB_DestroyCube3D(object);
      else
        BLB_DestroySphere3D(object);

      error = 1;
      break;
    }

    if (BLB_AddObject3D(app.scene, object) != 0) {
      BLB_Physics3D_BodyDestroy(world, body);

      if (type == BLB_TEST_OBJECT_CUBE)
        BLB_DestroyCube3D(object);
      else
        BLB_DestroySphere3D(object);

      error = 1;
      break;
    }

    objects[i] = object;
    bodies[i] = body;
    created++;
  }

  BLB_Material_Release(shared_material);

  if (error) {
    for (size_t i = 0; i < created; ++i) {
      if (bodies[i])
        BLB_Physics3D_BodyDestroy(world, bodies[i]);

      if (objects[i]) {
        if (type == BLB_TEST_OBJECT_CUBE)
          BLB_DestroyCube3D(objects[i]);
        else
          BLB_DestroySphere3D(objects[i]);
      }
    }

    free(objects);
    free(bodies);

    BLB_Physics3D_BodyDestroy(world, floor_body);
    BLB_DestroyCube3D(floor);

    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  fit_physics_camera(&app, columns, rows, spacing, radius);

  while (!BLB_WindowShouldClose(app.window)) {
    float dt = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &dt);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    BLB_Physics3DWorld_Step(world, dt);

    for (size_t i = 0; i < count; ++i) {
      if (!objects[i] || !bodies[i])
        continue;

      objects[i]->position = BLB_Physics3D_BodyGetPosition(bodies[i]);
      objects[i]->rotation = quat_to_euler_degrees(BLB_Physics3D_BodyGetRotation(bodies[i]));
    }

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  for (size_t i = 0; i < count; ++i) {
    if (bodies[i])
      BLB_Physics3D_BodyDestroy(world, bodies[i]);

    if (objects[i]) {
      if (type == BLB_TEST_OBJECT_CUBE)
        BLB_DestroyCube3D(objects[i]);
      else
        BLB_DestroySphere3D(objects[i]);
    }
  }

  free(objects);
  free(bodies);

  if (floor_body)
    BLB_Physics3D_BodyDestroy(world, floor_body);

  BLB_DestroyCube3D(floor);

  BLB_TestContext_Shutdown(&app);

  return 0;
}
