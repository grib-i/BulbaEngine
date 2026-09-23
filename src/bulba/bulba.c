#include "bulba/bulba.h"

static void cleanup(void) { free(BLB_OBJECTS_ID); }

void BLB_Init(void) {
  BLB_Object_Init();
  atexit(cleanup);
}
