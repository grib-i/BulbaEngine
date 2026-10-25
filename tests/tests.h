#ifndef BULBA_TESTS_H
#define BULBA_TESTS_H

#include "checks/test.h"
#include "demos/audio.h"
#include "demos/input.h"
#include "demos/lights_2d.h"
#include "demos/lights_3d.h"
#include "demos/lod.h"
#include "demos/materials_3d.h"
#include "demos/model_fbx.h"
#include "demos/model_glb.h"
#include "demos/model_gltf.h"
#include "demos/model_obj.h"
#include "demos/object_count_2d.h"
#include "demos/object_count_3d.h"
#include "demos/particles_2d.h"
#include "demos/particles_3d.h"
#include "demos/physics_2d.h"
#include "demos/physics_3d.h"
#include "demos/primitives_2d.h"
#include "demos/primitives_3d.h"
#include "demos/skybox.h"
#include "demos/sprite_animation.h"
#include "demos/text_2d.h"
#include "demos/textures_2d.h"
#include "demos/textures_3d.h"
#include "demos/transparency.h"

#include "demos/transparency_3d.h"

#include "demos/solar_system.h"

int Tests_Main(int argc, char **argv);

#endif
