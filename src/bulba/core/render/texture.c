#include "bulba/core/render/texture.h"

#include <png.h>
#include <stdint.h>
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
  texture->clamp_to_edge = false;

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

BLB_Texture *BLB_SpriteList_Load2D(const char *path, uint32_t sprite_x, uint32_t sprite_y, uint32_t sprite_width, uint32_t sprite_height,
                                   uint32_t distance) {
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

  size_t sprite_size = (size_t)sprite_width * (size_t)sprite_height * 4;
  unsigned char *sprite_pixels = malloc(sprite_size);

  for (uint32_t y = 0; y < sprite_height; y++) {
    const unsigned char *src = pixels + ((sprite_y + y) * image.width + sprite_x) * 4;
    unsigned char *dst = sprite_pixels + (y * sprite_width) * 4;

    memcpy(dst, src, sprite_width * 4);
  }

  BLB_Texture *texture = BLB_Texture_Create2D(sprite_width, sprite_height, sprite_pixels, sprite_size);

  free(pixels);
  png_image_free(&image);

  return texture;
}

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
  if (*count >= *capacity) {
    size_t new_capacity = *capacity ? *capacity * 2 : 16;

    BLB_AutoSpriteRect *new_sprites = realloc(*sprites, new_capacity * sizeof(**sprites));

    if (!new_sprites)
      return 0;

    *sprites = new_sprites;
    *capacity = new_capacity;
  }

  (*sprites)[*count] = rect;
  (*count)++;

  return 1;
}

static int BLB_AutoSprite_FindRects(const unsigned char *pixels, uint32_t width, uint32_t height, uint32_t distance, BLB_AutoSpriteRect **sprites,
                                    size_t *sprite_count, size_t *sprite_capacity) {
  if (!pixels || !width || !height || !sprites || !sprite_count || !sprite_capacity)
    return 0;

  if (distance == 0)
    distance = 1;

  size_t pixel_count = (size_t)width * height;

  uint8_t *visited = calloc(pixel_count, sizeof(*visited));

  if (!visited)
    return 0;

  size_t *queue = malloc(pixel_count * sizeof(*queue));

  if (!queue) {
    free(visited);
    return 0;
  }

  const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  for (uint32_t start_y = 0; start_y < height; start_y++) {
    for (uint32_t start_x = 0; start_x < width; start_x++) {
      size_t start_index = ((size_t)start_y * width + start_x);

      size_t alpha_index = start_index * 4 + 3;

      if (visited[start_index])
        continue;

      if (pixels[alpha_index] == 0)
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

        uint32_t current_x = current_index % width;

        uint32_t current_y = current_index / width;

        if (current_x < min_x)
          min_x = current_x;

        if (current_y < min_y)
          min_y = current_y;

        if (current_x > max_x)
          max_x = current_x;

        if (current_y > max_y)
          max_y = current_y;

        for (size_t direction = 0; direction < 4; direction++) {
          int dx = dirs[direction][0];
          int dy = dirs[direction][1];

          for (uint32_t step = 1; step <= distance; step++) {
            int nx = (int)current_x + dx * (int)step;
            int ny = (int)current_y + dy * (int)step;

            if (nx < 0 || ny < 0 || nx >= (int)width || ny >= (int)height)
              break;

            size_t neighbor_index = (size_t)ny * width + (size_t)nx;

            if (visited[neighbor_index])
              break;

            size_t neighbor_alpha = neighbor_index * 4 + 3;

            if (pixels[neighbor_alpha] == 0)
              continue;

            visited[neighbor_index] = 1;
            queue[queue_tail++] = neighbor_index;

            break;
          }
        }
      }

      BLB_AutoSpriteRect rect = {.x = min_x, .y = min_y, .width = max_x - min_x + 1, .height = max_y - min_y + 1};

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

  BLB_Texture **textures = calloc(sprite_count + 1, sizeof(BLB_Texture *));

  if (!textures) {
    free(sprites);
    free(pixels);
    png_image_free(&image);
    return NULL;
  }

  for (size_t i = 0; i < sprite_count; i++) {
    BLB_AutoSpriteRect *rect = &sprites[i];

    size_t sprite_size = (size_t)rect->width * rect->height * 4;

    unsigned char *sprite_pixels = malloc(sprite_size);

    if (!sprite_pixels) {
      textures[i] = NULL;
      BLB_SpriteList_Destroy(textures);
      free(sprites);
      free(pixels);
      png_image_free(&image);
      return NULL;
    }

    for (uint32_t y = 0; y < rect->height; y++) {
      size_t source_index = ((size_t)(rect->y + y) * image.width + rect->x) * 4;

      size_t destination_index = (size_t)y * rect->width * 4;

      memcpy(sprite_pixels + destination_index, pixels + source_index, (size_t)rect->width * 4);
    }

    textures[i] = BLB_Texture_Create2D(rect->width, rect->height, sprite_pixels, sprite_size);

    if (!textures[i]) {
      free(sprite_pixels);
      textures[i] = NULL;
      BLB_SpriteList_Destroy(textures);
      free(sprites);
      free(pixels);
      png_image_free(&image);
      return NULL;
    }

    free(sprite_pixels);
  }

  textures[sprite_count] = NULL;

  free(sprites);
  free(pixels);
  png_image_free(&image);

  return textures;
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
