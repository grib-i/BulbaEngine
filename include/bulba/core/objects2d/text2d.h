#ifndef BULBA_CORE_OBJECTS2D_TEXT2D_H
#define BULBA_CORE_OBJECTS2D_TEXT2D_H

#include "bulba/core/entity.h"
#include "bulba/core/math3v/math3v.h"
#include "bulba/core/render/material.h"
#include "bulba/core/render_mode.h"
#include "bulba/core/utils/font.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct BLB_Text2D {
  BLB_EntityId entity_id;
  BLB_ComponentMask component_mask;
  BLB_Material *material;

  BLB_Vec2 position;
  BLB_Vec2 scale;
  float rotation;

  float delta_time;

  unsigned char color[4];

  float glow;
  float emission;
  float roundness;

  unsigned short layer;
  BLB_RenderMode render_mode;

  bool visible;
  bool camera_attached;

  char *text;
  size_t text_capacity;

  char *font_path;
  float size;

  Font font;
  bool font_loaded;
} BLB_Text2D;

BLB_Text2D *BLB_CreateText2D(const char *text, const char *font_path, BLB_Vec2 position, float size, bool camera_attached);

void BLB_DestroyText2D(BLB_Text2D *text);

void BLB_SetText2D(BLB_Text2D *text, const char *value);

void BLB_Text2D_SetCameraAttached(BLB_Text2D *text, bool attached);
bool BLB_Text2D_IsCameraAttached(const BLB_Text2D *text);

#endif
