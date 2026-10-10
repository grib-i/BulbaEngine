#include "bulba/core/math3v/physics2d.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/scene.h"
#include "bulba/handlers/keyboard.h"
#include "test_common.h"
#include "tests.h"

#include <math.h>

static int setup_body2d(BLB_Physics2DWorld *world, BLB_Object2D *object, BLB_TestObject2DType object_type, BLB_Physics2DBodyType type,
                        HMM_Vec2 half_extents, float density, float friction, float restitution, BLB_RigidBody2D **out_body) {
  if (!world || !object || !out_body)
    return -1;
  BLB_RigidBody2D *body = BLB_Physics2D_BodyCreate(world, type);
  if (!body)
    return -1;
  BLB_Physics2D_BodySetPosition(body, object->position);
  BLB_Physics2D_BodySetRotation(body, object->rotation * HMM_PI / 180.0f);
  BLB_Physics2DShape shape = {0};
  if (object_type == BLB_TEST_OBJECT2D_CIRCLE) {
    shape.type = BLB_PHYSICS2D_SHAPE_CIRCLE;
    shape.data.circle.radius = fminf(half_extents.x, half_extents.y);
  } else {
    shape.type = BLB_PHYSICS2D_SHAPE_BOX;
    shape.data.box.half_extents = half_extents;
  }
  shape.user_data = object;
  BLB_Collider2D *collider = BLB_Physics2D_ColliderCreate(body, &shape);
  if (!collider) {
    BLB_Physics2D_BodyDestroy(world, body);
    return -1;
  }
  BLB_Physics2D_ColliderSetDensity(collider, density);
  BLB_Physics2D_ColliderSetFriction(collider, friction);
  BLB_Physics2D_ColliderSetRestitution(collider, restitution);
  *out_body = body;
  return 0;
}

int BLB_TestPhysicsObjectCount2D(size_t count, BLB_TestObject2DType type) {
  if (count == 0 || (type != BLB_TEST_OBJECT2D_SQUARE && type != BLB_TEST_OBJECT2D_CIRCLE))
    return -1;

  BLB_TestContext app;
  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Physics 2D Object Count", HMM_V3(0.0f, 0.0f, 30.0f), 6, 7, 11) != 0)
    return -1;

  BLB_Physics2DWorld *world = app.scene->physics_world2d;
  if (!world) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Physics2DWorld_SetGravity(world, HMM_V2(0.0f, -9.81f));
  BLB_Physics2DWorld_SetFixedTimestep(world, 1.0f / 60.0f);
  BLB_Physics2DWorld_SetSubsteps(world, 4);
  BLB_Physics2DWorld_SetLayerCollision(world, 0, 0, true);

  BLB_Material *shared_material = BLB_Material_Build(BLB_MATERIAL_2D, NULL);
  if (!shared_material) {
    BLB_TestContext_Shutdown(&app);
    return -1;
  }
  BLB_Material_SetLighting(shared_material, false);
  BLB_Material_SetUnlit(shared_material, true);
  BLB_Material_SetDepth(shared_material, false, false);
  BLB_Material_SetDoubleSided(shared_material, true);

  const size_t columns = (size_t)ceil(sqrt((double)count));
  const size_t rows = (count + columns - 1) / columns;
  const float spacing = 1.7f;
  const float half = 0.4f;
  const float width = (float)(columns - 1) * spacing + 1.0f;
  const float height = (float)(rows - 1) * spacing + 1.0f;
  const float floor_y = -(height * 0.5f + 2.0f);
  const float floor_width = fmaxf(16.0f, width * 0.6f + 8.0f);

  BLB_Object2D *floor = BLB_CreateSquare2D(HMM_V2(floor_width, 1.0f), HMM_V2(0.0f, floor_y), NULL, false);
  BLB_RigidBody2D *floor_body = NULL;
  if (!floor || setup_body2d(world, floor, BLB_TEST_OBJECT2D_SQUARE, BLB_PHYSICS2D_STATIC, HMM_V2(floor_width * 0.5f, 0.5f), 0.0f, 0.6f, 0.0f,
                             &floor_body) != 0) {
    if (floor)
      BLB_DestroySquare2D(floor);
    BLB_Material_Release(shared_material);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }
  BLB_Object2D_SetMaterial(floor, shared_material);
  floor->layer = 1;
  BLB_AddObject2D(app.scene, floor);

  BLB_Object2D **objects = calloc(count, sizeof(*objects));
  BLB_RigidBody2D **bodies = calloc(count, sizeof(*bodies));
  if (!objects || !bodies) {
    free(objects);
    free(bodies);
    BLB_Physics2D_BodyDestroy(world, floor_body);
    BLB_RemoveObject2D(app.scene, floor);
    BLB_DestroySquare2D(floor);
    BLB_Material_Release(shared_material);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  int error = 0;
  for (size_t i = 0; i < count; ++i) {
    size_t row = i / columns;
    size_t column = i % columns;
    float x = ((float)column - (float)(columns - 1) * 0.5f) * spacing;
    float y = floor_y + 3.0f + (float)row * spacing;
    if (type == BLB_TEST_OBJECT2D_SQUARE)
      objects[i] = BLB_CreateSquare2D(HMM_V2(0.8f, 0.8f), HMM_V2(x, y), NULL, false);
    else
      objects[i] = BLB_CreateCircle2D(HMM_V2(0.8f, 0.8f), HMM_V2(x, y), 5, NULL, false);
    if (!objects[i]) {
      error = 1;
      break;
    }
    BLB_Object2D_SetMaterial(objects[i], shared_material);
    objects[i]->layer = 1;
    objects[i]->color.x = 90 + (unsigned char)(i % 130);
    objects[i]->color.y = 180;
    objects[i]->color.z = 230;
    if (setup_body2d(world, objects[i], type, BLB_PHYSICS2D_DYNAMIC, HMM_V2(half, half), 1.0f, 0.5f, 0.2f, &bodies[i]) != 0 ||
        BLB_AddObject2D(app.scene, objects[i]) != 0) {
      error = 1;
      break;
    }
  }
  BLB_Material_Release(shared_material);

  if (error) {
    for (size_t i = 0; i < count; ++i) {
      if (bodies[i])
        BLB_Physics2D_BodyDestroy(world, bodies[i]);
      if (objects[i]) {
        if (type == BLB_TEST_OBJECT2D_SQUARE)
          BLB_DestroySquare2D(objects[i]);
        else
          BLB_DestroyCircle2D(objects[i]);
      }
    }
    free(bodies);
    free(objects);
    BLB_Physics2D_BodyDestroy(world, floor_body);
    BLB_RemoveObject2D(app.scene, floor);
    BLB_DestroySquare2D(floor);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  BLB_Camera_SetPosition(app.camera, HMM_V3(0.0f, 0.0f, fmaxf(25.0f, height * 0.7f + 18.0f)));
  app.camera->far_plane = 2000.0f;

  while (!BLB_WindowShouldClose(app.window)) {
    float delta_time = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &delta_time);
    if (frame < 0)
      break;
    if (frame > 0)
      continue;
    if (BLB_KeyboardPressed(&app.window->handlers, BLB_KEY_F3))
      BLB_Physics2DWorld_SetDebugDraw(world, !BLB_Physics2DWorld_GetDebugDraw(world));
    BLB_Physics2DWorld_Step(world, delta_time);
    for (size_t i = 0; i < count; ++i) {
      if (!objects[i] || !bodies[i])
        continue;
      HMM_Vec2 position = BLB_Physics2D_BodyGetPosition(bodies[i]);
      objects[i]->position = position;
      objects[i]->rotation = BLB_Physics2D_BodyGetRotation(bodies[i]) * 180.0f / HMM_PI;
    }
    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  for (size_t i = 0; i < count; ++i) {
    if (bodies[i])
      BLB_Physics2D_BodyDestroy(world, bodies[i]);
    if (objects[i]) {
      BLB_RemoveObject2D(app.scene, objects[i]);
      if (type == BLB_TEST_OBJECT2D_SQUARE) {
        BLB_DestroySquare2D(objects[i]);
        BLB_RemoveObject2D(app.scene, objects[i]);
      } else {
        BLB_DestroyCircle2D(objects[i]);
        BLB_RemoveObject2D(app.scene, objects[i]);
      }
    }
  }
  free(bodies);
  free(objects);
  BLB_Physics2D_BodyDestroy(world, floor_body);
  BLB_RemoveObject2D(app.scene, floor);
  BLB_DestroySquare2D(floor);
  BLB_TestContext_Shutdown(&app);
  return 0;
}

int BLB_Test_ph2d(void) { return BLB_TestPhysicsObjectCount2D(100, BLB_TEST_OBJECT2D_SQUARE); }
