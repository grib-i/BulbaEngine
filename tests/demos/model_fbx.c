#include "model_fbx.h"

int Demo_ModelFbx(const Demo_Settings *settings) {
  Demo_Context demo;

  if (!Demo_Open(&demo, "MOTION STUDY / FBX skeletal animation", settings))
    return Demo_Finish(&demo, NULL);

  unsigned count = Demo_Count(&demo, 1);
  float pitch = demo.settings.model_size * 0.95f;
  float width = fmaxf(6.5f, demo.settings.model_size * 1.4f) + (count - 1) * pitch;

  Demo_Stage(&demo, width);

  if (count == 1) {
    float framing = demo.settings.model_size / 4.6f;

    Demo_AimCamera(&demo, BLB_V3(3.8f * framing, 2.4f * framing, 5.8f * framing), BLB_V3(0, 1.05f * framing, 0));
  }

  Demo_Light(&demo);

  const char *path = demo.settings.model_path ? demo.settings.model_path : "assets/tests/models/fox/Fox.fbx";
  const char *clip = demo.settings.animation_name ? demo.settings.animation_name : "Run";

  for (unsigned i = 0; i < count && !demo.failed; ++i) {
    BLB_Object3D *object = Demo_Add3D(&demo, BLB_LoadModelFBX(path, BLB_V3(1, 1, 1), BLB_V3(0, 0, 0), NULL, BLB_TEXMAP_UV));

    if (!object)
      break;

    if (!BLB_Model_GetBoneCount(object) || BLB_Model_PlayAnimation(object, clip, true) != 0) {
      fprintf(stderr, "Unable to play skeletal clip '%s' from %s\n", clip, path);
      demo.failed = true;
      break;
    }

    BLB_Model_SetAnimationTime(object, 0);

    if (!Demo_FitAnimatedModel(object, demo.settings.model_size, demo.settings.layout_samples)) {
      demo.failed = true;
      break;
    }

    BLB_Vec3 position = object->position;

    position.x += ((float)i - ((float)count - 1) * 0.5f) * pitch;
    BLB_Object3D_SetPosition(object, position);

    BLB_Model_SetAnimationSpeed(object, demo.settings.animation_speed);

    if (i == 0) {
      demo.animation_subject = object;
      demo.animation_status = Demo_Text(&demo, "Preparing skeletal playback...", 20, 680, 18);
    }
  }

  return Demo_Finish(&demo, NULL);
}
