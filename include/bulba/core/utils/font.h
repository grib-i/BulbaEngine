#ifndef BULBA_CORE_UTILS_FONT_H
#define BULBA_CORE_UTILS_FONT_H

#include <stddef.h>
#include <stdint.h>

typedef struct FontGlyph {
  uint32_t codepoint;

  unsigned int x;
  unsigned int y;
  unsigned int width;
  unsigned int height;

  int bearing_x;
  int bearing_y;
  int advance_x;

} FontGlyph;

typedef struct Font {

  unsigned char *atlas;

  unsigned int atlas_width;
  unsigned int atlas_height;

  unsigned int size;

  FontGlyph *glyphs;

  size_t glyph_count;

} Font;

int BLB_FontLoad(Font *font, const char *path, unsigned int size);

void BLB_FontDestroy(Font *font);

const FontGlyph *BLB_FontGetGlyph(const Font *font, uint32_t codepoint);

#endif
