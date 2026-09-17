#ifndef TEXT2D_H
#define TEXT2D_H

#include "bulba/core/math3v/math3v.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/render/material.h"
#include "bulba/core/entity.h"
#include "bulba/core/utils/font.h"

#include <stdbool.h>

typedef struct BLB_Text2D {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;
  BLB_Material *material;
  HMM_Vec2 position;
  HMM_Vec2 scale;
  float rotation;

  float *delta_time;

  unsigned char color[4];

  float glow;
  float emission;
  float roundness;

  unsigned short layer;
  BLB_RenderMode render_mode;
  bool visible;

  char *text;
  char *font_path;
  float size;

  Font font;
  bool font_loaded;
} BLB_Text2D;

BLB_Text2D *BLB_CreateText2D(const char *text, const char *font_path, HMM_Vec2 position, float size);

void BLB_DestroyText2D(BLB_Text2D *text);

void BLB_SetText2D(BLB_Text2D *text, const char *value);

#endif
