#include "bulba/handlers/handlers.h"
#include "bulba/handlers/keyboard.h"
#include "bulba/handlers/mouse.h"

#include "bulba/core/window.h"

#include <GLFW/glfw3.h>
#include <string.h>

void BLB_HandlersQueueEvent(BLB_Handlers *handlers, BLB_HandlerEvent event) {
  if (!handlers)
    return;

  if (handlers->event_count == BLB_HANDLER_EVENT_QUEUE_CAPACITY) {
    handlers->event_read = (handlers->event_read + 1u) % BLB_HANDLER_EVENT_QUEUE_CAPACITY;
    --handlers->event_count;
    ++handlers->dropped_events;
  }
  event.serial = ++handlers->event_serial;
  handlers->events[handlers->event_write] = event;
  handlers->event_write = (handlers->event_write + 1u) % BLB_HANDLER_EVENT_QUEUE_CAPACITY;
  ++handlers->event_count;
}

static double event_time(void) { return glfwGetTime(); }

static void focus_cb(GLFWwindow *window, int focused) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers)
    return;
  handlers->focused = focused != 0;
  handlers->focus_changed = true;

  if (!handlers->focused) {
    for (size_t i = 0; i < BLB_HANDLER_KEY_COUNT; ++i) {
      handlers->keyboard[i].released = handlers->keyboard[i].released || handlers->keyboard[i].down;
      handlers->keyboard[i].down = false;
    }

    for (size_t i = 0; i < BLB_HANDLER_MOUSE_BUTTON_COUNT; ++i) {
      handlers->mouse_buttons[i].released = handlers->mouse_buttons[i].released || handlers->mouse_buttons[i].down;
      handlers->mouse_buttons[i].down = false;
    }
  }
  BLB_HandlersQueueEvent(handlers,
                         (BLB_HandlerEvent){.type = BLB_EVENT_WINDOW_FOCUS, .data.focus = {.focused = handlers->focused, .time = event_time()}});
}

void BLB_HandlersPushResize(BLB_Handlers *handlers, int width, int height) {
  if (!handlers)
    return;
  BLB_Window *owner = handlers->owner;

  if (owner) {
    owner->width = width;
    owner->height = height;
    owner->resized = true;
    owner->resize_time = event_time();
    owner->resize_serial++;

    if (!owner->fullscreen && width > 0 && height > 0) {
      owner->windowed_width = width;
      owner->windowed_height = height;

      if (owner->handle)
        glfwGetWindowPos(owner->handle, &owner->windowed_x, &owner->windowed_y);
    }
  }
  BLB_HandlersQueueEvent(
      handlers, (BLB_HandlerEvent){.type = BLB_EVENT_WINDOW_RESIZE, .data.resize = {.width = width, .height = height, .time = event_time()}});
}

static void close_cb(GLFWwindow *window) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers)
    return;
  handlers->should_close = true;
  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){.type = BLB_EVENT_QUIT, .data.quit = {.time = event_time()}});
}

int BLB_HandlersInit(BLB_Handlers *handlers, GLFWwindow *window) {
  if (!handlers || !window)
    return -1;
  memset(handlers, 0, sizeof(*handlers));
  handlers->window = window;
  handlers->mode = BLB_HANDLER_CALLBACK;
  handlers->focused = true;
  handlers->initialized = true;
  glfwSetWindowUserPointer(window, handlers);

  if (BLB_KeyboardAttach(handlers, window) != 0 || BLB_MouseAttach(handlers, window) != 0) {
    BLB_KeyboardDetach(handlers);
    BLB_MouseDetach(handlers);
    glfwSetWindowUserPointer(window, NULL);
    memset(handlers, 0, sizeof(*handlers));

    return -1;
  }
  glfwSetWindowFocusCallback(window, focus_cb);
  glfwSetWindowCloseCallback(window, close_cb);
  glfwGetCursorPos(window, &handlers->mouse_x, &handlers->mouse_y);
  handlers->mouse_inside = glfwGetWindowAttrib(window, GLFW_HOVERED) != 0;

  return 0;
}

void BLB_HandlersShutdown(BLB_Handlers *handlers) {
  if (!handlers)
    return;

  if (handlers->window) {
    BLB_KeyboardDetach(handlers);
    BLB_MouseDetach(handlers);
    glfwSetWindowFocusCallback(handlers->window, NULL);
    glfwSetFramebufferSizeCallback(handlers->window, NULL);
    glfwSetWindowCloseCallback(handlers->window, NULL);
    glfwSetWindowUserPointer(handlers->window, NULL);
  }
  memset(handlers, 0, sizeof(*handlers));
}

void BLB_HandlersSetMode(BLB_Handlers *handlers, BLB_HandlerMode mode) {
  if (!handlers)
    return;
  handlers->mode = mode == BLB_HANDLER_POLL ? BLB_HANDLER_POLL : BLB_HANDLER_CALLBACK;
}

BLB_HandlerMode BLB_HandlersGetMode(const BLB_Handlers *handlers) { return handlers ? handlers->mode : BLB_HANDLER_CALLBACK; }

void BLB_HandlersPoll(BLB_Handlers *handlers) {
  if (!handlers || !handlers->window)
    return;
  glfwPollEvents();
  handlers->should_close = handlers->should_close || glfwWindowShouldClose(handlers->window);
}

void BLB_HandlersProcess(BLB_Handlers *handlers) {
  if (!handlers || handlers->mode != BLB_HANDLER_CALLBACK)
    return;
  BLB_HandlerEvent event;

  while (BLB_HandlersNextEvent(handlers, &event)) {
    if (event.type <= BLB_EVENT_NONE || event.type > BLB_EVENT_WINDOW_RESIZE)
      continue;
    BLB_HandlerCallbackSlot *slots = handlers->callbacks[event.type];

    for (size_t i = 0; i < BLB_HANDLER_CALLBACK_CAPACITY; ++i)
      if (slots[i].callback)
        slots[i].callback(&event, slots[i].userdata);
  }
}

bool BLB_HandlersNextEvent(BLB_Handlers *handlers, BLB_HandlerEvent *event) {
  if (!handlers || !event || handlers->event_count == 0)
    return false;
  *event = handlers->events[handlers->event_read];
  handlers->event_read = (handlers->event_read + 1u) % BLB_HANDLER_EVENT_QUEUE_CAPACITY;
  --handlers->event_count;

  return true;
}

size_t BLB_HandlersPendingEvents(const BLB_Handlers *handlers) { return handlers ? handlers->event_count : 0; }

uint64_t BLB_HandlersDroppedEvents(const BLB_Handlers *handlers) { return handlers ? handlers->dropped_events : 0; }

bool BLB_HandlersSetCallback(BLB_Handlers *handlers, BLB_HandlerEventType type, BLB_HandlerCallback callback, void *userdata) {
  if (!handlers || type <= BLB_EVENT_NONE || type > BLB_EVENT_WINDOW_RESIZE || !callback)
    return false;
  BLB_HandlerCallbackSlot *slots = handlers->callbacks[type];

  for (size_t i = 0; i < BLB_HANDLER_CALLBACK_CAPACITY; ++i) {
    if (slots[i].callback == callback) {
      slots[i].userdata = userdata;

      return true;
    }
  }

  for (size_t i = 0; i < BLB_HANDLER_CALLBACK_CAPACITY; ++i) {
    if (!slots[i].callback) {
      slots[i].callback = callback;
      slots[i].userdata = userdata;

      return true;
    }
  }

  return false;
}

void BLB_HandlersClearCallbacks(BLB_Handlers *handlers, BLB_HandlerEventType type) {
  if (!handlers || type <= BLB_EVENT_NONE || type > BLB_EVENT_WINDOW_RESIZE)
    return;
  memset(handlers->callbacks[type], 0, sizeof(handlers->callbacks[type]));
}

void BLB_HandlersClearFrameTransitions(BLB_Handlers *handlers) {
  if (!handlers)
    return;

  for (size_t i = 0; i < BLB_HANDLER_KEY_COUNT; ++i) {
    handlers->keyboard[i].pressed = false;
    handlers->keyboard[i].released = false;
    handlers->keyboard[i].repeated = false;
  }

  for (size_t i = 0; i < BLB_HANDLER_MOUSE_BUTTON_COUNT; ++i) {
    handlers->mouse_buttons[i].pressed = false;
    handlers->mouse_buttons[i].released = false;
  }
  handlers->mouse_dx = 0.0;
  handlers->mouse_dy = 0.0;
  handlers->scroll_x = 0.0;
  handlers->scroll_y = 0.0;
  handlers->mouse_moved = false;
  handlers->focus_changed = false;
}

void BLB_HandlersBeginFrame(BLB_Handlers *handlers) { BLB_HandlersClearFrameTransitions(handlers); }
