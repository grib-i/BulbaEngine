#include "bulba/bulba.h"

#include "tests.h"

int main(void) {
  int result = -1;
  BLB_DEBUG = false;

  /*  Uncomment one return below to launch directly without command-line
   * arguments. */

  // Demo_Settings demo = Demo_DefaultSettings();
  // demo.object_count = 20;
  // demo.camera_mode = DEMO_CAMERA_ORBIT;
  // demo.transforms = false;
  // demo.camera_distance = 1;

  // result = Demo_ObjectCount3d(&demo);
  // result = Demo_ObjectCount2d(&demo);

  // result = Demo_ModelObj(NULL);
  // result = Demo_ModelGlb(NULL);
  // result = Demo_ModelFbx(NULL);
  // result = Demo_ModelGltf(NULL);

  // result = Demo_Primitives2d(NULL);
  // result = Demo_Primitives3d(NULL);
  // result = Demo_Textures2d(NULL);
  // result = Demo_SpriteAnimation(NULL);
  result = Demo_Text2d(NULL);
  // result = Demo_Particles2d(NULL);
  // result = Demo_Particles3d(NULL);
  // result = Demo_Textures3d(NULL);
  // result = Demo_Materials3d(NULL);
  // result = Demo_Lights3d(NULL);
  // result = Demo_Physics2d(NULL);
  // result = Demo_Physics3d(NULL);

  // result = Demo_Skybox(NULL);
  // result = Demo_SolarSystem(NULL);

  // result = Demo_ObjectCount2d(NULL);
  // result = Demo_ObjectCount3d(NULL);
  // result = Demo_Transparency(NULL);
  // result = Demo_Lod(NULL);
  // result = Demo_Input(NULL);
  // result = Demo_Lights2d(NULL);
  // result = Demo_Audio(NULL);
  // result = Demo_Transparency3d(NULL);

  // Test_Settings checks = Test_DefaultSettings();
  // checks.cycles = 500;
  // result = Test_Lifecycle(&checks);

  // result = Test_Run("all");
  // result = Test_Run("io");
  // result = Test_Run("shaders");
  // result = Test_Run("audio");
  // result = Test_Run("core");
  // result = Test_Run("resources");
  // result = Test_Run("physics2d");
  // result = Test_Run("physics3d");
  // result = Test_Run("particles2d");
  // result = Test_Run("particles3d");
  // result = Test_Run("handlers");
  // result = Test_Run("assets");
  // result = Test_Run("spatial");
  // result = Test_Run("renderer");
  // result = Test_Benchmarks(NULL);
  // result = Test_Lifecycle(NULL);

  // result = Tests_Main(argc, argv);

  return result;
}
