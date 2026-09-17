#include "bulba/core/window.h"
#include "bulba/core/platform.h"
#include <GLFW/glfw3.h>
#include <stdlib.h>

static void resize_cb(GLFWwindow *w, int width, int height) {
  BLB_Window *x = glfwGetWindowUserPointer(w);
  if (x) {
    x->width = width;
    x->height = height;
    x->resized = true;
  }
}

BLB_Window *BLB_CreateWindow(int width, int height, const char *title) {
  if (!glfwInit())
    return NULL;
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  BLB_Window *w = calloc(1, sizeof(*w));
  if (!w) {
    glfwTerminate();
    return NULL;
  }
  w->width = width;
  w->height = height;
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

void BLB_WindowPollEvents(BLB_Window *w) {
  if (!w)
    return;
  glfwPollEvents();
  w->should_close = glfwWindowShouldClose(w->handle);
}

void BLB_WindowPresent(BLB_Window *w) { (void)w; }

bool BLB_WindowShouldClose(const BLB_Window *w) { return w ? w->should_close : true; }
