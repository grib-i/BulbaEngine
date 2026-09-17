#include "bulba/graphics/vulkan/renderer.h"

#include <stdio.h>
#include <stdlib.h>

char *read_binary(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  long file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    return NULL;
  }

  rewind(file);

  char *buffer = malloc((size_t)file_size);
  if (!buffer) {
    fclose(file);
    return NULL;
  }

  size_t read_size = fread(buffer, 1, (size_t)file_size, file);
  fclose(file);

  if (read_size != (size_t)file_size) {
    free(buffer);
    return NULL;
  }

  if (size)
    *size = read_size;

  return buffer;
}
