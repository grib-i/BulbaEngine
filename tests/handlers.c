#include "bulba/core/camera.h"
#include "bulba/core/math3v/HandmadeMath.h"
#include "bulba/core/objects3d/objects3d.h"
#include "test_common.h"
#include "tests.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  BLB_Object2D *object2d;
  BLB_Object3D *object3d;
  BLB_Camera *camera;
  bool dragging;
  BLB_ObjectHandlerObjectType drag_type;
  HMM_Vec3 drag_offset;
} HandlerTestState;

static void handler_reset_drag(HandlerTestState *state) {
  if (!state)
    return;

  state->dragging = false;
  state->drag_type = 0;
  state->drag_offset = HMM_V3(0.0f, 0.0f, 0.0f);
}

static void handler_click(const BLB_ObjectEvent *event, void *userdata) {
  HandlerTestState *state = userdata;

  if (!state || !event || event->button != 0)
    return;

  handler_reset_drag(state);

  printf("[HandlrsTest] CLICK %s\n", event->object_type == BLB_OBJECT_HANDLER_2D ? "2D" : "3D");
}

static void handler_press(const BLB_ObjectEvent *event, void *userdata) {
  HandlerTestState *state = userdata;
  if (!state || !event || event->button != 0 || !event->captured || event->distance < 0.0f)
    return;

  state->dragging = false;
  state->drag_type = event->object_type;

  if (event->object_type == BLB_OBJECT_HANDLER_2D && event->object.object2d) {
    BLB_Object2D *object = event->object.object2d;
    state->drag_offset = HMM_V3(object->position.x - event->world_position.x, object->position.y - event->world_position.y, 0.0f);
    return;
  }

  if (event->object_type == BLB_OBJECT_HANDLER_3D && event->object.object3d) {
    BLB_Object3D *object = event->object.object3d;
    HMM_Vec3 object_world_position = object->position;
    if (object->camera_attached && state->camera && state->camera->camera_cache && state->camera->camera_cache->valid) {
      HMM_Mat4 camera_world = HMM_InvGeneralM4(state->camera->camera_cache->view);
      HMM_Vec4 world4 = HMM_MulM4V4(camera_world, HMM_V4(object->position.x, object->position.y, object->position.z, 1.0f));
      if (fabsf(world4.w) > 1e-7f)
        object_world_position = HMM_V3(world4.x / world4.w, world4.y / world4.w, world4.z / world4.w);
    }

    HMM_Vec3 plane_normal = HMM_V3(0.0f, 0.0f, 1.0f);
    if (state->camera && state->camera->camera_cache && state->camera->camera_cache->valid) {
      HMM_Mat4 camera_world = HMM_InvGeneralM4(state->camera->camera_cache->view);
      HMM_Vec4 normal4 = HMM_MulM4V4(camera_world, HMM_V4(0.0f, 0.0f, 1.0f, 0.0f));
      plane_normal = HMM_NormV3(HMM_V3(normal4.x, normal4.y, normal4.z));
    }

    const float denominator = HMM_DotV3(plane_normal, event->ray_direction);
    HMM_Vec3 initial_pointer = event->hit_point;
    if (fabsf(denominator) > 1e-7f) {
      const float t = HMM_DotV3(plane_normal, HMM_SubV3(object_world_position, event->ray_origin)) / denominator;
      if (t >= 0.0f)
        initial_pointer = HMM_AddV3(event->ray_origin, HMM_MulV3F(event->ray_direction, t));
    }
    state->drag_offset = HMM_SubV3(object_world_position, initial_pointer);
  }
}

static void handler_drag(const BLB_ObjectEvent *event, void *userdata) {
  HandlerTestState *state = userdata;
  if (!state || !event || event->button != 0 || !event->captured || event->distance < 0.0f)
    return;

  state->dragging = true;
  if (event->object_type == BLB_OBJECT_HANDLER_2D && event->object.object2d) {
    BLB_Object2D *object = event->object.object2d;
    BLB_Object2D_SetPosition(object, HMM_V2(event->world_position.x + state->drag_offset.x, event->world_position.y + state->drag_offset.y));

  } else if (event->object_type == BLB_OBJECT_HANDLER_3D && event->object.object3d) {
    BLB_Object3D *object = event->object.object3d;
    HMM_Vec3 world_position = HMM_AddV3(event->hit_point, state->drag_offset);

    if (object->camera_attached && state->camera && state->camera->camera_cache && state->camera->camera_cache->valid) {
      HMM_Vec4 local4 = HMM_MulM4V4(state->camera->camera_cache->view, HMM_V4(world_position.x, world_position.y, world_position.z, 1.0f));
      if (fabsf(local4.w) > 1e-7f)
        BLB_Object3D_SetPosition(object, HMM_V3(local4.x / local4.w, local4.y / local4.w, local4.z / local4.w));
    } else {
      BLB_Object3D_SetPosition(object, world_position);
    }
  }
}

static void handler_drag_end(const BLB_ObjectEvent *event, void *userdata) {
  (void)event;
  handler_reset_drag(userdata);
}

int BLB_TestHandlers(void) {
  BLB_TestContext app;

  if (BLB_TestContext_Init(&app, 1200, 900, "BulbaEngine - Object Handlers", HMM_V3(0.0f, 0.0f, 12.0f), 9, 11, 16) != 0)
    return -1;

  BLB_TestContext_AddLight(&app, BLB_LIGHT_POINT, HMM_V3(-3.0f, 5.0f, 8.0f), HMM_V3(0.0f, 0.0f, 0.0f), 18.0f, 0.03f, 1.0f, 30.0f);

  HandlerTestState state = {0};
  BLB_Camera *camera = app.scene->camera;
  BLB_Camera_SetPosition(camera, HMM_V3(0.0f, 0.0f, 1.0f));
  state.camera = camera;

  state.object3d = BLB_CreateCube3D(HMM_V3(0.6f, 0.6f, 0.6f), HMM_V3(0.35f, -0.05f, -4.0f), NULL, BLB_TEXMAP_STRETCH);
  BLB_Object3D_SetCameraAttached(state.object3d, false);

  state.object2d = BLB_CreateSquare2D(HMM_V2(0.25f, 0.22f), HMM_V2(-0.45f, 0.08f), NULL, false);

  if (!state.object3d || !state.object2d) {
    BLB_DestroySquare2D(state.object2d);
    BLB_DestroyCube3D(state.object3d);
    BLB_TestContext_Shutdown(&app);
    return -1;
  }

  state.object3d->layer = 11;
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
  handler2d.on_press = handler_press;
  handler2d.on_drag_start = handler_drag;
  handler2d.on_drag = handler_drag;
  handler2d.on_drag_end = handler_drag_end;
  handler2d.userdata = &state;

  handler3d.on_click = handler_click;
  handler3d.on_press = handler_press;
  handler3d.on_drag_start = handler_drag;
  handler3d.on_drag = handler_drag;
  handler3d.on_drag_end = handler_drag_end;
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
