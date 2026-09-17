#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

struct GLFWwindow;
typedef struct BLB_Window {
  struct GLFWwindow *handle;
  int width, height;
  bool should_close, resized;
  char *title;
} BLB_Window;

BLB_Window *BLB_CreateWindow(int width, int height, const char *title);
void BLB_DestroyWindow(BLB_Window *window);

void BLB_WindowPollEvents(BLB_Window *window);
void BLB_WindowPresent(BLB_Window *window);
bool BLB_WindowShouldClose(const BLB_Window *window);

#endif
