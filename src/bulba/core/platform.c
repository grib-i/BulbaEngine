#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "bulba/core/platform.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <profileapi.h>
#else
#include <dlfcn.h>
#include <time.h>
#endif

double BLB_Platform_TimeSeconds(void) {
#ifdef _WIN32
  static LARGE_INTEGER frequency;
  static int initialized;
  LARGE_INTEGER counter;
  if (!initialized) {
    QueryPerformanceFrequency(&frequency);
    initialized = 1;
  }
  QueryPerformanceCounter(&counter);
  return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

char *BLB_Platform_DuplicateString(const char *value) {
  if (!value)
    return NULL;
  size_t size = strlen(value) + 1;
  char *copy = malloc(size);
  if (!copy)
    return NULL;
  memcpy(copy, value, size);
  return copy;
}

BLB_PlatformLibrary BLB_Platform_LoadLibrary(const char *path) {
  if (!path)
    return NULL;
#ifdef _WIN32
  return LoadLibraryA(path);
#else
  return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

void *BLB_Platform_LoadSymbol(BLB_PlatformLibrary library, const char *name) {
  if (!library || !name)
    return NULL;
#ifdef _WIN32
  return (void *)GetProcAddress(library, name);
#else
  return dlsym(library, name);
#endif
}

void BLB_Platform_UnloadLibrary(BLB_PlatformLibrary library) {
  if (!library)
    return;
#ifdef _WIN32
  FreeLibrary(library);
#else
  dlclose(library);
#endif
}

const char *BLB_Platform_LibraryError(void) {
#ifdef _WIN32
  return "Windows dynamic library error";
#else
  return dlerror();
#endif
}
