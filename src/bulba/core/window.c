#include "bulba/core/window.h"
#include "bulba/core/platform.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>

static void resize_cb(GLFWwindow *w, int width, int height) {
  BLB_Window *x = glfwGetWindowUserPointer(w);
  if (!x)
    return;

  x->width = width;
  x->height = height;
  x->resized = true;
  x->resize_time = glfwGetTime();
  x->resize_serial++;

  if (!x->fullscreen && width > 0 && height > 0) {
    x->windowed_width = width;
    x->windowed_height = height;
    glfwGetWindowPos(w, &x->windowed_x, &x->windowed_y);
  }
}

BLB_Window *BLB_CreateWindow(int width, int height, const char *title) {
  if (!glfwInit())
    return NULL;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  BLB_Window *w = calloc(1, sizeof(*w));
  if (!w) {
    glfwTerminate();
    return NULL;
  }
  w->width = width;
  w->height = height;
  w->windowed_width = width;
  w->windowed_height = height;
  w->resize_time = glfwGetTime();
  w->title = BLB_Platform_DuplicateString(title ? title : "BulbaEngine");
  w->handle = glfwCreateWindow(width, height, w->title, NULL, NULL);
  if (!w->handle) {
    free(w->title);
    free(w);
    glfwTerminate();
    return NULL;
  }
  glfwSetWindowUserPointer(w->handle, w);
  glfwSetFramebufferSizeCallback(w->handle, resize_cb);
  glfwGetWindowPos(w->handle, &w->windowed_x, &w->windowed_y);
  return w;
}

void BLB_DestroyWindow(BLB_Window *w) {
  if (!w)
    return;
  if (w->handle)
    glfwDestroyWindow(w->handle);
  free(w->title);
  free(w);
  glfwTerminate();
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

  glfwPollEvents();

  int f11 = glfwGetKey(w->handle, GLFW_KEY_F11) == GLFW_PRESS;
  if (f11 && !w->f11_down)
    BLB_WindowToggleFullscreen(w);
  w->f11_down = f11;

  w->should_close = glfwWindowShouldClose(w->handle);
}

void BLB_WindowPresent(BLB_Window *w) { (void)w; }

bool BLB_WindowShouldClose(const BLB_Window *w) { return w ? w->should_close : true; }
