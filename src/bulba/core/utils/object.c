#include "bulba/core/utils/object.h"

#include <stdlib.h>

BLB_ObjectID *BLB_OBJECTS_ID = NULL;
size_t BLB_OBJECTS_ID_COUNT = BLB_OBJECT_COUNT;

void BLB_Object_Init(void) {
  BLB_OBJECTS_ID = malloc(sizeof(*BLB_OBJECTS_ID) * BLB_OBJECTS_ID_COUNT);

  if (BLB_OBJECTS_ID == NULL) {
    BLB_OBJECTS_ID_COUNT = 0;
    return;
  }

  BLB_OBJECTS_ID[0] = (BLB_ObjectID){0, "cube"};
  BLB_OBJECTS_ID[1] = (BLB_ObjectID){0, "sphere"};
  BLB_OBJECTS_ID[2] = (BLB_ObjectID){0, "cylinder"};
  BLB_OBJECTS_ID[3] = (BLB_ObjectID){0, "cone"};
  BLB_OBJECTS_ID[4] = (BLB_ObjectID){0, "torus"};
  BLB_OBJECTS_ID[5] = (BLB_ObjectID){0, "capsule"};
  BLB_OBJECTS_ID[6] = (BLB_ObjectID){0, "plane"};
  BLB_OBJECTS_ID[7] = (BLB_ObjectID){0, "polygon3d"};

  BLB_OBJECTS_ID[8] = (BLB_ObjectID){0, "square"};
  BLB_OBJECTS_ID[9] = (BLB_ObjectID){0, "circle2d"};
  BLB_OBJECTS_ID[10] = (BLB_ObjectID){0, "triangle2d"};
  BLB_OBJECTS_ID[11] = (BLB_ObjectID){0, "polygon2d"};
  BLB_OBJECTS_ID[12] = (BLB_ObjectID){0, "sprite"};
  BLB_OBJECTS_ID[13] = (BLB_ObjectID){0, "text"};

  BLB_OBJECTS_ID[14] = (BLB_ObjectID){0, "custom_model"};
}
