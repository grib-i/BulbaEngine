#include "bulba/core/math3v/HandmadeMath.h"
#include "test_common.h"
#include "tests.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
  BLB_Object2D *object2d;
  BLB_Object3D *object3d;
  bool dragging;
  bool have_hold_frame;
  uint64_t last_hold_frame;
  BLB_ObjectHandlerObjectType drag_type;
  HMM_Vec3 drag_offset;
} HandlerTestState;

static void handler_reset_drag(HandlerTestState *state) {
  if (!state)
    return;

  state->dragging = false;
  state->have_hold_frame = false;
  state->last_hold_frame = 0;
  state->drag_type = 0;
  state->drag_offset = HMM_V3(0.0f, 0.0f, 0.0f);
}

static void handler_click(const BLB_ObjectEvent *event, void *userdata) {
  HandlerTestState *state = userdata;

  if (!state || !event || event->button != 0)
    return;

  handler_reset_drag(state);

  printf("[HandlersTest] CLICK %s\n", event->object_type == BLB_OBJECT_HANDLER_2D ? "2D" : "3D");
}

static void handler_hold(const BLB_ObjectEvent *event, void *userdata) {
  HandlerTestState *state = userdata;

  if (!state || !event || event->button != 0 || !event->captured)
    return;

  const bool new_drag =
      !state->dragging || !state->have_hold_frame || event->frame != state->last_hold_frame + 1u || state->drag_type != event->object_type;

  if (new_drag) {
    state->dragging = true;
    state->drag_type = event->object_type;

    if (state->drag_type == BLB_OBJECT_HANDLER_3D && state->object3d) {
      const HMM_Vec3 mouse_world = HMM_V3(event->world_position.x, event->world_position.y, state->object3d->position.z);

      state->drag_offset = HMM_SubV3(state->object3d->position, mouse_world);
    } else {
      state->drag_offset = HMM_V3(0.0f, 0.0f, 0.0f);
    }
  }

  state->last_hold_frame = event->frame;
  state->have_hold_frame = true;

  if (event->object_type == BLB_OBJECT_HANDLER_2D && event->object.object2d) {
    event->object.object2d->position.x += (float)event->mouse_dx;
    event->object.object2d->position.y -= (float)event->mouse_dy;
  }

  if (event->object_type == BLB_OBJECT_HANDLER_3D && event->object.object3d) {
    event->object.object3d->position.x = event->world_position.x + state->drag_offset.x;

    event->object.object3d->position.y = -(event->world_position.y + state->drag_offset.y);
  }
}

int BLB_TestHandlers(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Object Handlers", HMM_V3(0.0f, 0.0f, 12.0f), 9, 11, 16) != 0)
    return -1;

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-3.0f, 5.0f, 8.0f), HMM_V3(0.0f, 0.0f, 0.0f), 18.0f, 0.03f, 1.0f, 30.0f);

  HandlerTestState state = {0};

  state.object3d = BLB_CreateCube3D(HMM_V3(3.0f, 3.0f, 3.0f), HMM_V3(-2.5f, 0.0f, 0.0f), NULL, BLB_TEXMAP_STRETCH);

  state.object2d = BLB_CreateSquare2D(HMM_V2(250.0f, 150.0f), HMM_V2(330.0f, 360.0f), NULL, true);

  if (!state.object3d || !state.object2d) {
    BLB_DestroySquare2D(state.object2d);
    BLB_DestroyCube3D(state.object3d);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  state.object3d->layer = 10;
  state.object3d->color.x = 255;
  state.object3d->color.y = 145;
  state.object3d->color.z = 40;
  state.object3d->color.w = 255;

  state.object2d->layer = 10;
  state.object2d->color.x = 60;
  state.object2d->color.y = 145;
  state.object2d->color.z = 255;
  state.object2d->color.w = 255;

  BLB_AddObject3D(app.scene, state.object3d);
  BLB_AddObject2D(app.scene, state.object2d);

  BLB_ObjectHandler handler2d;
  BLB_ObjectHandler handler3d;

  BLB_ObjectHandlerInit(&handler2d);
  BLB_ObjectHandlerInit(&handler3d);

  handler2d.on_click = handler_click;
  handler2d.on_hold = handler_hold;
  handler2d.userdata = &state;

  handler3d.on_click = handler_click;
  handler3d.on_hold = handler_hold;
  handler3d.userdata = &state;

  BLB_Object2D_SetHandler(state.object2d, &handler2d);
  BLB_Object3D_SetHandler(state.object3d, &handler3d);

  printf("[HandlersTest] LMB: drag 2D square or 3D cube.\n");

  while (!BLB_WindowShouldClose(app.window)) {
    float delta_time = 0.0f;
    int frame = BLB_TestContext_BeginFrame(&app, &delta_time);

    if (frame < 0)
      break;

    if (frame > 0)
      continue;

    if (BLB_TestContext_Draw(&app) < 0)
      break;
  }

  BLB_DestroySquare2D(state.object2d);
  BLB_DestroyCube3D(state.object3d);
  BLB_TestContext_Shutdown(&app);

  return 0;
}
