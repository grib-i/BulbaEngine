#include "input.h"
#include "bulba/core/math3v/ray.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/scene.h"

static void animate(Demo_Context *demo) {
  if (!demo->scene->object2d_count)
    return;

  BLB_Object2D *object1 =
      BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(3.5f, 3.5f), NULL, false);

  BLB_Object2D *object2 =
      BLB_CreateSquare2D(BLB_V2(1, 1), BLB_V2(-3.5f, -3.5f), NULL, false);

  BLB_AddObject2D(demo->scene, object1);
  BLB_AddObject2D(demo->scene, object2);

  BLB_Object2D *object = demo->scene->objects2d[0];
  BLB_Handlers *handlers = &demo->window->handlers;

  float dx = (BLB_KeyboardDown(handlers, 'D') ||
              BLB_KeyboardDown(handlers, BLB_KEY_RIGHT)) -
             (BLB_KeyboardDown(handlers, 'A') ||
              BLB_KeyboardDown(handlers, BLB_KEY_LEFT));

  float dy = (BLB_KeyboardDown(handlers, 'S') ||
              BLB_KeyboardDown(handlers, BLB_KEY_DOWN)) -
             (BLB_KeyboardDown(handlers, 'W') ||
              BLB_KeyboardDown(handlers, BLB_KEY_UP));

  BLB_Object2D_SetPosition(
      object, BLB_AddV2(object->position,
                        BLB_V2(dx * demo->dt * 300, dy * demo->dt * 300)));

  if (BLB_MouseDown(handlers, 0)) {
    BLB_Object2D_SetColor(object, BLB_ColorRGBA(255, 120, 70, 255));

    BLB_RayHit2D **ray2d = BLB_Ray2D(demo->scene, BLB_V2(-2, -2), BLB_V2(2, 2),
                                     BLB_RAY_ALL_HITS, NULL, NULL);

    if (ray2d)
      printf("%d\n", ray2d[0]->hit);
    else
      printf("%d\n", false);

  } else {
    BLB_Object2D_SetColor(object, BLB_ColorRGBA(80, 200, 240, 255));
  }
}

int Demo_Input(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "Keyboard and mouse: WASD moves the square", settings))
    return Demo_Finish(&demo, NULL);

  Demo_Square(&demo, 640, 360, 80, 80);
  Demo_Text(&demo, "WASD / arrow keys: move | left mouse: change color", 180,
            650, 22);
  return Demo_Finish(&demo, animate);
}
