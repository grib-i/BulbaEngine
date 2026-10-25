#include "bulba/core/render/texture.h"

#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef BLB_HAVE_JPEG
#include <jpeglib.h>
#include <setjmp.h>
#endif

typedef struct {
  BLB_Texture *texture;
  char *path;
} BLB_TextureCacheEntry;

static BLB_TextureCacheEntry *texture_cache = NULL;
static size_t texture_cache_count = 0;
static size_t texture_cache_capacity = 0;

static char *duplicate_string(const char *value) {
  if (!value)
    return NULL;

  size_t n = strlen(value) + 1u;
  char *copy = malloc(n);

  if (!copy)
    return NULL;

  memcpy(copy, value, n);

  return copy;
}

static BLB_Texture *texture_cache_find(const char *path) {
  if (!path)
    return NULL;

  BLB_Texture *result = NULL;

  for (size_t i = 0; i < texture_cache_count; ++i) {
    if (texture_cache[i].texture && strcmp(texture_cache[i].path, path) == 0) {
      result = texture_cache[i].texture;
      break;
    }
  }

  return result;
}

static void texture_cache_insert(BLB_Texture *texture, const char *path) {
  if (!texture || !path)
    return;

  char *key = duplicate_string(path);

  if (!key)
    return;

  if (texture_cache_count == texture_cache_capacity) {
    size_t new_capacity = texture_cache_capacity ? texture_cache_capacity * 2u : 32u;

    BLB_TextureCacheEntry *next = realloc(texture_cache, new_capacity * sizeof(*next));

    if (!next) {
      free(key);

      return;
    }
    texture_cache = next;
    texture_cache_capacity = new_capacity;
  }
  texture_cache[texture_cache_count++] = (BLB_TextureCacheEntry){.texture = texture, .path = key};
}

static void texture_cache_remove(BLB_Texture *texture) {
  if (!texture)
    return;

  for (size_t i = 0; i < texture_cache_count; ++i) {
    if (texture_cache[i].texture != texture)
      continue;
    free(texture_cache[i].path);
    texture_cache[i] = texture_cache[texture_cache_count - 1u];
    --texture_cache_count;

    return;
  }
}

BLB_Texture *BLB_Texture_Create2D(uint32_t width, uint32_t height, const void *pixels, size_t pixel_size) {
  if (!width || !height || !pixels)
    return NULL;

  if ((size_t)width > SIZE_MAX / (size_t)height)
    return NULL;

  size_t texels = (size_t)width * (size_t)height;

  if (texels > SIZE_MAX / 4u)
    return NULL;

  size_t expected = texels * 4u;

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
  texture->clamp_to_edge = false;

  return texture;
}

BLB_Texture *BLB_Texture_Create3D(uint32_t width, uint32_t height, uint32_t depth, const void *pixels, size_t pixel_size) {
  if (!width || !height || !depth || !pixels)
    return NULL;

  if ((size_t)width > SIZE_MAX / (size_t)height)
    return NULL;

  size_t texels = (size_t)width * (size_t)height;

  if ((size_t)depth > SIZE_MAX / texels)
    return NULL;

  texels *= (size_t)depth;

  if (texels > SIZE_MAX / 4u)
    return NULL;

  size_t expected = texels * 4u;

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
  texture->type = BLB_TEXTURE_3D;
  texture->format = BLB_TEXTURE_FORMAT_RGBA8;
  texture->width = width;
  texture->height = height;
  texture->depth = depth;
  texture->pixel_size = expected;
  texture->clamp_to_edge = false;

  return texture;
}

#ifdef BLB_HAVE_JPEG
typedef struct {
  struct jpeg_error_mgr base;
  jmp_buf jump;
} BLB_TextureJPEGError;

typedef struct {
  struct jpeg_decompress_struct decoder;
  BLB_TextureJPEGError error;
  FILE *file;
  unsigned char *rgba;
  unsigned char *row;
  int created;
} BLB_TextureJPEGContext;

static void texture_jpeg_error_exit(j_common_ptr decoder) {
  BLB_TextureJPEGError *error = (BLB_TextureJPEGError *)decoder->err;
  longjmp(error->jump, 1);
}

static BLB_Texture *texture_load_jpeg(const char *path) {
  BLB_TextureJPEGContext *context = calloc(1, sizeof(*context));

  if (!context)
    return NULL;

  context->file = fopen(path, "rb");

  if (!context->file) {
    free(context);

    return NULL;
  }

  context->decoder.err = jpeg_std_error(&context->error.base);
  context->error.base.error_exit = texture_jpeg_error_exit;

  if (setjmp(context->error.jump)) {
    if (context->created)
      jpeg_destroy_decompress(&context->decoder);

    fclose(context->file);
    free(context->rgba);
    free(context->row);
    free(context);

    return NULL;
  }

  jpeg_create_decompress(&context->decoder);
  context->created = 1;
  jpeg_stdio_src(&context->decoder, context->file);

  if (jpeg_read_header(&context->decoder, TRUE) != JPEG_HEADER_OK)
    longjmp(context->error.jump, 1);

  context->decoder.out_color_space = JCS_RGB;
  jpeg_start_decompress(&context->decoder);

  const size_t width = (size_t)context->decoder.output_width;
  const size_t height = (size_t)context->decoder.output_height;
  const size_t components = (size_t)context->decoder.output_components;

  if (width == 0 || height == 0 || components != 3u || width > SIZE_MAX / 4u || width * 4u > SIZE_MAX / height || width > SIZE_MAX / components)
    longjmp(context->error.jump, 1);

  const size_t rgba_size = width * height * 4u;
  context->rgba = malloc(rgba_size);
  context->row = malloc(width * components);

  if (!context->rgba || !context->row)
    longjmp(context->error.jump, 1);

  while (context->decoder.output_scanline < context->decoder.output_height) {
    JSAMPROW row = context->row;
    const size_t y = (size_t)context->decoder.output_scanline;

    if (jpeg_read_scanlines(&context->decoder, &row, 1) != 1)
      longjmp(context->error.jump, 1);

    unsigned char *destination = context->rgba + y * width * 4u;

    for (size_t x = 0; x < width; ++x) {
      destination[x * 4u + 0u] = context->row[x * 3u + 0u];
      destination[x * 4u + 1u] = context->row[x * 3u + 1u];
      destination[x * 4u + 2u] = context->row[x * 3u + 2u];
      destination[x * 4u + 3u] = 255u;
    }
  }

  const uint32_t image_width = (uint32_t)width;
  const uint32_t image_height = (uint32_t)height;

  jpeg_finish_decompress(&context->decoder);
  jpeg_destroy_decompress(&context->decoder);

  context->created = 0;
  fclose(context->file);

  BLB_Texture *texture = BLB_Texture_Create2D(image_width, image_height, context->rgba, rgba_size);

  free(context->rgba);
  free(context->row);
  free(context);

  return texture;
}
#endif

BLB_Texture *BLB_Texture_Load2D(const char *path) {
  if (!path)
    return NULL;

  BLB_Texture *cached = texture_cache_find(path);

  if (cached) {
    BLB_Texture_Retain(cached);

    return cached;
  }

  png_image image;
  memset(&image, 0, sizeof(image));

  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_file(&image, path)) {
    png_image_free(&image);
#ifdef BLB_HAVE_JPEG
    BLB_Texture *jpeg_texture = texture_load_jpeg(path);

    if (jpeg_texture)
      texture_cache_insert(jpeg_texture, path);

    return jpeg_texture;
#else
    return NULL;
#endif
  }

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

  if (texture)
    texture_cache_insert(texture, path);

  free(pixels);
  png_image_free(&image);

  return texture;
}

BLB_Texture *BLB_Texture_Load3D(const char *path, uint32_t width, uint32_t height, uint32_t depth) {
  if (!path || width == 0 || height == 0 || depth == 0)
    return NULL;

  png_image image;
  memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_file(&image, path))
    return NULL;

  image.format = PNG_FORMAT_RGBA;
  uint64_t expected_height = (uint64_t)height * (uint64_t)depth;

  if (image.width != width || image.height != expected_height) {
    png_image_free(&image);

    return NULL;
  }

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

  BLB_Texture *texture = BLB_Texture_Create3D(width, height, depth, pixels, size);
  free(pixels);
  png_image_free(&image);

  return texture;
}

// sprite list loader
BLB_Texture *BLB_SpriteList_Load2D(const char *path, uint32_t sprite_x, uint32_t sprite_y, uint32_t sprite_width, uint32_t sprite_height,
                                   uint32_t distance) {
  (void)distance;

  if (!path || !sprite_width || !sprite_height)
    return NULL;

  BLB_Texture *atlas = BLB_Texture_Load2D(path);

  if (!atlas)
    return NULL;

  if (sprite_x >= atlas->width || sprite_y >= atlas->height || sprite_width > atlas->width - sprite_x || sprite_height > atlas->height - sprite_y ||
      (size_t)sprite_width > SIZE_MAX / 4 / sprite_height) {
    BLB_Texture_Release(atlas);

    return NULL;
  }

  size_t size = (size_t)sprite_width * sprite_height * 4;
  unsigned char *pixels = malloc(size);

  if (!pixels) {
    BLB_Texture_Release(atlas);

    return NULL;
  }

  for (uint32_t y = 0; y < sprite_height; ++y)
    memcpy(pixels + (size_t)y * sprite_width * 4, atlas->pixels + ((size_t)(sprite_y + y) * atlas->width + sprite_x) * 4, (size_t)sprite_width * 4);

  BLB_Texture *texture = BLB_Texture_Create2D(sprite_width, sprite_height, pixels, size);
  free(pixels);
  BLB_Texture_Release(atlas);

  return texture;
}

// auto sprite list loader
static int BLB_AutoSpriteRect_Compare(const void *a, const void *b) {
  const BLB_AutoSpriteRect *ra = a;
  const BLB_AutoSpriteRect *rb = b;

  if (ra->y < rb->y)
    return -1;

  if (ra->y > rb->y)
    return 1;

  if (ra->x < rb->x)
    return -1;

  if (ra->x > rb->x)
    return 1;

  return 0;
}

static int BLB_AutoSprite_AddRect(BLB_AutoSpriteRect **sprites, size_t *count, size_t *capacity, BLB_AutoSpriteRect rect) {
  if (!sprites || !count || !capacity)
    return 0;

  if (*count >= *capacity) {
    size_t new_capacity = *capacity ? *capacity * 2u : 16u;

    if (new_capacity < *count + 1u)
      new_capacity = *count + 1u;

    BLB_AutoSpriteRect *new_sprites = realloc(*sprites, new_capacity * sizeof(*new_sprites));

    if (!new_sprites)
      return 0;

    *sprites = new_sprites;
    *capacity = new_capacity;
  }

  (*sprites)[(*count)++] = rect;

  return 1;
}

static int BLB_AutoSprite_FindRects(const unsigned char *pixels, uint32_t width, uint32_t height, uint32_t distance, BLB_AutoSpriteRect **sprites,
                                    size_t *sprite_count, size_t *sprite_capacity) {
  if (!pixels || width == 0 || height == 0 || !sprites || !sprite_count || !sprite_capacity)
    return 0;

  size_t pixel_count = (size_t)width * (size_t)height;

  if (pixel_count > SIZE_MAX / sizeof(uint8_t))
    return 0;

  uint8_t *visited = calloc(pixel_count, sizeof(*visited));

  if (!visited)
    return 0;

  if (pixel_count > SIZE_MAX / sizeof(size_t))
    goto fail;

  size_t *queue = malloc(pixel_count * sizeof(*queue));

  if (!queue)
    goto fail;

  if (distance == 0)
    distance = 1;

  for (uint32_t start_y = 0; start_y < height; ++start_y) {
    for (uint32_t start_x = 0; start_x < width; ++start_x) {
      size_t start_index = (size_t)start_y * width + start_x;

      if (visited[start_index])
        continue;

      if (pixels[start_index * 4u + 3u] == 0)
        continue;

      size_t queue_head = 0;
      size_t queue_tail = 0;

      queue[queue_tail++] = start_index;
      visited[start_index] = 1;

      uint32_t min_x = start_x;
      uint32_t min_y = start_y;
      uint32_t max_x = start_x;
      uint32_t max_y = start_y;

      while (queue_head < queue_tail) {
        size_t current_index = queue[queue_head++];

        uint32_t current_x = (uint32_t)(current_index % width);
        uint32_t current_y = (uint32_t)(current_index / width);

        if (current_x < min_x)
          min_x = current_x;

        if (current_y < min_y)
          min_y = current_y;

        if (current_x > max_x)
          max_x = current_x;

        if (current_y > max_y)
          max_y = current_y;

        const int dirs[4][2] = {
            {1, 0},
            {-1, 0},
            {0, 1},
            {0, -1},
        };

        for (int direction = 0; direction < 4; ++direction) {
          int dx = dirs[direction][0];
          int dy = dirs[direction][1];

          for (uint32_t step = 1; step <= distance; ++step) {
            int nx = (int)current_x + dx * (int)step;
            int ny = (int)current_y + dy * (int)step;

            if (nx < 0 || ny < 0 || nx >= (int)width || ny >= (int)height)
              break;

            size_t neighbor_index = (size_t)ny * width + (size_t)nx;

            if (visited[neighbor_index])
              break;

            if (pixels[neighbor_index * 4u + 3u] == 0)
              continue;

            visited[neighbor_index] = 1;

            if (queue_tail >= pixel_count) {
              free(queue);
              free(visited);

              return 0;
            }

            queue[queue_tail++] = neighbor_index;
            break;
          }
        }
      }

      BLB_AutoSpriteRect rect = {
          .x = min_x,
          .y = min_y,
          .width = max_x - min_x + 1u,
          .height = max_y - min_y + 1u,
      };

      if (!BLB_AutoSprite_AddRect(sprites, sprite_count, sprite_capacity, rect)) {
        free(queue);
        free(visited);

        return 0;
      }
    }
  }

  free(queue);
  free(visited);

  return 1;
fail:
  free(visited);

  return 0;
}

static void BLB_AutoSprite_FixTransparentPixels(unsigned char *pixels, uint32_t width, uint32_t height) {
  if (!pixels || width == 0 || height == 0)
    return;

  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      size_t index = ((size_t)y * width + x) * 4u;

      if (pixels[index + 3u] != 0)
        continue;

      int found = 0;

      for (int radius = 1; radius <= 2 && !found; ++radius) {
        const int dirs[8][2] = {
            {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1},
        };

        for (int i = 0; i < 8; ++i) {
          int nx = (int)x + dirs[i][0] * radius;
          int ny = (int)y + dirs[i][1] * radius;

          if (nx < 0 || ny < 0 || nx >= (int)width || ny >= (int)height)
            continue;

          size_t neighbor = ((size_t)ny * width + (size_t)nx) * 4u;

          if (pixels[neighbor + 3u] != 0) {
            pixels[index + 0u] = pixels[neighbor + 0u];
            pixels[index + 1u] = pixels[neighbor + 1u];
            pixels[index + 2u] = pixels[neighbor + 2u];
            found = 1;
            break;
          }
        }
      }
    }
  }
}

BLB_Texture **BLB_SpriteListAuto_Load2D(const char *path, uint32_t distance) {
  if (!path)
    return NULL;

  png_image image;
  memset(&image, 0, sizeof(image));
  image.version = PNG_IMAGE_VERSION;

  if (!png_image_begin_read_from_file(&image, path))
    return NULL;

  image.format = PNG_FORMAT_RGBA;

  size_t image_size = PNG_IMAGE_SIZE(image);

  if (image_size == 0) {
    png_image_free(&image);

    return NULL;
  }

  unsigned char *pixels = malloc(image_size);

  if (!pixels) {
    png_image_free(&image);

    return NULL;
  }

  if (!png_image_finish_read(&image, NULL, pixels, 0, NULL)) {
    free(pixels);
    png_image_free(&image);

    return NULL;
  }

  BLB_AutoSpriteRect *sprites = NULL;
  size_t sprite_count = 0;
  size_t sprite_capacity = 0;

  if (!BLB_AutoSprite_FindRects(pixels, image.width, image.height, distance, &sprites, &sprite_count, &sprite_capacity)) {
    free(sprites);
    free(pixels);
    png_image_free(&image);

    return NULL;
  }

  if (sprite_count == 0) {
    free(sprites);
    free(pixels);
    png_image_free(&image);

    return NULL;
  }

  qsort(sprites, sprite_count, sizeof(*sprites), BLB_AutoSpriteRect_Compare);

  BLB_Texture **textures = calloc(sprite_count + 1u, sizeof(BLB_Texture *));

  if (!textures) {
    free(sprites);
    free(pixels);
    png_image_free(&image);

    return NULL;
  }

  for (size_t i = 0; i < sprite_count; ++i) {
    BLB_AutoSpriteRect *rect = &sprites[i];

    if (rect->width == 0 || rect->height == 0)
      goto fail_textures;

    if ((size_t)rect->width > SIZE_MAX / (size_t)rect->height)
      goto fail_textures;

    size_t texels = (size_t)rect->width * (size_t)rect->height;

    if (texels > SIZE_MAX / 4u)
      goto fail_textures;

    size_t sprite_size = texels * 4u;

    unsigned char *sprite_pixels = malloc(sprite_size);

    if (!sprite_pixels)
      goto fail_textures;

    for (uint32_t y = 0; y < rect->height; ++y) {
      size_t source_index = ((size_t)rect->y + y) * image.width + rect->x;
      size_t destination_index = (size_t)y * rect->width;

      memcpy(sprite_pixels + destination_index * 4u, pixels + source_index * 4u, (size_t)rect->width * 4u);
    }

    BLB_AutoSprite_FixTransparentPixels(sprite_pixels, rect->width, rect->height);

    textures[i] = BLB_Texture_Create2D(rect->width, rect->height, sprite_pixels, sprite_size);

    free(sprite_pixels);

    if (!textures[i])
      goto fail_textures;

    textures[i]->clamp_to_edge = true;
  }

  textures[sprite_count] = NULL;

  free(sprites);
  free(pixels);
  png_image_free(&image);

  return textures;
fail_textures:
  BLB_SpriteList_Destroy(textures);
  free(sprites);
  free(pixels);
  png_image_free(&image);

  return NULL;
}

void BLB_SpriteList_Destroy(BLB_Texture **textures) {
  if (!textures)
    return;

  for (size_t i = 0; textures[i] != NULL; i++)
    BLB_Texture_Destroy(textures[i]);

  free(textures);
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

  texture_cache_remove(texture);

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

bool BLB_Texture_IsLoaded(const BLB_Texture *texture) {
  return texture && texture->pixels && texture->width > 0 && texture->height > 0 && (texture->type != BLB_TEXTURE_3D || texture->depth > 0);
}
