#include "bulba/bulba.h"
#include "bulba/core/render/optimization/render_async.h"

#include <stdlib.h>

static bool blb_initialized;

static void cleanup(void) {
  BLB_RenderAsync_Shutdown();

  free(BLB_OBJECTS_ID);
  BLB_OBJECTS_ID = NULL;
  BLB_OBJECTS_ID_COUNT = 0;
}

void BLB_Init(void) {
  /* Initialization registers process-wide state and an atexit handler. */
  if (blb_initialized)
    return;

  BLB_Object_Init();
  if (!BLB_OBJECTS_ID)
    return; /* Leave initialization retryable if the registry allocation fails. */

  BLB_InitConfig();
  BLB_RenderAsync_Init(BLB_RENDER_ASYNC_WORKERS > 0 ? (size_t)BLB_RENDER_ASYNC_WORKERS : 0);

  blb_initialized = true;
  (void)atexit(cleanup);
}
