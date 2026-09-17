#pragma once

#ifdef NDEBUG

#define printd(...) ((void)0)
#define wprintd(...) ((void)0)
#define printp(...) ((void)0)

#else

#include <stdio.h>
#include <wchar.h>

#define printd(...) fprintf(stderr, __VA_ARGS__)
#define wprintd(...) fwprintf(stderr, __VA_ARGS__)
#define printp(...) fputs(__VA_ARGS__, stderr)

#endif
