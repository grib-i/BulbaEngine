#ifndef BULBA_CORE_PLATFORM_H
#define BULBA_CORE_PLATFORM_H

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
typedef HMODULE BLB_PlatformLibrary;
#else
typedef void *BLB_PlatformLibrary;
#endif

double BLB_Platform_TimeSeconds(void);
char *BLB_Platform_DuplicateString(const char *value);
BLB_PlatformLibrary BLB_Platform_LoadLibrary(const char *path);
void *BLB_Platform_LoadSymbol(BLB_PlatformLibrary library, const char *name);
void BLB_Platform_UnloadLibrary(BLB_PlatformLibrary library);
const char *BLB_Platform_LibraryError(void);

#if defined(BLB_PLATFORM_ANDROID)
#define BLB_PLATFORM_MOBILE 1
#endif

#endif
