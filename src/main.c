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
  //
  // result = BLB_TestSpriteSheet();

  result = BLB_TestObjectCount(100, BLB_TEST_OBJECT_CUBE);

  // result = BLB_TestPhysicsObjectCount(100, BLB_TEST_OBJECT_CUBE);
  // result = BLB_Test_ph3d();
  // result = BLB_Test_ph2d();

  // result = BLB_AnimationTest();

  if (result != 0) {
    return 1;
  }

  return 0;
}
