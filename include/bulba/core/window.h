#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>
#include <stdint.h>

struct GLFWwindow;
typedef struct BLB_Window {
  struct GLFWwindow *handle;
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
void BLB_WindowPresent(BLB_Window *window);

void BLB_WindowSetFullscreen(BLB_Window *window, bool fullscreen);
void BLB_WindowToggleFullscreen(BLB_Window *window);
bool BLB_WindowResizeStable(const BLB_Window *window, double quiet_seconds);
bool BLB_WindowShouldClose(const BLB_Window *window);

#endif
