#include "bulba/core/window.h"
#include "bulba/core/platform.h"
#include "bulba/core/utils/delta_time.h"
#include "bulba/handlers/keyboard.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>

static size_t blb_window_count;
static bool blb_glfw_initialized;

static void blb_glfw_release_if_unused(void) {
  if (blb_window_count == 0 && blb_glfw_initialized) {
    glfwTerminate();
    blb_glfw_initialized = false;
  }
}

static void framebuffer_resize_cb(GLFWwindow *window, int width, int height) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);
  BLB_HandlersPushResize(handlers, width, height);
}

BLB_Window *BLB_CreateWindow(int width, int height, const char *title) {
  if (width <= 0 || height <= 0)
    return NULL;

  if (!blb_glfw_initialized) {
    if (!glfwInit())
      return NULL;

    blb_glfw_initialized = true;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);
  BLB_Window *w = calloc(1, sizeof(*w));

  if (!w) {
    blb_glfw_release_if_unused();
    return NULL;
  }

  w->width = width;
  w->height = height;
  w->windowed_width = width;
  w->windowed_height = height;
  w->resize_time = glfwGetTime();
  w->title = BLB_Platform_DuplicateString(title ? title : "BulbaEngine");

  if (!w->title) {
    free(w);
    blb_glfw_release_if_unused();
    return NULL;
  }

  w->handle = glfwCreateWindow(width, height, w->title, NULL, NULL);

  if (!w->handle) {
    free(w->title);
    free(w);
    blb_glfw_release_if_unused();
    return NULL;
  }

  if (BLB_HandlersInit(&w->handlers, w->handle) != 0) {
    glfwDestroyWindow(w->handle);
    free(w->title);
    free(w);
    blb_glfw_release_if_unused();
    return NULL;
  }

  w->handlers.owner = w;

  glfwSetFramebufferSizeCallback(w->handle, framebuffer_resize_cb);
  glfwGetWindowPos(w->handle, &w->windowed_x, &w->windowed_y);
  ++blb_window_count;

  return w;
}

void BLB_DestroyWindow(BLB_Window *w) {
  if (!w)
    return;

  if (w->handle) {
    BLB_HandlersShutdown(&w->handlers);
    glfwDestroyWindow(w->handle);
    if (blb_window_count > 0)
      --blb_window_count;
  }

  free(w->title);
  free(w);
  blb_glfw_release_if_unused();
}

void BLB_WindowSetFullscreen(BLB_Window *w, bool fullscreen) {
  if (!w || !w->handle || w->fullscreen == fullscreen)
    return;

  if (fullscreen) {
    glfwGetWindowPos(w->handle, &w->windowed_x, &w->windowed_y);
    glfwGetWindowSize(w->handle, &w->windowed_width, &w->windowed_height);

    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = monitor ? glfwGetVideoMode(monitor) : NULL;

    if (!monitor || !mode)
      return;

    w->fullscreen = true;
    glfwSetWindowMonitor(w->handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
  } else {
    int width = w->windowed_width > 0 ? w->windowed_width : w->width;
    int height = w->windowed_height > 0 ? w->windowed_height : w->height;
    w->fullscreen = false;
    glfwSetWindowMonitor(w->handle, NULL, w->windowed_x, w->windowed_y, width, height, 0);
  }

  w->resized = true;
  w->resize_time = glfwGetTime();
  w->resize_serial++;
}

void BLB_WindowToggleFullscreen(BLB_Window *w) {
  if (!w)
    return;

  BLB_WindowSetFullscreen(w, !w->fullscreen);
}

bool BLB_WindowResizeStable(const BLB_Window *w, double quiet_seconds) {
  if (!w || !w->resized || w->width <= 0 || w->height <= 0)
    return false;

  if (quiet_seconds < 0.0)
    quiet_seconds = 0.0;

  double elapsed = glfwGetTime() - w->resize_time;

  return elapsed >= quiet_seconds;
}

void BLB_WindowPollEvents(BLB_Window *w) {
  if (!w)
    return;

  BLB_DeltaTime_Update();
  BLB_HandlersBeginFrame(&w->handlers);
  BLB_HandlersPoll(&w->handlers);

  int f11 = BLB_KeyboardDown(&w->handlers, GLFW_KEY_F11);

  if (f11 && !w->f11_down)
    BLB_WindowToggleFullscreen(w);

  w->f11_down = f11;

  w->should_close = w->handlers.should_close || glfwWindowShouldClose(w->handle);
}

void BLB_WindowProcessHandlers(BLB_Window *w, BLB_Scene *scene) {
  if (!w)
    return;

  BLB_HandlersProcess(&w->handlers);
  BLB_HandlersProcessObjects(&w->handlers, scene);
}

void BLB_WindowSetHandlerMode(BLB_Window *w, BLB_HandlerMode mode) {
  if (!w)
    return;

  BLB_HandlersSetMode(&w->handlers, mode);
}

BLB_HandlerMode BLB_WindowGetHandlerMode(const BLB_Window *w) {
  return w ? BLB_HandlersGetMode(&w->handlers) : BLB_HANDLER_CALLBACK;
}

bool BLB_WindowNextEvent(BLB_Window *w, BLB_HandlerEvent *event) {
  return w ? BLB_HandlersNextEvent(&w->handlers, event) : false;
}

size_t BLB_WindowPendingEvents(const BLB_Window *w) {
  return w ? BLB_HandlersPendingEvents(&w->handlers) : 0;
}

uint64_t BLB_WindowDroppedEvents(const BLB_Window *w) {
  return w ? BLB_HandlersDroppedEvents(&w->handlers) : 0;
}

void BLB_WindowPresent(BLB_Window *w) {
  (void)w;
}

bool BLB_WindowShouldClose(const BLB_Window *w) {
  return w ? w->should_close : true;
}

void BLB_WindowSetCursorMode(BLB_Window *window, BLB_CursorMode mode) {
  if (!window || !window->handle)
    return;

  if (mode != BLB_CURSOR_NORMAL && mode != BLB_CURSOR_HIDDEN && mode != BLB_CURSOR_DISABLED)
    mode = BLB_CURSOR_NORMAL;

  glfwSetInputMode(window->handle, GLFW_CURSOR, mode);
}
