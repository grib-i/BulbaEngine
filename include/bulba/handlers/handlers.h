#ifndef BLB_HANDLERS_H
#define BLB_HANDLERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct GLFWwindow;
typedef struct BLB_Scene BLB_Scene;
typedef struct BLB_Object2D BLB_Object2D;
typedef struct BLB_Object3D BLB_Object3D;

#define BLB_HANDLER_EVENT_QUEUE_CAPACITY 2048u
#define BLB_HANDLER_CALLBACK_CAPACITY 8u
#define BLB_HANDLER_KEY_COUNT 512u
#define BLB_HANDLER_MOUSE_BUTTON_COUNT 16u
#define BLB_HANDLER_DRAG_THRESHOLD 4.0

typedef enum { BLB_HANDLER_CALLBACK = 0, BLB_HANDLER_POLL = 1 } BLB_HandlerMode;

typedef enum {
  BLB_EVENT_NONE = 0,
  BLB_EVENT_QUIT,
  BLB_EVENT_KEY,
  BLB_EVENT_MOUSE_BUTTON,
  BLB_EVENT_MOUSE_MOVE,
  BLB_EVENT_MOUSE_SCROLL,
  BLB_EVENT_MOUSE_ENTER,
  BLB_EVENT_TEXT,
  BLB_EVENT_WINDOW_FOCUS,
  BLB_EVENT_WINDOW_RESIZE
} BLB_HandlerEventType;

typedef enum { BLB_INPUT_RELEASE = 0, BLB_INPUT_PRESS = 1, BLB_INPUT_REPEAT = 2, BLB_INPUT_IDLE = 3, BLB_INPUT_HOLD = 4 } BLB_InputAction;

typedef int BLB_Key;

enum {
  BLB_KEY_UNKNOWN = 0,
  BLB_KEY_ESCAPE = 0x1000,
  BLB_KEY_ENTER,
  BLB_KEY_TAB,
  BLB_KEY_BACKSPACE,
  BLB_KEY_INSERT,
  BLB_KEY_DELETE,
  BLB_KEY_RIGHT,
  BLB_KEY_LEFT,
  BLB_KEY_DOWN,
  BLB_KEY_UP,
  BLB_KEY_PAGE_UP,
  BLB_KEY_PAGE_DOWN,
  BLB_KEY_HOME,
  BLB_KEY_END,
  BLB_KEY_CAPS_LOCK,
  BLB_KEY_SCROLL_LOCK,
  BLB_KEY_NUM_LOCK,
  BLB_KEY_PRINT_SCREEN,
  BLB_KEY_PAUSE,
  BLB_KEY_SPACE,

  BLB_KEY_F1,
  BLB_KEY_F2,
  BLB_KEY_F3,
  BLB_KEY_F4,
  BLB_KEY_F5,
  BLB_KEY_F6,
  BLB_KEY_F7,
  BLB_KEY_F8,
  BLB_KEY_F9,
  BLB_KEY_F10,
  BLB_KEY_F11,
  BLB_KEY_F12,
  BLB_KEY_F13,
  BLB_KEY_F14,
  BLB_KEY_F15,
  BLB_KEY_F16,
  BLB_KEY_F17,
  BLB_KEY_F18,
  BLB_KEY_F19,
  BLB_KEY_F20,
  BLB_KEY_F21,
  BLB_KEY_F22,
  BLB_KEY_F23,
  BLB_KEY_F24,
  BLB_KEY_F25,

  BLB_KEY_KP_0,
  BLB_KEY_KP_1,
  BLB_KEY_KP_2,
  BLB_KEY_KP_3,
  BLB_KEY_KP_4,
  BLB_KEY_KP_5,
  BLB_KEY_KP_6,
  BLB_KEY_KP_7,
  BLB_KEY_KP_8,
  BLB_KEY_KP_9,
  BLB_KEY_KP_DECIMAL,
  BLB_KEY_KP_DIVIDE,
  BLB_KEY_KP_MULTIPLY,
  BLB_KEY_KP_SUBTRACT,
  BLB_KEY_KP_ADD,
  BLB_KEY_KP_ENTER,
  BLB_KEY_KP_EQUAL,

  BLB_KEY_LEFT_SHIFT,
  BLB_KEY_LEFT_CONTROL,
  BLB_KEY_LEFT_ALT,
  BLB_KEY_LEFT_SUPER,
  BLB_KEY_RIGHT_SHIFT,
  BLB_KEY_RIGHT_CONTROL,
  BLB_KEY_RIGHT_ALT,
  BLB_KEY_RIGHT_SUPER,
  BLB_KEY_MENU
};

typedef struct {
  BLB_Key key;
  int native_key;
  int scancode;
  uint32_t mods;
  BLB_InputAction action;
  bool down;
  bool pressed;
  bool released;
  bool repeated;
  double time;
} BLB_KeyEvent;

typedef struct {
  int button;
  uint32_t mods;
  BLB_InputAction action;
  double x;
  double y;
  double delta_x;
  double delta_y;
  bool down;
  bool pressed;
  bool released;
  double time;
} BLB_MouseButtonEvent;

typedef struct {
  double x;
  double y;
  double delta_x;
  double delta_y;
  double time;
} BLB_MouseMoveEvent;

typedef struct {
  double x_offset;
  double y_offset;
  double x;
  double y;
  double time;
} BLB_MouseScrollEvent;

typedef struct {
  bool inside;
  double time;
} BLB_MouseEnterEvent;

typedef struct {
  uint32_t codepoint;
  double time;
} BLB_TextEvent;

typedef struct {
  BLB_HandlerEventType type;
  uint64_t serial;
  union {
    BLB_KeyEvent key;
    BLB_MouseButtonEvent mouse_button;
    BLB_MouseMoveEvent mouse_move;
    BLB_MouseScrollEvent mouse_scroll;
    BLB_MouseEnterEvent mouse_enter;
    BLB_TextEvent text;
    struct {
      bool focused;
      double time;
    } focus;
    struct {
      int width;
      int height;
      double time;
    } resize;
    struct {
      double time;
    } quit;
  } data;
} BLB_HandlerEvent;

typedef struct {
  bool down;
  bool pressed;
  bool released;
  bool repeated;
  int scancode;
  uint32_t mods;
  double last_change_time;
} BLB_KeyState;

typedef struct {
  bool down;
  bool pressed;
  bool released;
  uint32_t mods;
  double last_change_time;
} BLB_MouseButtonState;

typedef struct {
  bool active;
  bool started;
  void *object;
  uint8_t object_type;

  double press_mouse_x;
  double press_mouse_y;

  float start_point[3];
  float last_point[3];

  float plane_point[3];
  float plane_normal[3];

  float grab_offset[3];
} BLB_ObjectDragState;

typedef void (*BLB_HandlerCallback)(const BLB_HandlerEvent *event, void *userdata);

typedef struct {
  BLB_HandlerCallback callback;
  void *userdata;
} BLB_HandlerCallbackSlot;

typedef struct BLB_Handlers {
  struct GLFWwindow *window;
  void *owner;

  BLB_HandlerMode mode;

  BLB_HandlerEvent events[BLB_HANDLER_EVENT_QUEUE_CAPACITY];
  size_t event_read;
  size_t event_write;
  size_t event_count;
  uint64_t event_serial;

  BLB_HandlerCallbackSlot callbacks[BLB_EVENT_WINDOW_RESIZE + 1][BLB_HANDLER_CALLBACK_CAPACITY];

  BLB_KeyState keyboard[BLB_HANDLER_KEY_COUNT];
  BLB_MouseButtonState mouse_buttons[BLB_HANDLER_MOUSE_BUTTON_COUNT];

  double mouse_x;
  double mouse_y;
  double mouse_dx;
  double mouse_dy;

  double scroll_x;
  double scroll_y;

  bool mouse_inside;
  bool mouse_moved;

  bool focus_changed;
  bool focused;
  bool should_close;

  void *hovered_object;
  uint8_t hovered_object_type;

  uint64_t dropped_events;

  void *captured_objects[BLB_HANDLER_MOUSE_BUTTON_COUNT];
  uint8_t captured_object_types[BLB_HANDLER_MOUSE_BUTTON_COUNT];

  BLB_ObjectDragState object_drags[BLB_HANDLER_MOUSE_BUTTON_COUNT];

  bool object_hover_initialized;
  bool initialized;
} BLB_Handlers;

int BLB_HandlersInit(BLB_Handlers *handlers, struct GLFWwindow *window);
void BLB_HandlersShutdown(BLB_Handlers *handlers);

void BLB_HandlersSetMode(BLB_Handlers *handlers, BLB_HandlerMode mode);
BLB_HandlerMode BLB_HandlersGetMode(const BLB_Handlers *handlers);

void BLB_HandlersPoll(BLB_Handlers *handlers);
void BLB_HandlersPushResize(BLB_Handlers *handlers, int width, int height);
void BLB_HandlersProcess(BLB_Handlers *handlers);

bool BLB_HandlersNextEvent(BLB_Handlers *handlers, BLB_HandlerEvent *event);

size_t BLB_HandlersPendingEvents(const BLB_Handlers *handlers);
uint64_t BLB_HandlersDroppedEvents(const BLB_Handlers *handlers);

bool BLB_HandlersSetCallback(BLB_Handlers *handlers, BLB_HandlerEventType type, BLB_HandlerCallback callback, void *userdata);
void BLB_HandlersClearCallbacks(BLB_Handlers *handlers, BLB_HandlerEventType type);

void BLB_HandlersClearFrameTransitions(BLB_Handlers *handlers);
void BLB_HandlersBeginFrame(BLB_Handlers *handlers);

void BLB_HandlersQueueEvent(BLB_Handlers *handlers, BLB_HandlerEvent event);
void BLB_HandlersProcessObjects(BLB_Handlers *handlers, BLB_Scene *scene);

#endif
