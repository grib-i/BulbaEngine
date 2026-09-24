#include "bulba/core/objects2d/text2d.h"
#include "bulba/core/render/material.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static const char *default_font_paths[] = {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/calibri.ttf"};
#elif defined(__APPLE__)
static const char *default_font_paths[] = {"/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Supplemental/Helvetica.ttc",
                                           "/Library/Fonts/Arial.ttf"};
#elif defined(__ANDROID__)
static const char *default_font_paths[] = {"/system/fonts/Roboto-Regular.ttf", "/system/fonts/NotoSans-Regular.ttf"};
#else
static const char *default_font_paths[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                           "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                                           "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/freefont/FreeSans.ttf"};
#endif

static char *dupstr(const char *s) {
  if (!s)
    return NULL;

  size_t n = strlen(s) + 1;
  char *d = malloc(n);

  if (d)
    memcpy(d, s, n);

  return d;
}

static const char *find_default_font(void) {
  size_t count = sizeof(default_font_paths) / sizeof(default_font_paths[0]);

  for (size_t i = 0; i < count; i++) {
    FILE *file = fopen(default_font_paths[i], "rb");

    if (!file)
      continue;

    fclose(file);

    return default_font_paths[i];
  }

  return NULL;
}

static int load_text_font(BLB_Text2D *text, const char *path) {
  if (!text)
    return -1;

  const char *font_path = path;

  if (!font_path)
    font_path = find_default_font();

  if (!font_path)
    return -1;

  free(text->font_path);

  text->font_path = dupstr(font_path);

  if (!text->font_path)
    return -1;

  if (BLB_FontLoad(&text->font, text->font_path, (unsigned int)text->size) != 0) {
    free(text->font_path);
    text->font_path = NULL;
    return -1;
  }

  text->font_loaded = true;

  return 0;
}

BLB_Text2D *BLB_CreateText2D(const char *s, const char *font, HMM_Vec2 pos, float size, bool screen_space) {
  BLB_Text2D *t = calloc(1, sizeof(*t));

  if (!t)
    return NULL;

  const char *initial_text = s ? s : "";
  size_t text_length = strlen(initial_text) + 1;

  t->text = malloc(text_length);

  if (!t->text) {
    free(t);
    return NULL;
  }

  memcpy(t->text, initial_text, text_length);
  t->text_capacity = text_length;

  t->position = pos;
  t->scale = HMM_V2(1.0f, 1.0f);
  t->rotation = 0.0f;

  t->size = size > 0.0f ? size : 16.0f;

  t->color[0] = 255;
  t->color[1] = 255;
  t->color[2] = 255;
  t->color[3] = 255;

  t->glow = 0.0f;
  t->emission = 0.0f;
  t->roundness = 0.0f;

  t->layer = 0;
  t->render_mode = BLB_RENDER_OPAQUE;
  t->visible = true;
  t->screen_space = screen_space;
  t->entity_id = BLB_INVALID_ENTITY_ID;
  t->component_mask = BLB_COMPONENT_TRANSFORM | BLB_COMPONENT_RENDERABLE;

  t->material = BLB_Material_Create2D();

  if (!t->material) {
    BLB_DestroyText2D(t);
    return NULL;
  }

  t->delta_time = calloc(1, sizeof(float));

  if (!t->delta_time) {
    BLB_DestroyText2D(t);
    return NULL;
  }

  if (load_text_font(t, font) != 0) {
    BLB_DestroyText2D(t);
    return NULL;
  }

  return t;
}

void BLB_DestroyText2D(BLB_Text2D *t) {
  if (!t)
    return;

  if (t->font_loaded)
    BLB_FontDestroy(&t->font);

  if (t->material)
    BLB_Material_Release(t->material);

  free(t->text);
  free(t->font_path);

  free(t);
}

void BLB_SetText2D(BLB_Text2D *t, const char *s) {
  if (!t)
    return;

  if (!s)
    s = "";

  if (!t->text) {
    size_t length = strlen(s) + 1;

    char *text = malloc(length);

    if (!text)
      return;

    memcpy(text, s, length);

    t->text = text;
    t->text_capacity = length;

    return;
  }

  size_t length = strlen(s) + 1;

  if (length > t->text_capacity) {
    size_t new_capacity = t->text_capacity;

    if (new_capacity == 0)
      new_capacity = 1;

    while (new_capacity < length) {
      if (new_capacity > SIZE_MAX / 2) {
        new_capacity = length;
        break;
      }

      new_capacity *= 2;
    }

    char *new_text = realloc(t->text, new_capacity);

    if (!new_text)
      return;

    t->text = new_text;
    t->text_capacity = new_capacity;
  }

  memcpy(t->text, s, length);
}
