#ifndef BULBA_CORE_OBJECTS3D_MODEL_LOADER_H
#define BULBA_CORE_OBJECTS3D_MODEL_LOADER_H

#include "bulba/core/math3v/polygon.h"

int BLB_LoadModelOBJ(const char *path, Mesh *mesh);
void BLB_DestroyMesh(Mesh *mesh);

#endif
