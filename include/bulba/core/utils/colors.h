#include "bulba/core/math3v/vec_math.h"

/*
 * Vector literals.
 *
 * C supports compound literals starting with C99.
 * C++ uses aggregate initialization.
 */
#ifdef __cplusplus
#define BLB_DETAIL_VEC4(r, g, b, a)                                                                                                                  \
  BLB_Vec4 {                                                                                                                                         \
    { (r), (g), (b), (a) }                                                                                                                           \
  }
#else
#define BLB_DETAIL_VEC4(r, g, b, a) ((BLB_Vec4){.Elements = {(r), (g), (b), (a)}})
#endif

/* Colors: normalized RGBA */
#define BLB_COLOR_BLACK BLB_DETAIL_VEC4(0.0f, 0.0f, 0.0f, 1.0f)
#define BLB_COLOR_WHITE BLB_DETAIL_VEC4(1.0f, 1.0f, 1.0f, 1.0f)

#define BLB_COLOR_RED BLB_DETAIL_VEC4(1.0f, 0.0f, 0.0f, 1.0f)
#define BLB_COLOR_GREEN BLB_DETAIL_VEC4(0.0f, 1.0f, 0.0f, 1.0f)
#define BLB_COLOR_BLUE BLB_DETAIL_VEC4(0.0f, 0.0f, 1.0f, 1.0f)

#define BLB_COLOR_YELLOW BLB_DETAIL_VEC4(1.0f, 1.0f, 0.0f, 1.0f)
#define BLB_COLOR_CYAN BLB_DETAIL_VEC4(0.0f, 1.0f, 1.0f, 1.0f)
#define BLB_COLOR_MAGENTA BLB_DETAIL_VEC4(1.0f, 0.0f, 1.0f, 1.0f)

#define BLB_COLOR_GRAY BLB_DETAIL_VEC4(0.5f, 0.5f, 0.5f, 1.0f)

#define BLB_COLOR_TRANSPARENT BLB_DETAIL_VEC4(0.0f, 0.0f, 0.0f, 0.0f)
