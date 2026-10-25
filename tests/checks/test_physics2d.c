#include "test.h"

static void body_and_collider_contract(void) {
  BLB_Physics2DWorld *world = BLB_Physics2D_WorldCreate();

  TEST_REQUIRE(world);
  BLB_Object2D *object = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 0), NULL, false);

  TEST_CHECK(object);

  if (object) {
    BLB_Physics2D_SetBody(world, object, BLB_PHYSICS2D_DYNAMIC);
    TEST_CHECK(BLB_Physics2D_GetBody(object));
    TEST_CHECK(BLB_Physics2D_BodyGetId(object) != 0);
    TEST_CHECK(BLB_Physics2D_BodyGetType(object) == BLB_PHYSICS2D_DYNAMIC);
    BLB_Physics2DShape *shape = BLB_Physics2D_CreateShape(BLB_PHYSICS2D_SHAPE_BOX, BLB_V2(1, 1));

    TEST_CHECK(shape);
    if (shape) {
      TEST_CHECK(BLB_Physics2D_SetCollider(object, shape));
      BLB_Physics2D_DestroyShape(shape);
      TEST_CHECK(BLB_Physics2D_ColliderGetId(object) != 0);
      BLB_Physics2D_ColliderSetFriction(object, 0.7f);
      BLB_Physics2D_ColliderSetRestitution(object, 0.2f);
      BLB_Physics2D_ColliderSetDensity(object, 2.0f);
      BLB_Physics2D_ColliderSetTrigger(object, true);
      TEST_NEAR(BLB_Physics2D_ColliderGetFriction(object), 0.7, 1e-5);
      TEST_NEAR(BLB_Physics2D_ColliderGetRestitution(object), 0.2, 1e-5);
      TEST_NEAR(BLB_Physics2D_ColliderGetDensity(object), 2, 1e-5);
      TEST_CHECK(BLB_Physics2D_ColliderIsTrigger(object));
      BLB_Physics2D_ColliderSetTrigger(object, false);
      BLB_Physics2D_ColliderSetLayer(object, 3);
      BLB_Physics2D_ColliderSetMask(object, 1ull << 4);
      TEST_CHECK(BLB_Physics2D_ColliderGetFilter(object).layer == 3);
      BLB_Physics2D_WorldSetDebugDraw(world, true);
      TEST_CHECK(BLB_Physics2D_WorldGetDebugDraw(world));
      TEST_CHECK(BLB_Physics2D_WorldGetDebugColliderCount(world) == 1);
    }

    BLB_Physics2D_BodySetPosition(object, BLB_V2(2, 5));
    TEST_NEAR(BLB_Physics2D_BodyGetPosition(object).y, 5, 1e-5);
    BLB_Physics2D_BodySetVelocity(object, BLB_V2(0, 0));
    BLB_Physics2D_WorldSetGravity(world, BLB_V2(0, -10));
    BLB_Physics2D_WorldSetFixedTimestep(world, 1.0f / 60.0f);
    BLB_Physics2D_WorldSetSubsteps(world, 4);
    BLB_DeltaTime_Set(1.0f / 60.0f);

    for (int i = 0; i < 30; ++i)
      BLB_Physics2D_WorldUpdate(world);

    TEST_CHECK(BLB_Physics2D_BodyGetPosition(object).y < 4.5f);
    TEST_CHECK(BLB_Physics2D_BodyGetVelocity(object).y < 0);
    TEST_CHECK(world->interpolation >= 0 && world->interpolation <= 1);
    BLB_Physics2D_BodySetActive(object, false);
    TEST_CHECK(!BLB_Physics2D_BodyIsActive(object));
    BLB_Physics2D_BodySetActive(object, true);
    BLB_Physics2D_BodySetType(object, BLB_PHYSICS2D_KINEMATIC);
    TEST_CHECK(BLB_Physics2D_BodyGetType(object) == BLB_PHYSICS2D_KINEMATIC);
    BLB_Physics2D_BodyDestroy(world, object);
    TEST_CHECK(world->body_count == 0 && world->collider_count == 0);
    Test_Destroy2D(object);
  }

  BLB_Physics2D_WorldDestroy(world);
}

static void invalid_timestep_is_rejected(void) {
  BLB_Physics2DWorld *world = BLB_Physics2D_WorldCreate();

  TEST_REQUIRE(world);
  float before = world->fixed_timestep;

  BLB_Physics2D_WorldSetFixedTimestep(world, -1);
  BLB_Physics2D_WorldSetFixedTimestep(world, NAN);
  TEST_NEAR(world->fixed_timestep, before, 1e-8);
  BLB_Physics2D_WorldSetSubsteps(world, 0);
  TEST_CHECK(world->substeps >= 1);
  BLB_Physics2D_WorldDestroy(world);
  BLB_Physics2D_WorldDestroy(NULL);
}

static void falling_box_collides_with_floor(void) {
  BLB_Physics2DWorld *world = BLB_Physics2D_WorldCreate();

  TEST_REQUIRE(world);
  BLB_Object2D *box = BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(0, 3), NULL, false);
  BLB_Object2D *floor = BLB_CreateSquare2D(BLB_V2(20, 1), BLB_V2(0, -1), NULL, false);

  TEST_CHECK(box && floor);
  if (box && floor) {
    BLB_Physics2D_SetBody(world, box, BLB_PHYSICS2D_DYNAMIC);
    BLB_Physics2D_SetBody(world, floor, BLB_PHYSICS2D_STATIC);
    BLB_Physics2DShape *a = BLB_Physics2D_CreateShape(BLB_PHYSICS2D_SHAPE_BOX, box->scale);
    BLB_Physics2DShape *b = BLB_Physics2D_CreateShape(BLB_PHYSICS2D_SHAPE_BOX, floor->scale);

    TEST_CHECK(a && b);
    if (a && b) {
      TEST_CHECK(BLB_Physics2D_SetCollider(box, a));
      TEST_CHECK(BLB_Physics2D_SetCollider(floor, b));
      BLB_Physics2D_WorldSetGravity(world, BLB_V2(0, -10));
      BLB_DeltaTime_Set(1.0f / 60.0f);

      for (int i = 0; i < 240; ++i)
        BLB_Physics2D_WorldUpdate(world);
      TEST_NEAR(BLB_Physics2D_BodyGetPosition(box).y, 0, 0.15);
      TEST_NEAR(BLB_Physics2D_BodyGetVelocity(box).y, 0, 0.15);
    }
    BLB_Physics2D_DestroyShape(a);
    BLB_Physics2D_DestroyShape(b);
  }
  Test_Destroy2D(box);
  Test_Destroy2D(floor);
  BLB_Physics2D_WorldDestroy(world);
}

static const Test_Case cases[] = {TEST_CASE(falling_box_collides_with_floor), TEST_CASE(body_and_collider_contract),
                                  TEST_CASE(invalid_timestep_is_rejected)};
const Test_Suite test_physics2d = {"physics2d", cases, sizeof(cases) / sizeof(cases[0])};
