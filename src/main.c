#include "bulba/core/utils/config.h"
#include "tests.h"
#include <bulba/bulba.h>
#include <debug.h>

#ifdef DEBUG
#include <tests.h>
#endif

#include <stdio.h>

int main(void) {
  BLB_DEBUG = false;

  DEBUG_InitStdIO();
  BLB_Init();
  int result;

  // int result = 0;
  // result = BLB_TestMaterial();

  // result = BLB_TestObjects();

  // BLB_TestSuperNova();

  // result = BLB_TestTexture();

  // result = BLB_TestSpriteSheet();
  // result = BLB_AnimationTest();

  // BLB_OBJECT_AGGREGATION = true;
  // BLB_OBJECT_INSTANCING = true;
  // BLB_HSA_LEVELS = 1;

  // result = BLB_TestObjectCount(10000, BLB_TEST_OBJECT_CUBE, false);

  // result = BLB_TestObjectCount(1488, BLB_TEST_OBJECT_TEAPOT, false);
  // result = BLB_TestObjectCount2D(16384, BLB_TEST_OBJECT2D_SQUARE, false);
  // result = BLB_TestObjectCount2D(10000, BLB_TEST_OBJECT2D_CIRCLE, false);

  // result = BLB_TestPhysicsObjectCount(1000, BLB_TEST_OBJECT_CUBE);
  // result = BLB_TestPhysicsObjectCount2D(300, BLB_TEST_OBJECT2D_SQUARE);
  // result = BLB_TestPhysicsObjectCount2D(100, BLB_TEST_OBJECT2D_CIRCLE);
  // result = BLB_Test_ph3d();
  // result = BLB_Test_ph2d();
  // result = BLB_TestParticles3D();
  // result = BLB_TestParticles2D();

  result = BLB_TestHandlers();

  // result = BLB_TestSound();

  if (result != 0) {
    return 1;
  }

  return 0;
}

// #include "bulba/core/utils/config.h"
// #include "tests.h"
// #include <bulba/bulba.h>
// #include <debug.h>

// #ifdef DEBUG
// #include <tests.h>
// #endif

// #include <stdio.h>

// int main(void) {
//   BLB_DEBUG = false;

//   BLB_OBJECT_AGGREGATION = true;
//   BLB_OBJECT_INSTANCING = true;
//   BLB_OBJECT_SMART_OPTIMIZATION = true;
//   BLB_HSA_LEVELS = 1;

//   DEBUG_InitStdIO();
//   BLB_Init();
//   int result;

//   // input for test
//   int count, type, mode, r;
//   printf("Count objects (1 - n): ");
//   scanf("%d", &count);

//   printf("\nMode (TestObjectCount - 0,  TestPhysicsObjectCount - 1): ");
//   scanf("%d", &mode);

//   printf("\nCount objects (cube - 0, sphere - 1, torus - 2, teapot - 3): ");
//   scanf("%d", &type);

//   printf("rotarting? (0 - false, 1 - true): ");
//   scanf("%d", &r);

//   if (mode == 0) {
//     result = BLB_TestObjectCount(count, type, r);
//   } else if (mode == 1) {
//     result = BLB_TestPhysicsObjectCount(100, BLB_TEST_OBJECT_CUBE);
//   } else {
//     printf("ERROR");
//   }

//   if (result != 0) {
//     return 1;
//   }

//   return 0;
// }
