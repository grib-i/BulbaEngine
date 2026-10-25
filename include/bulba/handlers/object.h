#ifndef BLB_HANDLERS_OBJECT_H
#define BLB_HANDLERS_OBJECT_H

#include "bulba/core/math3v/vec_math.h"

#include <stdbool.h>
#include <stdint.h>

struct BLB_Object2D;
struct BLB_Object3D;

typedef enum {
  BLB_OBJECT_HANDLER_CLICK = 0,
  BLB_OBJECT_HANDLER_HOLD = 1,
  BLB_OBJECT_HANDLER_PRESS = 2,
  BLB_OBJECT_HANDLER_RELEASE = 3,
  BLB_OBJECT_HANDLER_HOVER_ENTER = 4,
  BLB_OBJECT_HANDLER_HOVER_LEAVE = 5,
  BLB_OBJECT_HANDLER_DRAG_START = 6,
  BLB_OBJECT_HANDLER_DRAG = 7,
  BLB_OBJECT_HANDLER_DRAG_END = 8,
  BLB_OBJECT_HANDLER_CANCEL = 9
} BLB_ObjectHandlerEventType;

typedef enum { BLB_OBJECT_HANDLER_2D = 1, BLB_OBJECT_HANDLER_3D = 2 } BLB_ObjectHandlerObjectType;

typedef struct {
  BLB_ObjectHandlerEventType type;
  BLB_ObjectHandlerObjectType object_type;

  union {
    struct BLB_Object2D *object2d;
    struct BLB_Object3D *object3d;
    void *ptr;
  } object;

  int button;
  uint32_t mods;

  uint64_t frame;
  double time;

  double mouse_x;
  double mouse_y;

  double mouse_dx;
  double mouse_dy;

  BLB_Vec2 world_position;
  BLB_Vec2 screen_position;
  BLB_Vec3 world_point;
  BLB_Vec3 local_position;

  BLB_Vec3 ray_origin;
  BLB_Vec3 ray_direction;

  BLB_Vec3 hit_point;
  BLB_Vec3 hit_normal;

  float distance;
  float t;

  bool hovered;
  bool captured;
  bool inside;
  bool button_pressed;
  bool button_down;
  bool button_released;
  bool dragging;
  double hold_duration;
} BLB_ObjectEvent;

typedef void (*BLB_ObjectHandlerCallback)(const BLB_ObjectEvent *event, void *userdata);

typedef struct {
  BLB_ObjectHandlerCallback on_click;
  BLB_ObjectHandlerCallback on_hold;
  BLB_ObjectHandlerCallback on_press;
  BLB_ObjectHandlerCallback on_release;
  BLB_ObjectHandlerCallback on_hover_enter;
  BLB_ObjectHandlerCallback on_hover_leave;
  BLB_ObjectHandlerCallback on_drag_start;
  BLB_ObjectHandlerCallback on_drag;
  BLB_ObjectHandlerCallback on_drag_end;
  BLB_ObjectHandlerCallback on_cancel;

  void *userdata;

  const void *object_geometry;

  BLB_Vec3 object_bounds_min;
  BLB_Vec3 object_bounds_max;

  uint16_t captured_buttons;
  uint16_t down_buttons;
  uint16_t pressed_buttons;
  uint16_t released_buttons;

  bool hovered;
  bool enabled;
} BLB_ObjectHandler;

void BLB_ObjectHandlerInit(BLB_ObjectHandler *handler);
bool BLB_ObjectHandlerIsHovered(const BLB_ObjectHandler *handler);
bool BLB_ObjectHandlerIsDown(const BLB_ObjectHandler *handler, int button);
bool BLB_ObjectHandlerWasPressed(const BLB_ObjectHandler *handler, int button);
bool BLB_ObjectHandlerWasReleased(const BLB_ObjectHandler *handler, int button);
bool BLB_ObjectHandlerIsCaptured(const BLB_ObjectHandler *handler, int button);
BLB_ObjectEvent BLB_ObjectEventZero(void);

#endif
