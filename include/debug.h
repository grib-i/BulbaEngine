#pragma once

#include <stdio.h>
#include <wchar.h>

static inline void DEBUG_InitStdIO(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);
}

#ifdef NDEBUG

#define printd(...) ((void)0)
#define wprintd(...) ((void)0)
#define printp(...) ((void)0)

#else

#define printd(...) fprintf(stderr, __VA_ARGS__)
#define wprintd(...) fwprintf(stderr, __VA_ARGS__)
#define printp(...) fputs(__VA_ARGS__, stderr)

#endif
