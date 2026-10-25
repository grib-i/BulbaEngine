#ifndef WINDOW_H
#define WINDOW_H

#include "bulba/handlers/handlers.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum BLB_CursorMode {
  BLB_CURSOR_NORMAL = 0x00034001,
  BLB_CURSOR_HIDDEN = 0x00034002,
  BLB_CURSOR_DISABLED = 0x00034003,
} BLB_CursorMode;

struct GLFWwindow;
typedef struct BLB_Scene BLB_Scene;
typedef struct BLB_Window {
  struct GLFWwindow *handle;
  BLB_Handlers handlers;
  int width, height;
  bool should_close, resized, fullscreen;
  bool f11_down;
  int windowed_x, windowed_y;
  int windowed_width, windowed_height;
  double resize_time;
  uint64_t resize_serial;
  char *title;
} BLB_Window;

BLB_Window *BLB_CreateWindow(int width, int height, const char *title);
void BLB_DestroyWindow(BLB_Window *window);

void BLB_WindowPollEvents(BLB_Window *window);
void BLB_WindowProcessHandlers(BLB_Window *window, BLB_Scene *scene);
void BLB_WindowSetHandlerMode(BLB_Window *window, BLB_HandlerMode mode);
BLB_HandlerMode BLB_WindowGetHandlerMode(const BLB_Window *window);
bool BLB_WindowNextEvent(BLB_Window *window, BLB_HandlerEvent *event);
size_t BLB_WindowPendingEvents(const BLB_Window *window);
uint64_t BLB_WindowDroppedEvents(const BLB_Window *window);
void BLB_WindowPresent(BLB_Window *window);

void BLB_WindowSetFullscreen(BLB_Window *window, bool fullscreen);
void BLB_WindowToggleFullscreen(BLB_Window *window);
bool BLB_WindowResizeStable(const BLB_Window *window, double quiet_seconds);
bool BLB_WindowShouldClose(const BLB_Window *window);
void BLB_WindowSetCursorMode(BLB_Window *window, BLB_CursorMode mode);

#endif
