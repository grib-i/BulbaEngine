#include <png.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message) {
  fprintf(stderr, "png_to_c: %s\n", message);
  exit(EXIT_FAILURE);
}

static void write_header(const char *path, const char *name) {
  FILE *file = fopen(path, "w");

  if (!file)
    fail("failed to create header");

  fprintf(file,
          "#ifndef %s_H\n"
          "#define %s_H\n"
          "\n"
          "#include <stddef.h>\n"
          "#include <stdint.h>\n"
          "\n"
          "extern const unsigned char %s_pixels[];\n"
          "extern const size_t %s_pixel_size;\n"
          "extern const uint32_t %s_width;\n"
          "extern const uint32_t %s_height;\n"
          "\n"
          "#endif\n",
          name, name, name, name, name, name);

  fclose(file);
}

static void write_source(const char *path, const char *header_name, const char *name, const unsigned char *pixels, size_t pixel_size, uint32_t width,
                         uint32_t height) {
  FILE *file = fopen(path, "w");

  if (!file)
    fail("failed to create source");

  fprintf(file, "#include \"%s\"\n\n", header_name);

  fprintf(file, "const unsigned char %s_pixels[] = {\n", name);

  for (size_t i = 0; i < pixel_size; i++) {
    if (i % 12 == 0)
      fprintf(file, "  ");

    fprintf(file, "0x%02X", pixels[i]);

    if (i + 1 != pixel_size)
      fprintf(file, ", ");

    if (i % 12 == 11 || i + 1 == pixel_size)
      fprintf(file, "\n");
  }

  fprintf(file,
          "};\n\n"
          "const size_t %s_pixel_size = %zu;\n"
          "const uint32_t %s_width = %u;\n"
          "const uint32_t %s_height = %u;\n",
          name, pixel_size, name, width, name, height);

  fclose(file);
}

int main(int argc, char **argv) {
  if (argc != 5) {
    fprintf(stderr, "usage: png_to_c <input.png> <output.c> <output.h> <name>\n");

    return EXIT_FAILURE;
  }

  const char *input_path = argv[1];
  const char *output_c = argv[2];
  const char *output_h = argv[3];
  const char *name = argv[4];

  FILE *file = fopen(input_path, "rb");

  if (!file)
    fail("failed to open PNG");

  unsigned char signature[8];

  if (fread(signature, 1, 8, file) != 8) {
    fclose(file);
    fail("failed to read PNG signature");
  }

  if (png_sig_cmp(signature, 0, 8) != 0) {
    fclose(file);
    fail("file is not a PNG");
  }

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png) {
    fclose(file);
    fail("failed to create PNG read struct");
  }

  png_infop info = png_create_info_struct(png);

  if (!info) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(file);
    fail("failed to create PNG info struct");
  }

  if (setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    fail("failed to decode PNG");
  }

  png_init_io(png, file);
  png_set_sig_bytes(png, 8);

  png_read_info(png, info);

  png_uint_32 width;
  png_uint_32 height;

  int bit_depth;
  int color_type;
  int interlace_type;
  int compression_type;
  int filter_method;

  png_get_IHDR(png, info, &width, &height, &bit_depth, &color_type, &interlace_type, &compression_type, &filter_method);

  if (bit_depth == 16)
    png_set_strip_16(png);

  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png);
  }

  int has_alpha = (color_type & PNG_COLOR_MASK_ALPHA) != 0 || png_get_valid(png, info, PNG_INFO_tRNS) != 0;

  if (png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(png);
  }

  if (!has_alpha)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  png_read_update_info(png, info);

  png_size_t row_bytes = png_get_rowbytes(png, info);

  if (row_bytes != width * 4) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    fail("decoded PNG is not RGBA8");
  }

  size_t pixel_size = (size_t)width * (size_t)height * 4;

  unsigned char *pixels = malloc(pixel_size);

  if (!pixels) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    fail("failed to allocate pixel buffer");
  }

  png_bytep *rows = malloc(sizeof(*rows) * height);

  if (!rows) {
    free(pixels);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    fail("failed to allocate row pointers");
  }

  for (png_uint_32 y = 0; y < height; y++)
    rows[y] = pixels + (size_t)y * row_bytes;

  png_read_image(png, rows);
  png_read_end(png, NULL);

  free(rows);

  png_destroy_read_struct(&png, &info, NULL);
  fclose(file);

  const char *header_basename = strrchr(output_h, '/');

  if (header_basename)
    header_basename++;
  else
    header_basename = output_h;

  write_header(output_h, name);

  write_source(output_c, header_basename, name, pixels, pixel_size, (uint32_t)width, (uint32_t)height);

  free(pixels);

  return EXIT_SUCCESS;
}
