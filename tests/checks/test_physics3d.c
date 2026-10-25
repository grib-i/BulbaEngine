#include "test.h"

static void body_and_collider_contract(void) {
  BLB_Physics3DWorld *world = BLB_Physics3D_WorldCreate();

  TEST_REQUIRE(world);
  BLB_Object3D *object = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV);

  TEST_CHECK(object);

  if (object) {
    BLB_Physics3D_SetBody(world, object, BLB_PHYSICS3D_DYNAMIC);
    TEST_CHECK(BLB_Physics3D_GetBody(object));
    TEST_CHECK(BLB_Physics3D_BodyGetId(object) != 0);
    TEST_CHECK(BLB_Physics3D_BodyGetType(object) == BLB_PHYSICS3D_DYNAMIC);
    BLB_Physics3DShape *shape = BLB_Physics3D_CreateShape(BLB_PHYSICS3D_SHAPE_BOX, BLB_V3(1, 1, 1));

    TEST_CHECK(shape);
    if (shape) {
      TEST_CHECK(BLB_Physics3D_SetCollider(object, shape));
      BLB_Physics3D_DestroyShape(shape);
      TEST_CHECK(BLB_Physics3D_ColliderGetId(object) != 0);
      BLB_Physics3D_ColliderSetFriction(object, 0.7f);
      BLB_Physics3D_ColliderSetRestitution(object, 0.2f);
      BLB_Physics3D_ColliderSetDensity(object, 2.0f);
      BLB_Physics3D_ColliderSetTrigger(object, true);
      TEST_NEAR(BLB_Physics3D_ColliderGetFriction(object), 0.7, 1e-5);
      TEST_NEAR(BLB_Physics3D_ColliderGetRestitution(object), 0.2, 1e-5);
      TEST_NEAR(BLB_Physics3D_ColliderGetDensity(object), 2, 1e-5);
      TEST_CHECK(BLB_Physics3D_ColliderIsTrigger(object));
      BLB_Physics3D_ColliderSetTrigger(object, false);
      BLB_Physics3D_ColliderSetLayer(object, 3);
      BLB_Physics3D_ColliderSetMask(object, 1ull << 4);
      TEST_CHECK(BLB_Physics3D_ColliderGetFilter(object).layer == 3);
      BLB_Physics3D_WorldSetDebugDraw(world, true);
      TEST_CHECK(BLB_Physics3D_WorldGetDebugDraw(world));
      TEST_CHECK(BLB_Physics3D_WorldGetDebugColliderCount(world) == 1);
    }

    BLB_Physics3D_BodySetPosition(object, BLB_V3(2, 5, 0));
    TEST_NEAR(BLB_Physics3D_BodyGetPosition(object).y, 5, 1e-5);
    BLB_Physics3D_BodySetVelocity(object, BLB_V3(0, 0, 0));
    BLB_Physics3D_WorldSetGravity(world, BLB_V3(0, -10, 0));
    BLB_Physics3D_WorldSetFixedTimestep(world, 1.0f / 60.0f);
    BLB_Physics3D_WorldSetSubsteps(world, 4);
    BLB_DeltaTime_Set(1.0f / 60.0f);

    for (int i = 0; i < 30; ++i)
      BLB_Physics3D_WorldUpdate(world);

    TEST_CHECK(BLB_Physics3D_BodyGetPosition(object).y < 4.5f);
    TEST_CHECK(BLB_Physics3D_BodyGetVelocity(object).y < 0);
    TEST_CHECK(world->interpolation >= 0 && world->interpolation <= 1);
    BLB_Physics3D_BodySetActive(object, false);
    TEST_CHECK(!BLB_Physics3D_BodyIsActive(object));
    BLB_Physics3D_BodySetActive(object, true);
    BLB_Physics3D_BodySetType(object, BLB_PHYSICS3D_KINEMATIC);
    TEST_CHECK(BLB_Physics3D_BodyGetType(object) == BLB_PHYSICS3D_KINEMATIC);
    BLB_Physics3D_BodyDestroy(world, object);
    TEST_CHECK(world->body_count == 0 && world->collider_count == 0);
    Test_Destroy3D(object);
  }

  BLB_Physics3D_WorldDestroy(world);
}

static void invalid_timestep_is_rejected(void) {
  BLB_Physics3DWorld *world = BLB_Physics3D_WorldCreate();

  TEST_REQUIRE(world);
  float before = world->fixed_timestep;

  BLB_Physics3D_WorldSetFixedTimestep(world, -1);
  BLB_Physics3D_WorldSetFixedTimestep(world, NAN);
  TEST_NEAR(world->fixed_timestep, before, 1e-8);
  BLB_Physics3D_WorldSetSubsteps(world, 0);
  TEST_CHECK(world->substeps >= 1);
  BLB_Physics3D_WorldDestroy(world);
  BLB_Physics3D_WorldDestroy(NULL);
}

static void falling_box_collides_with_floor(void) {
  BLB_Physics3DWorld *world = BLB_Physics3D_WorldCreate();

  TEST_REQUIRE(world);
  BLB_Object3D *box = BLB_CreateCube3D(BLB_V3(1, 1, 1), BLB_V3(0, 3, 0), NULL, BLB_TEXMAP_UV);
  BLB_Object3D *floor = BLB_CreateCube3D(BLB_V3(20, 1, 20), BLB_V3(0, -1, 0), NULL, BLB_TEXMAP_UV);

  TEST_CHECK(box && floor);
  if (box && floor) {
    BLB_Physics3D_SetBody(world, box, BLB_PHYSICS3D_DYNAMIC);
    BLB_Physics3D_SetBody(world, floor, BLB_PHYSICS3D_STATIC);
    BLB_Physics3DShape *a = BLB_Physics3D_CreateShape(BLB_PHYSICS3D_SHAPE_BOX, box->scale);
    BLB_Physics3DShape *b = BLB_Physics3D_CreateShape(BLB_PHYSICS3D_SHAPE_BOX, floor->scale);

    TEST_CHECK(a && b);
    if (a && b) {
      TEST_CHECK(BLB_Physics3D_SetCollider(box, a));
      TEST_CHECK(BLB_Physics3D_SetCollider(floor, b));
      BLB_Physics3D_WorldSetGravity(world, BLB_V3(0, -10, 0));
      BLB_DeltaTime_Set(1.0f / 60.0f);

      for (int i = 0; i < 240; ++i)
        BLB_Physics3D_WorldUpdate(world);
      TEST_NEAR(BLB_Physics3D_BodyGetPosition(box).y, 0, 0.15);
      TEST_NEAR(BLB_Physics3D_BodyGetVelocity(box).y, 0, 0.15);
    }
    BLB_Physics3D_DestroyShape(a);
    BLB_Physics3D_DestroyShape(b);
  }
  Test_Destroy3D(box);
  Test_Destroy3D(floor);
  BLB_Physics3D_WorldDestroy(world);
}

static const Test_Case cases[] = {TEST_CASE(falling_box_collides_with_floor), TEST_CASE(body_and_collider_contract),
                                  TEST_CASE(invalid_timestep_is_rejected)};
const Test_Suite test_physics3d = {"physics3d", cases, sizeof(cases) / sizeof(cases[0])};
