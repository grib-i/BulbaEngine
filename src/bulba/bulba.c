#include "bulba/bulba.h"
#include "bulba/core/render/optimization/render_async.h"

static void cleanup(void) {
  BLB_RenderAsync_Shutdown();
  free(BLB_OBJECTS_ID);
}

void BLB_Init(void) {
  BLB_Object_Init();
  BLB_InitConfig();
  BLB_RenderAsync_Init(BLB_RENDER_ASYNC_WORKERS > 0 ? (size_t)BLB_RENDER_ASYNC_WORKERS : 0);
  atexit(cleanup);
}
