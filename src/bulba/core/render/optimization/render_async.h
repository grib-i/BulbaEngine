#ifndef BLB_RENDER_ASYNC_H
#define BLB_RENDER_ASYNC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*BLB_RenderAsyncRun)(void *user_data);
typedef void (*BLB_RenderAsyncComplete)(void *user_data);
typedef void (*BLB_RenderAsyncDestroy)(void *user_data);

int BLB_RenderAsync_Init(size_t worker_count);
void BLB_RenderAsync_Shutdown(void);
int BLB_RenderAsync_Submit(BLB_RenderAsyncRun run, BLB_RenderAsyncComplete complete, BLB_RenderAsyncDestroy destroy, void *user_data);
size_t BLB_RenderAsync_Poll(void);
size_t BLB_RenderAsync_GetPending(void);
void BLB_RenderAsync_WaitAll(void);

#ifdef __cplusplus
}
#endif

#endif
