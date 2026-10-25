#include "bulba/handlers/mouse.h"

#include <GLFW/glfw3.h>
#include <stddef.h>

static double event_time(void) { return glfwGetTime(); }

static void mouse_button_cb(GLFWwindow *window, int button, int action, int mods) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers || button < 0 || (unsigned)button >= BLB_HANDLER_MOUSE_BUTTON_COUNT)
    return;

  BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];
  const double time = event_time();
  state->mods = (uint32_t)mods;
  state->last_change_time = time;
  state->pressed = state->pressed || action == GLFW_PRESS;
  state->released = state->released || action == GLFW_RELEASE;
  state->down = action == GLFW_PRESS ? true : (action == GLFW_RELEASE ? false : state->down);

  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){
                                       .type = BLB_EVENT_MOUSE_BUTTON,
                                       .data.mouse_button =
                                           {
                                               .button = button,
                                               .mods = (uint32_t)mods,
                                               .action = action == GLFW_PRESS ? BLB_INPUT_PRESS : BLB_INPUT_RELEASE,
                                               .x = handlers->mouse_x,
                                               .y = handlers->mouse_y,
                                               .delta_x = handlers->mouse_dx,
                                               .delta_y = handlers->mouse_dy,
                                               .down = state->down,
                                               .pressed = state->pressed,
                                               .released = state->released,
                                               .time = time,
                                           },
                                   });
}

static void cursor_pos_cb(GLFWwindow *window, double x, double y) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers)
    return;

  const double dx = x - handlers->mouse_x;
  const double dy = y - handlers->mouse_y;
  handlers->mouse_dx += dx;
  handlers->mouse_dy += dy;
  handlers->mouse_x = x;
  handlers->mouse_y = y;
  handlers->mouse_moved = true;
  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){
                                       .type = BLB_EVENT_MOUSE_MOVE,
                                       .data.mouse_move = {.x = x, .y = y, .delta_x = dx, .delta_y = dy, .time = event_time()},
                                   });
}

static void scroll_cb(GLFWwindow *window, double x_offset, double y_offset) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers)
    return;

  handlers->scroll_x += x_offset;
  handlers->scroll_y += y_offset;
  BLB_HandlersQueueEvent(
      handlers,
      (BLB_HandlerEvent){
          .type = BLB_EVENT_MOUSE_SCROLL,
          .data.mouse_scroll = {.x_offset = x_offset, .y_offset = y_offset, .x = handlers->mouse_x, .y = handlers->mouse_y, .time = event_time()},
      });
}

static void cursor_enter_cb(GLFWwindow *window, int entered) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers)
    return;

  handlers->mouse_inside = entered != 0;
  handlers->mouse_moved = true;
  BLB_HandlersQueueEvent(
      handlers, (BLB_HandlerEvent){.type = BLB_EVENT_MOUSE_ENTER, .data.mouse_enter = {.inside = handlers->mouse_inside, .time = event_time()}});
}

int BLB_MouseAttach(BLB_Handlers *handlers, GLFWwindow *window) {
  if (!handlers || !window)
    return -1;

  glfwSetMouseButtonCallback(window, mouse_button_cb);
  glfwSetCursorPosCallback(window, cursor_pos_cb);
  glfwSetScrollCallback(window, scroll_cb);
  glfwSetCursorEnterCallback(window, cursor_enter_cb);
  glfwGetCursorPos(window, &handlers->mouse_x, &handlers->mouse_y);
  handlers->mouse_inside = glfwGetWindowAttrib(window, GLFW_HOVERED) != 0;

  return 0;
}

void BLB_MouseDetach(BLB_Handlers *handlers) {
  if (!handlers || !handlers->window)
    return;

  glfwSetMouseButtonCallback(handlers->window, NULL);
  glfwSetCursorPosCallback(handlers->window, NULL);
  glfwSetScrollCallback(handlers->window, NULL);
  glfwSetCursorEnterCallback(handlers->window, NULL);
}

static bool valid_button(int button) { return button >= 0 && (unsigned)button < BLB_HANDLER_MOUSE_BUTTON_COUNT; }

const BLB_MouseButtonState *BLB_MouseButtonGet(const BLB_Handlers *handlers, int button) {
  if (!handlers || !valid_button(button))
    return NULL;

  return &handlers->mouse_buttons[(size_t)button];
}

BLB_MouseButtonEvent BLB_MouseButtonInfo(const BLB_Handlers *handlers, int button) {
  BLB_MouseButtonEvent event = {0};

  if (!handlers || !valid_button(button))
    return event;

  const BLB_MouseButtonState *state = &handlers->mouse_buttons[(size_t)button];
  event.button = button;
  event.mods = state->mods;
  event.action = state->pressed ? BLB_INPUT_PRESS : (state->released ? BLB_INPUT_RELEASE : (state->down ? BLB_INPUT_HOLD : BLB_INPUT_IDLE));
  event.x = handlers->mouse_x;
  event.y = handlers->mouse_y;
  event.delta_x = handlers->mouse_dx;
  event.delta_y = handlers->mouse_dy;
  event.down = state->down;
  event.pressed = state->pressed;
  event.released = state->released;
  event.time = state->last_change_time;

  return event;
}

BLB_MouseInfo BLB_MouseGet(const BLB_Handlers *handlers) {
  BLB_MouseInfo info = {0};

  if (!handlers)
    return info;

  info.x = handlers->mouse_x;
  info.y = handlers->mouse_y;
  info.delta_x = handlers->mouse_dx;
  info.delta_y = handlers->mouse_dy;
  info.scroll_x = handlers->scroll_x;
  info.scroll_y = handlers->scroll_y;
  info.inside = handlers->mouse_inside;
  info.moved = handlers->mouse_moved;

  for (size_t i = 0; i < BLB_HANDLER_MOUSE_BUTTON_COUNT; ++i) {
    const BLB_MouseButtonState *state = &handlers->mouse_buttons[i];

    if (state->down)
      info.down_mask |= (uint16_t)(1u << i);

    if (state->pressed)
      info.pressed_mask |= (uint16_t)(1u << i);

    if (state->released)
      info.released_mask |= (uint16_t)(1u << i);
  }

  return info;
}

bool BLB_MouseDown(const BLB_Handlers *handlers, int button) {
  const BLB_MouseButtonState *state = BLB_MouseButtonGet(handlers, button);

  return state ? state->down : false;
}

bool BLB_MouseHeld(const BLB_Handlers *handlers, int button) {
  const BLB_MouseButtonState *state = BLB_MouseButtonGet(handlers, button);

  return state ? state->down && !state->pressed && !state->released : false;
}

bool BLB_MousePressed(const BLB_Handlers *handlers, int button) {
  const BLB_MouseButtonState *state = BLB_MouseButtonGet(handlers, button);

  return state ? state->pressed : false;
}

bool BLB_MouseReleased(const BLB_Handlers *handlers, int button) {
  const BLB_MouseButtonState *state = BLB_MouseButtonGet(handlers, button);

  return state ? state->released : false;
}

bool BLB_MouseSetButtonHandler(BLB_Handlers *handlers, BLB_HandlerCallback callback, void *userdata) {
  return BLB_HandlersSetCallback(handlers, BLB_EVENT_MOUSE_BUTTON, callback, userdata);
}

bool BLB_MouseSetMoveHandler(BLB_Handlers *handlers, BLB_HandlerCallback callback, void *userdata) {
  return BLB_HandlersSetCallback(handlers, BLB_EVENT_MOUSE_MOVE, callback, userdata);
}

bool BLB_MouseSetScrollHandler(BLB_Handlers *handlers, BLB_HandlerCallback callback, void *userdata) {
  return BLB_HandlersSetCallback(handlers, BLB_EVENT_MOUSE_SCROLL, callback, userdata);
}
