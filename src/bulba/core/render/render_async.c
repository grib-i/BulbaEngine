#include "bulba/core/render/render_async.h"

#include <stdbool.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
typedef HANDLE BLB_AsyncThread;
typedef CRITICAL_SECTION BLB_AsyncMutex;
typedef CONDITION_VARIABLE BLB_AsyncCondition;
#else
#include <pthread.h>
#include <sched.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef pthread_t BLB_AsyncThread;
typedef pthread_mutex_t BLB_AsyncMutex;
typedef pthread_cond_t BLB_AsyncCondition;
#endif

typedef struct BLB_RenderAsyncJob {
  BLB_RenderAsyncRun run;
  BLB_RenderAsyncComplete complete;
  BLB_RenderAsyncDestroy destroy;
  void *user_data;
  struct BLB_RenderAsyncJob *next;
} BLB_RenderAsyncJob;

#define BLB_RENDER_ASYNC_MAX_PENDING 64u

typedef struct {
  BLB_AsyncThread *threads;
  size_t thread_count;
  BLB_RenderAsyncJob *queue_head;
  BLB_RenderAsyncJob *queue_tail;
  BLB_RenderAsyncJob *complete_head;
  BLB_RenderAsyncJob *complete_tail;
  BLB_AsyncMutex mutex;
  BLB_AsyncCondition cond;
  bool initialized;
  bool stopping;
  size_t pending;
} BLB_RenderAsyncState;

static BLB_RenderAsyncState state;

static int async_mutex_init(BLB_AsyncMutex *mutex) {
#ifdef _WIN32
  InitializeCriticalSection(mutex);
  return 0;
#else
  return pthread_mutex_init(mutex, NULL);
#endif
}

static void async_mutex_destroy(BLB_AsyncMutex *mutex) {
#ifdef _WIN32
  DeleteCriticalSection(mutex);
#else
  pthread_mutex_destroy(mutex);
#endif
}

static void async_mutex_lock(BLB_AsyncMutex *mutex) {
#ifdef _WIN32
  EnterCriticalSection(mutex);
#else
  pthread_mutex_lock(mutex);
#endif
}

static void async_mutex_unlock(BLB_AsyncMutex *mutex) {
#ifdef _WIN32
  LeaveCriticalSection(mutex);
#else
  pthread_mutex_unlock(mutex);
#endif
}

static int async_cond_init(BLB_AsyncCondition *cond) {
#ifdef _WIN32
  InitializeConditionVariable(cond);
  return 0;
#else
  return pthread_cond_init(cond, NULL);
#endif
}

static void async_cond_destroy(BLB_AsyncCondition *cond) {
#ifdef _WIN32
  (void)cond;
#else
  pthread_cond_destroy(cond);
#endif
}

static void async_cond_wait(BLB_AsyncCondition *cond, BLB_AsyncMutex *mutex) {
#ifdef _WIN32
  (void)SleepConditionVariableCS(cond, mutex, INFINITE);
#else
  (void)pthread_cond_wait(cond, mutex);
#endif
}

static void async_cond_signal(BLB_AsyncCondition *cond) {
#ifdef _WIN32
  WakeConditionVariable(cond);
#else
  pthread_cond_signal(cond);
#endif
}

static void async_cond_broadcast(BLB_AsyncCondition *cond) {
#ifdef _WIN32
  WakeAllConditionVariable(cond);
#else
  pthread_cond_broadcast(cond);
#endif
}

#ifdef _WIN32
static DWORD WINAPI worker_main(LPVOID unused) {
#else
static void *worker_main(void *unused) {
#endif
  (void)unused;

  for (;;) {
    async_mutex_lock(&state.mutex);

    while (!state.stopping && !state.queue_head)
      async_cond_wait(&state.cond, &state.mutex);

    if (state.stopping && !state.queue_head) {
      async_mutex_unlock(&state.mutex);
      break;
    }

    BLB_RenderAsyncJob *job = state.queue_head;
    state.queue_head = job->next;
    if (!state.queue_head)
      state.queue_tail = NULL;
    job->next = NULL;

    async_mutex_unlock(&state.mutex);

    if (job->run)
      job->run(job->user_data);

    async_mutex_lock(&state.mutex);
    if (state.pending > 0)
      --state.pending;

    if (job->complete) {
      if (state.complete_tail)
        state.complete_tail->next = job;
      else
        state.complete_head = job;
      state.complete_tail = job;
    } else {
      BLB_RenderAsyncDestroy destroy = job->destroy;
      void *user_data = job->user_data;
      async_mutex_unlock(&state.mutex);
      if (destroy)
        destroy(user_data);
      free(job);
      continue;
    }

    async_mutex_unlock(&state.mutex);
  }

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

static size_t default_worker_count(void) {
#ifdef _WIN32
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  size_t count = (size_t)system_info.dwNumberOfProcessors;
#else
  long online_processors = sysconf(_SC_NPROCESSORS_ONLN);
  size_t count = online_processors > 0 ? (size_t)online_processors : 1u;
#endif
  if (count <= 2u)
    return 1u;
  size_t workers = count - 2u;
  if (workers > 4u)
    workers = 4u;
  return workers;
}

int BLB_RenderAsync_Init(size_t worker_count) {
  if (state.initialized)
    return 0;

  if (worker_count == 0)
    worker_count = default_worker_count();
  if (worker_count == 0)
    worker_count = 1;

  state.threads = calloc(worker_count, sizeof(*state.threads));
  if (!state.threads)
    return -1;

  if (async_mutex_init(&state.mutex) != 0) {
    free(state.threads);
    state.threads = NULL;
    return -1;
  }

  if (async_cond_init(&state.cond) != 0) {
    async_mutex_destroy(&state.mutex);
    free(state.threads);
    state.threads = NULL;
    return -1;
  }

  state.thread_count = worker_count;
  state.initialized = true;
  state.stopping = false;

  size_t started = 0;
  for (; started < worker_count; ++started) {
#ifdef _WIN32
    state.threads[started] = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
    if (!state.threads[started])
      break;
#else
    if (pthread_create(&state.threads[started], NULL, worker_main, NULL) != 0)
      break;
#endif
  }

  if (started == 0) {
    state.stopping = true;
    async_cond_broadcast(&state.cond);
    async_cond_destroy(&state.cond);
    async_mutex_destroy(&state.mutex);
    free(state.threads);
    state.threads = NULL;
    state.thread_count = 0;
    state.initialized = false;
    return -1;
  }

  state.thread_count = started;
  return 0;
}

void BLB_RenderAsync_Shutdown(void) {
  if (!state.initialized)
    return;

  async_mutex_lock(&state.mutex);
  state.stopping = true;
  async_cond_broadcast(&state.cond);
  async_mutex_unlock(&state.mutex);

  for (size_t i = 0; i < state.thread_count; ++i) {
#ifdef _WIN32
    (void)WaitForSingleObject(state.threads[i], INFINITE);
    CloseHandle(state.threads[i]);
#else
    pthread_join(state.threads[i], NULL);
#endif
  }

  BLB_RenderAsyncJob *job = state.queue_head;
  while (job) {
    BLB_RenderAsyncJob *next = job->next;
    if (job->destroy)
      job->destroy(job->user_data);
    free(job);
    job = next;
  }

  job = state.complete_head;
  while (job) {
    BLB_RenderAsyncJob *next = job->next;
    if (job->destroy)
      job->destroy(job->user_data);
    free(job);
    job = next;
  }

  state.queue_head = NULL;
  state.queue_tail = NULL;
  state.complete_head = NULL;
  state.complete_tail = NULL;
  state.pending = 0;
  state.stopping = false;
  state.initialized = false;

  async_cond_destroy(&state.cond);
  async_mutex_destroy(&state.mutex);
  free(state.threads);
  state.threads = NULL;
  state.thread_count = 0;
}

int BLB_RenderAsync_Submit(BLB_RenderAsyncRun run, BLB_RenderAsyncComplete complete, BLB_RenderAsyncDestroy destroy, void *user_data) {
  if (!run || !state.initialized)
    return -1;

  BLB_RenderAsyncJob *job = calloc(1, sizeof(*job));
  if (!job)
    return -1;

  job->run = run;
  job->complete = complete;
  job->destroy = destroy;
  job->user_data = user_data;

  async_mutex_lock(&state.mutex);
  if (state.stopping || state.pending >= BLB_RENDER_ASYNC_MAX_PENDING) {
    async_mutex_unlock(&state.mutex);
    free(job);
    return -1;
  }

  if (state.queue_tail)
    state.queue_tail->next = job;
  else
    state.queue_head = job;
  state.queue_tail = job;
  ++state.pending;
  async_cond_signal(&state.cond);
  async_mutex_unlock(&state.mutex);

  return 0;
}

size_t BLB_RenderAsync_Poll(void) {
  if (!state.initialized)
    return 0;

  size_t count = 0;
  for (;;) {
    async_mutex_lock(&state.mutex);
    BLB_RenderAsyncJob *job = state.complete_head;
    if (job) {
      state.complete_head = job->next;
      if (!state.complete_head)
        state.complete_tail = NULL;
      job->next = NULL;
    }
    async_mutex_unlock(&state.mutex);

    if (!job)
      break;

    if (job->complete)
      job->complete(job->user_data);
    if (job->destroy)
      job->destroy(job->user_data);
    free(job);
    ++count;
  }

  return count;
}

size_t BLB_RenderAsync_GetPending(void) {
  if (!state.initialized)
    return 0;

  async_mutex_lock(&state.mutex);
  size_t pending = state.pending;
  async_mutex_unlock(&state.mutex);
  return pending;
}

void BLB_RenderAsync_WaitAll(void) {
  if (!state.initialized)
    return;

  for (;;) {
    BLB_RenderAsync_Poll();
    if (BLB_RenderAsync_GetPending() == 0) {
      BLB_RenderAsync_Poll();
      if (BLB_RenderAsync_GetPending() == 0)
        break;
    }
#ifdef _WIN32
    (void)SwitchToThread();
#else
    sched_yield();
#endif
  }
}
