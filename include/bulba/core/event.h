#ifndef BULBA_CORE_EVENT_H
#define BULBA_CORE_EVENT_H

#include "bulba/core/math3v/vec_math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct BLB_Window BLB_Window;
typedef struct BLB_Scene BLB_Scene;
typedef struct BLB_Object2D BLB_Object2D;
typedef struct BLB_Object3D BLB_Object3D;

typedef enum {
  BLB_EVENT_NONE = 0,
  BLB_EVENT_QUIT,
  BLB_EVENT_WINDOW_RESIZED,
  BLB_EVENT_WINDOW_FOCUS,
  BLB_EVENT_KEY,
  BLB_EVENT_TEXT_INPUT,
  BLB_EVENT_MOUSE_BUTTON,
  BLB_EVENT_MOUSE_MOVE,
  BLB_EVENT_MOUSE_SCROLL,
  BLB_EVENT_MOUSE_ENTER,
  BLB_EVENT_COUNT
} BLB_EventType;

typedef enum { BLB_INPUT_RELEASE = 0, BLB_INPUT_PRESS = 1, BLB_INPUT_REPEAT = 2 } BLB_InputAction;

typedef enum {
  BLB_OBJECT_EVENT_NONE = 0,
  BLB_OBJECT_EVENT_MOUSE_DOWN = 1,
  BLB_OBJECT_EVENT_MOUSE_UP = 2,
  BLB_OBJECT_EVENT_CLICK = 3
} BLB_ObjectEventType;

typedef enum { BLB_HANDLER_CALLBACK = 0, BLB_HANDLER_POLL = 1 } BLB_HandlerMode;

typedef struct {
  BLB_EventType type;
  uint64_t timestamp_us;

  int key;
  int scancode;
  int action;
  int mods;

  int button;

  double x;
  double y;
  double dx;
  double dy;
  double scroll_x;
  double scroll_y;

  uint32_t codepoint;

  int width;
  int height;
  bool focused;
  bool entered;

  BLB_Object2D *object2d;
  BLB_Object3D *object3d;
  BLB_ObjectEventType object_event;
  BLB_Vec3 world_position;
  float hit_distance;
} BLB_Event;

typedef void (*BLB_EventHandler)(BLB_Window *window, const BLB_Event *event, void *user_data);

typedef uint32_t BLB_ObjectEventMask;

#define BLB_OBJECT_EVENT_MOUSE_DOWN (1u << 0)
#define BLB_OBJECT_EVENT_MOUSE_UP (1u << 1)
#define BLB_OBJECT_EVENT_CLICK (1u << 2)

typedef void (*BLB_Object2DEventHandler)(BLB_Object2D *object, const BLB_Event *event, void *user_data);
typedef void (*BLB_Object3DEventHandler)(BLB_Object3D *object, const BLB_Event *event, void *user_data);

#endif
