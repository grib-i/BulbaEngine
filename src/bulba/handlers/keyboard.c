#include "bulba/handlers/keyboard.h"

#include <GLFW/glfw3.h>
#include <ctype.h>

static double event_time(void) { return glfwGetTime(); }

static int key_to_native(BLB_Key key) {
  const int value = key;

  if (value >= 'a' && value <= 'z')
    return value - 'a' + 'A';

  if (value >= 'A' && value <= 'Z')
    return value;

  if (value >= 32 && value <= 126)
    return value;

  switch (value) {
  case BLB_KEY_ESCAPE:
    return GLFW_KEY_ESCAPE;

  case BLB_KEY_ENTER:
    return GLFW_KEY_ENTER;

  case BLB_KEY_TAB:
    return GLFW_KEY_TAB;

  case BLB_KEY_BACKSPACE:
    return GLFW_KEY_BACKSPACE;

  case BLB_KEY_INSERT:
    return GLFW_KEY_INSERT;

  case BLB_KEY_DELETE:
    return GLFW_KEY_DELETE;

  case BLB_KEY_RIGHT:
    return GLFW_KEY_RIGHT;

  case BLB_KEY_LEFT:
    return GLFW_KEY_LEFT;

  case BLB_KEY_DOWN:
    return GLFW_KEY_DOWN;

  case BLB_KEY_UP:
    return GLFW_KEY_UP;

  case BLB_KEY_PAGE_UP:
    return GLFW_KEY_PAGE_UP;

  case BLB_KEY_PAGE_DOWN:
    return GLFW_KEY_PAGE_DOWN;

  case BLB_KEY_HOME:
    return GLFW_KEY_HOME;

  case BLB_KEY_END:
    return GLFW_KEY_END;

  case BLB_KEY_CAPS_LOCK:
    return GLFW_KEY_CAPS_LOCK;

  case BLB_KEY_SCROLL_LOCK:
    return GLFW_KEY_SCROLL_LOCK;

  case BLB_KEY_NUM_LOCK:
    return GLFW_KEY_NUM_LOCK;

  case BLB_KEY_PRINT_SCREEN:
    return GLFW_KEY_PRINT_SCREEN;

  case BLB_KEY_PAUSE:
    return GLFW_KEY_PAUSE;

  case BLB_KEY_SPACE:
    return GLFW_KEY_SPACE;

  case BLB_KEY_F1:
  case BLB_KEY_F2:
  case BLB_KEY_F3:
  case BLB_KEY_F4:
  case BLB_KEY_F5:
  case BLB_KEY_F6:
  case BLB_KEY_F7:
  case BLB_KEY_F8:
  case BLB_KEY_F9:
  case BLB_KEY_F10:
  case BLB_KEY_F11:
  case BLB_KEY_F12:
  case BLB_KEY_F13:
  case BLB_KEY_F14:
  case BLB_KEY_F15:
  case BLB_KEY_F16:
  case BLB_KEY_F17:
  case BLB_KEY_F18:
  case BLB_KEY_F19:
  case BLB_KEY_F20:
  case BLB_KEY_F21:
  case BLB_KEY_F22:
  case BLB_KEY_F23:
  case BLB_KEY_F24:
  case BLB_KEY_F25:
    return GLFW_KEY_F1 + (value - BLB_KEY_F1);

  case BLB_KEY_KP_0:
  case BLB_KEY_KP_1:
  case BLB_KEY_KP_2:
  case BLB_KEY_KP_3:
  case BLB_KEY_KP_4:
  case BLB_KEY_KP_5:
  case BLB_KEY_KP_6:
  case BLB_KEY_KP_7:
  case BLB_KEY_KP_8:
  case BLB_KEY_KP_9:
    return GLFW_KEY_KP_0 + (value - BLB_KEY_KP_0);

  case BLB_KEY_KP_DECIMAL:
    return GLFW_KEY_KP_DECIMAL;

  case BLB_KEY_KP_DIVIDE:
    return GLFW_KEY_KP_DIVIDE;

  case BLB_KEY_KP_MULTIPLY:
    return GLFW_KEY_KP_MULTIPLY;

  case BLB_KEY_KP_SUBTRACT:
    return GLFW_KEY_KP_SUBTRACT;

  case BLB_KEY_KP_ADD:
    return GLFW_KEY_KP_ADD;

  case BLB_KEY_KP_ENTER:
    return GLFW_KEY_KP_ENTER;

  case BLB_KEY_KP_EQUAL:
    return GLFW_KEY_KP_EQUAL;

  case BLB_KEY_LEFT_SHIFT:
    return GLFW_KEY_LEFT_SHIFT;

  case BLB_KEY_LEFT_CONTROL:
    return GLFW_KEY_LEFT_CONTROL;

  case BLB_KEY_LEFT_ALT:
    return GLFW_KEY_LEFT_ALT;

  case BLB_KEY_LEFT_SUPER:
    return GLFW_KEY_LEFT_SUPER;

  case BLB_KEY_RIGHT_SHIFT:
    return GLFW_KEY_RIGHT_SHIFT;

  case BLB_KEY_RIGHT_CONTROL:
    return GLFW_KEY_RIGHT_CONTROL;

  case BLB_KEY_RIGHT_ALT:
    return GLFW_KEY_RIGHT_ALT;

  case BLB_KEY_RIGHT_SUPER:
    return GLFW_KEY_RIGHT_SUPER;

  case BLB_KEY_MENU:
    return GLFW_KEY_MENU;

  default:
    break;
  }

  if (value > 0 && value < (int)BLB_HANDLER_KEY_COUNT)
    return value;

  return GLFW_KEY_UNKNOWN;
}

static BLB_Key native_to_key(int key) {
  if (key >= 'A' && key <= 'Z')
    return key;

  if (key >= '0' && key <= '9')
    return key;

  if (key >= 32 && key <= 126)
    return key;

  switch (key) {
  case GLFW_KEY_ESCAPE:
    return BLB_KEY_ESCAPE;

  case GLFW_KEY_ENTER:
    return BLB_KEY_ENTER;

  case GLFW_KEY_TAB:
    return BLB_KEY_TAB;

  case GLFW_KEY_BACKSPACE:
    return BLB_KEY_BACKSPACE;

  case GLFW_KEY_INSERT:
    return BLB_KEY_INSERT;

  case GLFW_KEY_DELETE:
    return BLB_KEY_DELETE;

  case GLFW_KEY_RIGHT:
    return BLB_KEY_RIGHT;

  case GLFW_KEY_LEFT:
    return BLB_KEY_LEFT;

  case GLFW_KEY_DOWN:
    return BLB_KEY_DOWN;

  case GLFW_KEY_UP:
    return BLB_KEY_UP;

  case GLFW_KEY_PAGE_UP:
    return BLB_KEY_PAGE_UP;

  case GLFW_KEY_PAGE_DOWN:
    return BLB_KEY_PAGE_DOWN;

  case GLFW_KEY_HOME:
    return BLB_KEY_HOME;

  case GLFW_KEY_END:
    return BLB_KEY_END;

  case GLFW_KEY_CAPS_LOCK:
    return BLB_KEY_CAPS_LOCK;

  case GLFW_KEY_SCROLL_LOCK:
    return BLB_KEY_SCROLL_LOCK;

  case GLFW_KEY_NUM_LOCK:
    return BLB_KEY_NUM_LOCK;

  case GLFW_KEY_PRINT_SCREEN:
    return BLB_KEY_PRINT_SCREEN;

  case GLFW_KEY_PAUSE:
    return BLB_KEY_PAUSE;

  case GLFW_KEY_SPACE:
    return BLB_KEY_SPACE;

  case GLFW_KEY_F1:
  case GLFW_KEY_F2:
  case GLFW_KEY_F3:
  case GLFW_KEY_F4:
  case GLFW_KEY_F5:
  case GLFW_KEY_F6:
  case GLFW_KEY_F7:
  case GLFW_KEY_F8:
  case GLFW_KEY_F9:
  case GLFW_KEY_F10:
  case GLFW_KEY_F11:
  case GLFW_KEY_F12:
  case GLFW_KEY_F13:
  case GLFW_KEY_F14:
  case GLFW_KEY_F15:
  case GLFW_KEY_F16:
  case GLFW_KEY_F17:
  case GLFW_KEY_F18:
  case GLFW_KEY_F19:
  case GLFW_KEY_F20:
  case GLFW_KEY_F21:
  case GLFW_KEY_F22:
  case GLFW_KEY_F23:
  case GLFW_KEY_F24:
  case GLFW_KEY_F25:
    return BLB_KEY_F1 + (key - GLFW_KEY_F1);

  case GLFW_KEY_KP_0:
  case GLFW_KEY_KP_1:
  case GLFW_KEY_KP_2:
  case GLFW_KEY_KP_3:
  case GLFW_KEY_KP_4:
  case GLFW_KEY_KP_5:
  case GLFW_KEY_KP_6:
  case GLFW_KEY_KP_7:
  case GLFW_KEY_KP_8:
  case GLFW_KEY_KP_9:
    return BLB_KEY_KP_0 + (key - GLFW_KEY_KP_0);

  case GLFW_KEY_KP_DECIMAL:
    return BLB_KEY_KP_DECIMAL;

  case GLFW_KEY_KP_DIVIDE:
    return BLB_KEY_KP_DIVIDE;

  case GLFW_KEY_KP_MULTIPLY:
    return BLB_KEY_KP_MULTIPLY;

  case GLFW_KEY_KP_SUBTRACT:
    return BLB_KEY_KP_SUBTRACT;

  case GLFW_KEY_KP_ADD:
    return BLB_KEY_KP_ADD;

  case GLFW_KEY_KP_ENTER:
    return BLB_KEY_KP_ENTER;

  case GLFW_KEY_KP_EQUAL:
    return BLB_KEY_KP_EQUAL;

  case GLFW_KEY_LEFT_SHIFT:
    return BLB_KEY_LEFT_SHIFT;

  case GLFW_KEY_LEFT_CONTROL:
    return BLB_KEY_LEFT_CONTROL;

  case GLFW_KEY_LEFT_ALT:
    return BLB_KEY_LEFT_ALT;

  case GLFW_KEY_LEFT_SUPER:
    return BLB_KEY_LEFT_SUPER;

  case GLFW_KEY_RIGHT_SHIFT:
    return BLB_KEY_RIGHT_SHIFT;

  case GLFW_KEY_RIGHT_CONTROL:
    return BLB_KEY_RIGHT_CONTROL;

  case GLFW_KEY_RIGHT_ALT:
    return BLB_KEY_RIGHT_ALT;

  case GLFW_KEY_RIGHT_SUPER:
    return BLB_KEY_RIGHT_SUPER;

  case GLFW_KEY_MENU:
    return BLB_KEY_MENU;

  default:
    return BLB_KEY_UNKNOWN;
  }
}

static void key_cb(GLFWwindow *window, int key, int scancode, int action, int mods) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers || key < 0 || (unsigned)key >= BLB_HANDLER_KEY_COUNT)
    return;

  BLB_KeyState *state = &handlers->keyboard[(size_t)key];
  const double time = event_time();

  state->scancode = scancode;
  state->mods = (uint32_t)mods;
  state->last_change_time = time;

  state->pressed = state->pressed || action == GLFW_PRESS;
  state->released = state->released || action == GLFW_RELEASE;
  state->repeated = state->repeated || action == GLFW_REPEAT;

  if (action == GLFW_PRESS || action == GLFW_REPEAT)
    state->down = true;

  else if (action == GLFW_RELEASE)
    state->down = false;

  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){
                                       .type = BLB_EVENT_KEY,
                                       .data.key =
                                           {
                                               .key = native_to_key(key),
                                               .native_key = key,
                                               .scancode = scancode,
                                               .mods = (uint32_t)mods,
                                               .action = action == GLFW_PRESS     ? BLB_INPUT_PRESS
                                                         : action == GLFW_RELEASE ? BLB_INPUT_RELEASE
                                                                                  : BLB_INPUT_REPEAT,
                                               .down = state->down,
                                               .pressed = state->pressed,
                                               .released = state->released,
                                               .repeated = state->repeated,
                                               .time = time,
                                           },
                                   });
}

static void char_cb(GLFWwindow *window, unsigned int codepoint) {
  BLB_Handlers *handlers = glfwGetWindowUserPointer(window);

  if (!handlers)
    return;

  BLB_HandlersQueueEvent(handlers, (BLB_HandlerEvent){
                                       .type = BLB_EVENT_TEXT,
                                       .data.text =
                                           {
                                               .codepoint = codepoint,
                                               .time = event_time(),
                                           },
                                   });
}

int BLB_KeyboardAttach(BLB_Handlers *handlers, GLFWwindow *window) {
  if (!handlers || !window)
    return -1;

  glfwSetKeyCallback(window, key_cb);
  glfwSetCharCallback(window, char_cb);

  return 0;
}

void BLB_KeyboardDetach(BLB_Handlers *handlers) {
  if (!handlers || !handlers->window)
    return;

  glfwSetKeyCallback(handlers->window, NULL);
  glfwSetCharCallback(handlers->window, NULL);
}

static bool valid_key(BLB_Key key) { return key_to_native(key) >= 0 && key_to_native(key) < (int)BLB_HANDLER_KEY_COUNT; }

const BLB_KeyState *BLB_KeyboardGet(const BLB_Handlers *handlers, BLB_Key key) {

  if (!handlers || !valid_key(key))
    return NULL;

  const int native_key = key_to_native(key);

  return &handlers->keyboard[(size_t)native_key];
}

BLB_KeyEvent BLB_KeyboardInfo(const BLB_Handlers *handlers, BLB_Key key) {
  BLB_KeyEvent event = {0};

  if (!handlers || !valid_key(key))
    return event;

  const int native_key = key_to_native(key);
  const BLB_KeyState *state = &handlers->keyboard[(size_t)native_key];

  event.key = native_to_key(native_key);
  event.native_key = native_key;
  event.scancode = state->scancode;
  event.mods = state->mods;

  event.down = state->down;
  event.pressed = state->pressed;
  event.released = state->released;
  event.repeated = state->repeated;

  event.action = state->pressed    ? BLB_INPUT_PRESS
                 : state->released ? BLB_INPUT_RELEASE
                 : state->repeated ? BLB_INPUT_REPEAT
                 : state->down     ? BLB_INPUT_HOLD
                                   : BLB_INPUT_IDLE;

  event.time = state->last_change_time;

  return event;
}

bool BLB_KeyboardDown(const BLB_Handlers *handlers, BLB_Key key) {
  const BLB_KeyState *state = BLB_KeyboardGet(handlers, key);

  return state ? state->down : false;
}

bool BLB_KeyboardHeld(const BLB_Handlers *handlers, BLB_Key key) {
  const BLB_KeyState *state = BLB_KeyboardGet(handlers, key);

  return state ? state->down && !state->pressed && !state->released : false;
}

bool BLB_KeyboardPressed(const BLB_Handlers *handlers, BLB_Key key) {
  const BLB_KeyState *state = BLB_KeyboardGet(handlers, key);

  return state ? state->pressed : false;
}

bool BLB_KeyboardReleased(const BLB_Handlers *handlers, BLB_Key key) {

  const BLB_KeyState *state = BLB_KeyboardGet(handlers, key);

  return state ? state->released : false;
}

bool BLB_KeyboardRepeated(const BLB_Handlers *handlers, BLB_Key key) {
  const BLB_KeyState *state = BLB_KeyboardGet(handlers, key);

  return state ? state->repeated : false;
}

bool BLB_KeyboardSetHandler(BLB_Handlers *handlers, BLB_HandlerCallback callback, void *userdata) {
  return BLB_HandlersSetCallback(handlers, BLB_EVENT_KEY, callback, userdata);
}

void BLB_KeyboardClearTransitions(BLB_Handlers *handlers) {
  if (!handlers)
    return;

  for (size_t i = 0; i < BLB_HANDLER_KEY_COUNT; ++i) {
    handlers->keyboard[i].pressed = false;
    handlers->keyboard[i].released = false;
    handlers->keyboard[i].repeated = false;
  }
}
