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

  int result = 0;

#ifdef DEBUG
  // result = BLB_TestMaterial();
  // if (result != 0) {
  //   fprintf(stderr, "BLB_TestMaterial failed: %d\n", result);
  //   return 1;
  // }

  // result = BLB_TestObjects();
  // if (result != 0) {
  //   fprintf(stderr, "BLB_TestMaterial failed: %d\n", result);
  //   return 1;
  // }

  result = BLB_TestSuperNova();
  if (result != 0) {
    fprintf(stderr, "BLB_TestMaterial failed: %d\n", result);
    return 1;
  }

  // result = BLB_TestTexture();
  // if (result != 0) {
  //   fprintf(stderr, "BLB_TestTexture failed: %d\n", result);
  //   return 1;
  // }

  // result = BLB_TestSpriteSheet();
  // if (result != 0) {
  //   fprintf(stderr, "BLB_TestSpriteSheet failed: %d\n", result);
  //   return 1;
  // }

  // result = BLB_TestObjectCount(1000, BLB_TEST_OBJECT_CUBE);
  // if (result != 0) {
  //   fprintf(stderr, "BLB_TestObjectCount failed: %d\n", result);
  //   return 1;
  // }
#endif

  return 0;
}
