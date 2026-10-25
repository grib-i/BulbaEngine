#include "physics_2d.h"

static void attach(Demo_Context *demo, BLB_Object2D *object) {
  BLB_Physics2DShape *shape = BLB_Physics2D_CreateShapeFromObject2D(object);

  if (!shape || !BLB_Physics2D_SetCollider(object, shape))
    demo->failed = true;
  BLB_Physics2D_DestroyShape(shape);
}

static void update_physics(Demo_Context *demo) {
  BLB_Physics2D_WorldUpdate(demo->scene->physics_world2d);
}

int Demo_Physics2d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "2D physics: falling boxes and static ground", settings))
    return Demo_Finish(&demo, NULL);

  BLB_Physics2D_WorldSetGravity(demo.scene->physics_world2d, BLB_V2(0, 350));
  BLB_Object2D *floor = Demo_Square(&demo, 640, 650, 1100, 30);

  if (floor) {
    BLB_Physics2D_SetBody(demo.scene->physics_world2d, floor, BLB_PHYSICS2D_STATIC);
    attach(&demo, floor);
  }

  for (int i = 0; i < (int)Demo_Count(&demo, 40); ++i) {
    BLB_Object2D *object = Demo_Square(&demo, 200 + (i % 10) * 90.0f, 170 + (i / 10) * 80.0f, 42, 42);

    if (!object)
      break;

    BLB_Object2D_SetColor(object, BLB_V4(80 + i * 3.0f, 190, 240, 255));
    BLB_Physics2D_SetBody(demo.scene->physics_world2d, object, BLB_PHYSICS2D_DYNAMIC);
    attach(&demo, object);
  }
  return Demo_Finish(&demo, update_physics);
}
