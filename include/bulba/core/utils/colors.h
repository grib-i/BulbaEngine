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

/* Convert normalized floating-point channels (0.0..1.0) to 8-bit RGBA. */
static inline unsigned char BLB_ColorChannelFromNormalized(float value) {
  if (!(value > 0.0f))
    return 0;
  if (value >= 1.0f)
    return 255;
  return (unsigned char)(value * 255.0f + 0.5f);
}

static inline BLB_Color BLB_ColorFromNormalized(float r, float g, float b, float a) {
  return BLB_ColorRGBA(BLB_ColorChannelFromNormalized(r), BLB_ColorChannelFromNormalized(g),
                       BLB_ColorChannelFromNormalized(b), BLB_ColorChannelFromNormalized(a));
}

static inline float BLB_ColorChannelToNormalized(unsigned char value) { return (float)value / 255.0f; }

static inline BLB_Color BLB_ColorLerp(BLB_Color start, BLB_Color end, float t) {
  if (!(t > 0.0f))
    t = 0.0f;
  else if (t > 1.0f)
    t = 1.0f;

  return BLB_ColorRGBA(
      (unsigned char)(start.r + ((float)end.r - start.r) * t + 0.5f),
      (unsigned char)(start.g + ((float)end.g - start.g) * t + 0.5f),
      (unsigned char)(start.b + ((float)end.b - start.b) * t + 0.5f),
      (unsigned char)(start.a + ((float)end.a - start.a) * t + 0.5f));
}

/* Standard RGBA colors (8-bit channels). */
#define BLB_COLOR_BLACK ((BLB_Color){.r = 0, .g = 0, .b = 0, .a = 255})
#define BLB_COLOR_WHITE ((BLB_Color){.r = 255, .g = 255, .b = 255, .a = 255})

#define BLB_COLOR_RED ((BLB_Color){.r = 255, .g = 0, .b = 0, .a = 255})
#define BLB_COLOR_GREEN ((BLB_Color){.r = 0, .g = 255, .b = 0, .a = 255})
#define BLB_COLOR_BLUE ((BLB_Color){.r = 0, .g = 0, .b = 255, .a = 255})

#define BLB_COLOR_YELLOW ((BLB_Color){.r = 255, .g = 255, .b = 0, .a = 255})
#define BLB_COLOR_CYAN ((BLB_Color){.r = 0, .g = 255, .b = 255, .a = 255})
#define BLB_COLOR_MAGENTA ((BLB_Color){.r = 255, .g = 0, .b = 255, .a = 255})

#define BLB_COLOR_GRAY ((BLB_Color){.r = 128, .g = 128, .b = 128, .a = 255})
#define BLB_COLOR_TRANSPARENT ((BLB_Color){.r = 0, .g = 0, .b = 0, .a = 0})

#endif
