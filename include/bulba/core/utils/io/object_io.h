#ifndef BLB_OBJECT_IO_H
#define BLB_OBJECT_IO_H

#include "bulba/core/objects2d/objects2d.h"
#include "bulba/core/objects3d/objects3d.h"

#include <stdint.h>

typedef struct BLB_Scene BLB_Scene;

#define BLB_OBJECT_FILE_VERSION 4

typedef enum { BLB_OBJECT_FILE_2D = 2, BLB_OBJECT_FILE_3D = 3 } BLB_ObjectFileType;

typedef struct {
  char magic[4];
  uint32_t version;
  uint32_t type;
} BLB_ObjectFileHeader;

int BLB_Object2D_Save(const BLB_Object2D *object, const char *path);
BLB_Object2D *BLB_Object2D_Load(const char *path, BLB_Scene *scene);

int BLB_Object3D_Save(const BLB_Object3D *object, const char *path);
BLB_Object3D *BLB_Object3D_Load(const char *path, BLB_Scene *scene);

#endif
