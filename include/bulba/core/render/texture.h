#ifndef BULBA_CORE_RENDER_TEXTURE_H
#define BULBA_CORE_RENDER_TEXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { BLB_TEXTURE_2D = 0, BLB_TEXTURE_3D = 1, BLB_TEXTURE_CUBE = 2 } BLB_TextureType;

typedef enum { BLB_TEXTURE_FORMAT_RGBA8 = 0 } BLB_TextureFormat;

typedef struct {
  uint32_t x;
  uint32_t y;
  uint32_t width;
  uint32_t height;
} BLB_AutoSpriteRect;

typedef struct BLB_Texture {
  size_t ref_count;

  BLB_TextureType type;
  BLB_TextureFormat format;

  uint32_t width;
  uint32_t height;
  uint32_t depth;

  unsigned char *pixels;
  size_t pixel_size;

  bool clamp_to_edge;

  void *backend_data;

  void (*backend_destroy)(void *backend_data);
} BLB_Texture;

BLB_Texture *BLB_Texture_Load2D(const char *path);

BLB_Texture *BLB_SpriteList_Load2D(const char *path, uint32_t sprite_x, uint32_t sprite_y, uint32_t sprite_width, uint32_t sprite_height,
                                   uint32_t distance);

BLB_Texture **BLB_SpriteListAuto_Load2D(const char *path, uint32_t distance);

void BLB_SpriteList_Destroy(BLB_Texture **textures);

BLB_Texture *BLB_Texture_Create2D(uint32_t width, uint32_t height, const void *pixels, size_t pixel_size);

void BLB_Texture_Retain(BLB_Texture *texture);

void BLB_Texture_Release(BLB_Texture *texture);

void BLB_Texture_Destroy(BLB_Texture *texture);

uint32_t BLB_Texture_GetWidth(const BLB_Texture *texture);

uint32_t BLB_Texture_GetHeight(const BLB_Texture *texture);

uint32_t BLB_Texture_GetDepth(const BLB_Texture *texture);

BLB_TextureType BLB_Texture_GetType(const BLB_Texture *texture);

BLB_TextureFormat BLB_Texture_GetFormat(const BLB_Texture *texture);

bool BLB_Texture_IsLoaded(const BLB_Texture *texture);

#endif
