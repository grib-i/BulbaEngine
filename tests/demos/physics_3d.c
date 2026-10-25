#include "physics_3d.h"

static void attach(Demo_Context *demo, BLB_Object3D *object) {
  BLB_Physics3DShape *shape = BLB_Physics3D_CreateShape(BLB_PHYSICS3D_SHAPE_BOX, object->scale);

  if (!shape || !BLB_Physics3D_SetCollider(object, shape))
    demo->failed = true;
  BLB_Physics3D_DestroyShape(shape);
}

static void update_physics(Demo_Context *demo) {
  BLB_Physics3D_WorldUpdate(demo->scene->physics_world3d);
}

int Demo_Physics3d(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "3D physics: falling boxes and static ground", settings))
    return Demo_Finish(&demo, NULL);

  BLB_Physics3D_WorldSetGravity(demo.scene->physics_world3d, BLB_V3(0, -9.81f, 0));
  BLB_Object3D *floor = Demo_Cube(&demo, 0, -2, 0, 1);

  if (floor) {
    BLB_Object3D_SetScale(floor, BLB_V3(12, 0.5f, 8));
    BLB_Physics3D_SetBody(demo.scene->physics_world3d, floor, BLB_PHYSICS3D_STATIC);
    attach(&demo, floor);
  }

  for (int i = 0; i < (int)Demo_Count(&demo, 24); ++i) {
    BLB_Object3D *object = Demo_Cube(&demo, (i % 4 - 1.5f) * 1.1f, (i / 4) * 0.9f, 0, 0.65f);

    if (!object)
      break;

    BLB_Physics3D_SetBody(demo.scene->physics_world3d, object, BLB_PHYSICS3D_DYNAMIC);
    attach(&demo, object);
  }
  Demo_Light(&demo);
  return Demo_Finish(&demo, update_physics);
}
