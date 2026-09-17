#include "bulba/core/render/texture.h"

#include <png.h>
#include <stdlib.h>
#include <string.h>

BLB_Texture *BLB_Texture_Create2D(uint32_t width, uint32_t height, const void *pixels, size_t pixel_size) {
  if (!width || !height || !pixels)
    return NULL;

  size_t expected = (size_t)width * (size_t)height * 4;

  if (pixel_size != expected)
    return NULL;

  BLB_Texture *texture = calloc(1, sizeof(*texture));

  if (!texture)
    return NULL;

  texture->pixels = malloc(expected);

  if (!texture->pixels) {
    free(texture);
    return NULL;
  }

  memcpy(texture->pixels, pixels, expected);

  texture->ref_count = 1;
  texture->type = BLB_TEXTURE_2D;
  texture->format = BLB_TEXTURE_FORMAT_RGBA8;
  texture->width = width;
  texture->height = height;
  texture->depth = 1;
  texture->pixel_size = expected;

  return texture;
}

BLB_Texture *BLB_Texture_Load2D(const char *path) {
  if (!path)
    return NULL;

  png_image image;
  memset(&image, 0, sizeof(image));

  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_file(&image, path))
    return NULL;

  image.format = PNG_FORMAT_RGBA;

  size_t size = PNG_IMAGE_SIZE(image);

  unsigned char *pixels = malloc(size);

  if (!pixels) {
    png_image_free(&image);
    return NULL;
  }

  if (!png_image_finish_read(&image, NULL, pixels, 0, NULL)) {
    free(pixels);
    png_image_free(&image);
    return NULL;
  }

  BLB_Texture *texture = BLB_Texture_Create2D(image.width, image.height, pixels, size);

  free(pixels);
  png_image_free(&image);

  return texture;
}

void BLB_Texture_Retain(BLB_Texture *texture) {
  if (!texture)
    return;

  texture->ref_count++;
}

void BLB_Texture_Release(BLB_Texture *texture) {
  if (!texture)
    return;

  if (texture->ref_count > 1) {
    texture->ref_count--;
    return;
  }

  if (texture->backend_destroy && texture->backend_data) {
    texture->backend_destroy(texture->backend_data);
  }

  free(texture->pixels);
  free(texture);
}

void BLB_Texture_Destroy(BLB_Texture *texture) { BLB_Texture_Release(texture); }

uint32_t BLB_Texture_GetWidth(const BLB_Texture *texture) { return texture ? texture->width : 0; }

uint32_t BLB_Texture_GetHeight(const BLB_Texture *texture) { return texture ? texture->height : 0; }

uint32_t BLB_Texture_GetDepth(const BLB_Texture *texture) { return texture ? texture->depth : 0; }

BLB_TextureType BLB_Texture_GetType(const BLB_Texture *texture) { return texture ? texture->type : BLB_TEXTURE_2D; }

BLB_TextureFormat BLB_Texture_GetFormat(const BLB_Texture *texture) { return texture ? texture->format : BLB_TEXTURE_FORMAT_RGBA8; }

bool BLB_Texture_IsLoaded(const BLB_Texture *texture) { return texture && texture->pixels && texture->width > 0 && texture->height > 0; }
