#include "bulba/core/utils/font.h"

#include <stdlib.h>
#include <string.h>

#define FONT_MAX_CODEPOINT 0x0500
#define FONT_ATLAS_COLUMNS 32
#define FONT_ATLAS_PADDING 2

#ifdef BULBA_USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H

#endif

int BLB_FontLoad(Font *font, const char *path, unsigned int size) {
  if (font == NULL || path == NULL)
    return -1;

  memset(font, 0, sizeof(*font));

  font->size = size > 0 ? size : 16;

#ifdef BULBA_USE_FREETYPE

  FT_Library ft = NULL;
  FT_Face face = NULL;

  if (FT_Init_FreeType(&ft) != 0)
    return -1;

  if (FT_New_Face(ft, path, 0, &face) != 0) {
    FT_Done_FreeType(ft);
    return -1;
  }

  FT_Set_Pixel_Sizes(face, 0, font->size);

  unsigned int cell = font->size + 8;
  unsigned int padding = FONT_ATLAS_PADDING;

  unsigned int rows = (FONT_MAX_CODEPOINT + FONT_ATLAS_COLUMNS - 1) / FONT_ATLAS_COLUMNS;

  font->atlas_width = cell * FONT_ATLAS_COLUMNS;
  font->atlas_height = cell * rows;

  font->atlas = calloc(1, (size_t)font->atlas_width * (size_t)font->atlas_height);

  font->glyphs = calloc(FONT_MAX_CODEPOINT, sizeof(FontGlyph));

  if (font->atlas == NULL || font->glyphs == NULL) {
    BLB_FontDestroy(font);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return -1;
  }

  font->glyph_count = FONT_MAX_CODEPOINT;

  for (unsigned int cp = 0; cp < FONT_MAX_CODEPOINT; cp++) {

    FontGlyph *glyph = &font->glyphs[cp];

    glyph->codepoint = cp;

    glyph->x = (cp % FONT_ATLAS_COLUMNS) * cell + padding;

    glyph->y = (cp / FONT_ATLAS_COLUMNS) * cell + padding;

    glyph->width = 0;
    glyph->height = 0;

    glyph->bearing_x = 0;
    glyph->bearing_y = 0;

    glyph->advance_x = (int)font->size;
  }

  for (unsigned int cp = 32; cp < FONT_MAX_CODEPOINT; cp++) {

    if (FT_Load_Char(face, cp, FT_LOAD_DEFAULT) != 0)
      continue;

    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
      continue;

    FT_GlyphSlot slot = face->glyph;

    FT_Bitmap *bitmap = &slot->bitmap;

    FontGlyph *glyph = &font->glyphs[cp];

    unsigned int copy_width = bitmap->width;
    unsigned int copy_height = bitmap->rows;

    if (copy_width > cell - padding * 2)
      copy_width = cell - padding * 2;

    if (copy_height > cell - padding * 2)
      copy_height = cell - padding * 2;

    glyph->width = copy_width;
    glyph->height = copy_height;

    glyph->bearing_x = slot->bitmap_left;
    glyph->bearing_y = slot->bitmap_top;

    glyph->advance_x = (int)(slot->advance.x / 64);

    if (bitmap->buffer == NULL || copy_width == 0 || copy_height == 0) {

      continue;
    }

    unsigned int dst_x = glyph->x;
    unsigned int dst_y = glyph->y;
    glyph->y = dst_y;
    int pitch = bitmap->pitch;

    for (unsigned int row = 0; row < copy_height; row++) {
      const unsigned char *src;

      if (pitch >= 0)
        src = bitmap->buffer + (size_t)row * (size_t)pitch;
      else
        src = bitmap->buffer + (size_t)(copy_height - 1 - row) * (size_t)(-pitch);

      unsigned char *dst = font->atlas + (size_t)(dst_y + row) * font->atlas_width + dst_x;
      memcpy(dst, src, copy_width);
    }
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  return 0;

#else

  (void)path;

  font->atlas_width = 128;
  font->atlas_height = 128;

  font->atlas = calloc(1, font->atlas_width * font->atlas_height);

  font->glyphs = calloc(FONT_MAX_CODEPOINT, sizeof(FontGlyph));

  if (font->atlas == NULL || font->glyphs == NULL) {

    BLB_FontDestroy(font);

    return -1;
  }

  font->glyph_count = FONT_MAX_CODEPOINT;

  for (unsigned int cp = 0; cp < FONT_MAX_CODEPOINT; cp++) {

    FontGlyph *glyph = &font->glyphs[cp];

    glyph->codepoint = cp;

    glyph->width = 8;
    glyph->height = 8;

    glyph->advance_x = 8;
    glyph->bearing_y = 8;
  }

  return 0;

#endif
}

void BLB_FontDestroy(Font *font) {

  if (font == NULL)
    return;

  free(font->atlas);
  free(font->glyphs);

  memset(font, 0, sizeof(*font));
}

const FontGlyph *BLB_FontGetGlyph(const Font *font, unsigned int codepoint) {

  if (font == NULL || font->glyphs == NULL)
    return NULL;

  if (codepoint >= font->glyph_count)
    return NULL;

  return &font->glyphs[codepoint];
}
