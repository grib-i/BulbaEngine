#ifndef BULBA_CORE_RENDER_H
#define BULBA_CORE_RENDER_H

#include "bulba/core/render/material.h"
#include "bulba/core/render/shader.h"
#include "bulba/core/scene.h"
#include "bulba/graphics/vulkan/renderer.h"

int BLB_DrawScene(BLB_Scene *scene, VULKAN *renderer);

#endif
