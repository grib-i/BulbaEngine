#ifndef BULBA_CORE_UTILS_COLORS_H
#define BULBA_CORE_UTILS_COLORS_H

typedef union BLB_Color {
  struct {
    unsigned char r, g, b, a;
  };

  unsigned char rgba[4];
} BLB_Color;

static inline BLB_Color BLB_ColorRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
  return (BLB_Color){.r = r, .g = g, .b = b, .a = a};
}

static inline BLB_Color BLB_ColorRGB(unsigned char r, unsigned char g, unsigned char b) {
  return BLB_ColorRGBA(r, g, b, 255);
}

/* Colors: RGBA */
#define BLB_COLOR_BLACK ((BLB_Color)(0.0f, 0.0f, 0.0f, 1.0f))
#define BLB_COLOR_WHITE ((BLB_Color)(1.0f, 1.0f, 1.0f, 1.0f))

#define BLB_COLOR_RED ((BLB_Color)(1.0f, 0.0f, 0.0f, 1.0f))
#define BLB_COLOR_GREEN ((BLB_Color)(0.0f, 1.0f, 0.0f, 1.0f))
#define BLB_COLOR_BLUE ((BLB_Color)(0.0f, 0.0f, 1.0f, 1.0f))

#define BLB_COLOR_YELLOW ((BLB_Color)(1.0f, 1.0f, 0.0f, 1.0f))
#define BLB_COLOR_CYAN ((BLB_Color)(0.0f, 1.0f, 1.0f, 1.0f))
#define BLB_COLOR_MAGENTA ((BLB_Color)(1.0f, 0.0f, 1.0f, 1.0f))

#define BLB_COLOR_GRAY ((BLB_Color)(0.5f, 0.5f, 0.5f, 1.0f))

#define BLB_COLOR_TRANSPARENT ((BLB_Color)(0.0f, 0.0f, 0.0f, 0.0f))

#endif
