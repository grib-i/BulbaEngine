#ifndef BULBA_H
#define BULBA_H

#include "bulba/core/camera.h"
#include "bulba/core/entity.h"
#include "bulba/core/math3v/lights.h"
#include "bulba/core/math3v/polygon.h"
#include "bulba/core/objects2d/audio2d.h"
#include "bulba/core/objects2d/circle.h"
#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects2d/square.h"
#include "bulba/core/objects2d/text2d.h"
#include "bulba/core/objects3d/animation3d.h"
#include "bulba/core/objects3d/audio3d.h"
#include "bulba/core/objects3d/cube.h"
#include "bulba/core/physics/physics2d.h"
#include "bulba/core/physics/physics3d.h"

#include "bulba/core/objects3d/model_loader_fbx.h"
#include "bulba/core/objects3d/model_loader_glb.h"
#include "bulba/core/objects3d/model_loader_gltf.h"
#include "bulba/core/objects3d/model_loader_obj.h"

#include "bulba/core/objects3d/objects3d.h"
#include "bulba/core/objects3d/skybox.h"
#include "bulba/core/objects3d/sphere.h"
#include "bulba/core/objects3d/torus.h"
#include "bulba/core/physics/physics.h"
#include "bulba/core/platform.h"
#include "bulba/core/render.h"
#include "bulba/core/render/material.h"

#include "bulba/core/render/object_lod.h"

#include "bulba/core/render/shader.h"
#include "bulba/core/render/texture.h"
#include "bulba/core/scene.h"
#include "bulba/core/utils/audio.h"
#include "bulba/core/utils/colors.h"
#include "bulba/core/utils/config.h"
#include "bulba/core/utils/delta_time.h"
#include "bulba/core/utils/font.h"
#include "bulba/core/utils/fps.h"
#include "bulba/core/utils/io/object_io.h"
#include "bulba/core/window.h"
#include "bulba/graphics/renderer.h"
#include "bulba/graphics/vulkan/renderer.h"
#include "bulba/graphics/vulkan/renderer_backend.h"
#include "bulba/graphics/vulkan/vulkan.h"
#include "bulba/handlers/handlers.h"
#include "bulba/handlers/keyboard.h"
#include "bulba/handlers/mouse.h"
#include "bulba/handlers/object.h"

void BLB_Init(void);

#endif
