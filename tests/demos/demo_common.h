#ifndef BULBA_DEMO_COMMON_H
#define BULBA_DEMO_COMMON_H

#include "../checks/test.h"

/* Pass NULL for defaults. A zero object_count selects the scene's recommended count.
 * Camera modes apply to 3D; screen-space 2D scenes intentionally keep their HUD/canvas fixed. */
typedef enum { DEMO_CAMERA_SCENE, DEMO_CAMERA_FIXED, DEMO_CAMERA_ORBIT, DEMO_CAMERA_FLY } Demo_CameraMode;

typedef struct Demo_Settings {
  /* Scene collections: 0 selects the scene default; a positive value overrides it. */
  unsigned object_count;
  unsigned particle_count;
  unsigned detail;

  /* Zero frame_limit keeps the window open until Esc/close. */
  unsigned long frame_limit;
  bool animate;    /* Pause all simulation time when false. */
  bool transforms; /* Scene movement/physics callback; skeletal/sprite clips are independent. */
  bool bloom;
  float speed;

  /* Clip playback, camera control and scene-specific visual parameters. */
  float animation_speed;
  float model_size;
  unsigned layout_samples; /* Full-clip envelope sampling for skeletal model placement. */
  float camera_speed;
  float camera_distance;
  float camera_height;
  float light_intensity;
  float opacity;
  float emission_rate;
  float sprite_frame_time;

  /* Orbital integration uses AU/day doubles, independently of display scaling. */
  float solar_days_per_second;
  float solar_step_days;
  float solar_distance_scale;
  float solar_radius_scale;
  bool solar_orbits;

  /* NULL selects bundled assets / the recommended clip. Strings are borrowed for the run. */
  const char *model_path;
  const char *animation_name;
  const char *skybox_path;
  Demo_CameraMode camera_mode;
} Demo_Settings;

Demo_Settings Demo_DefaultSettings(void);

typedef struct Demo_Context {
  BLB_Window *window;
  BLB_Renderer renderer;
  BLB_Scene *scene;
  BLB_Text2D *fps;
  BLB_Text2D *animation_status;
  BLB_Object3D *animation_subject;
  BLB_IO_Audio *audio;
  Demo_Settings settings;
  BLB_Vec3 camera_target;
  BLB_Object3D *stage[3];
  void *user_data;
  void (*after_camera)(struct Demo_Context *demo); /* Presentation-only hook; runs even when transforms are off. */
  bool is_3d;
  bool failed;
  double elapsed;
  float dt;
} Demo_Context;

typedef void (*Demo_Update)(Demo_Context *demo);

bool Demo_Open(Demo_Context *demo, const char *title, const Demo_Settings *settings);
int Demo_Finish(Demo_Context *demo, Demo_Update update);
bool Demo_IsStage(const Demo_Context *demo, const BLB_Object3D *object);
void Demo_Stage(Demo_Context *demo, float width);
void Demo_AimCamera(Demo_Context *demo, BLB_Vec3 position, BLB_Vec3 target);
void Demo_Color(Demo_Context *demo, BLB_Object3D *object, BLB_Vec4 color, float metallic, float roughness);
unsigned Demo_Count(const Demo_Context *demo, unsigned fallback);
BLB_Object2D *Demo_Add2D(Demo_Context *demo, BLB_Object2D *object);
BLB_Object3D *Demo_Add3D(Demo_Context *demo, BLB_Object3D *object);
BLB_Text2D *Demo_Text(Demo_Context *demo, const char *value, float x, float y, float size);
BLB_Object2D *Demo_Square(Demo_Context *demo, float x, float y, float width, float height);
BLB_Object3D *Demo_Cube(Demo_Context *demo, float x, float y, float z, float size);
void Demo_Light(Demo_Context *demo);
void Demo_Spin(Demo_Context *demo);
/* World bounds include every submesh and the renderer's Rz * Ry * Rx * S transform. */
bool Demo_Bounds3D(const BLB_Object3D *object, bool world, BLB_Vec3 *minimum, BLB_Vec3 *maximum);
void Demo_GroundObject(BLB_Object3D *object, float surface, float clearance);
void Demo_FitModel(BLB_Object3D *object, float size);
bool Demo_FitAnimatedModel(BLB_Object3D *object, float size, unsigned samples);
const char *Demo_Font(void);

#endif
