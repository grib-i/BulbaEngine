#ifndef BLB_OBJECT_H
#define BLB_OBJECT_H

#include <stddef.h>
#include <stdint.h>

#define BLB_INVALID_OBJECT_ID UINT32_MAX

typedef enum {
  // 3D objects
  BLB_OBJECT_CUBE = 0,
  BLB_OBJECT_SPHERE = 1,
  BLB_OBJECT_CYLINDER = 2,
  BLB_OBJECT_CONE = 3,
  BLB_OBJECT_TORUS = 4,
  BLB_OBJECT_CAPSULE = 5,
  BLB_OBJECT_PLANE = 6,
  BLB_OBJECT_POLYGON3D = 7,

  // 2D objects
  BLB_OBJECT_SQUARE = 8,
  BLB_OBJECT_CIRCLE2D = 9,
  BLB_OBJECT_TRIANGLE2D = 10,
  BLB_OBJECT_POLYGON2D = 11,
  BLB_OBJECT_SPRITE = 12,
  BLB_OBJECT_TEXT = 13,

  // Custom objects
  BLB_OBJECT_CUSTOM_MODEL = 14,

  BLB_OBJECT_COUNT = 15
} BLB_ObjectType;

typedef struct {
  uint32_t id;
  char id_type[60];
} BLB_ObjectID;

extern BLB_ObjectID *BLB_OBJECTS_ID;
extern size_t BLB_OBJECTS_ID_COUNT;

void BLB_Object_Init(void);

#endif
