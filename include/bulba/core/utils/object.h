#ifndef BLB_OBJECT_H
#define BLB_OBJECT_H

#include <stddef.h>
#include <stdint.h>

typedef struct BLB_Object3D BLB_Object3D;
typedef struct BLB_Object2D BLB_Object2D;

typedef enum {
  BLB_TRANSFORM_TYPE_NONE = -1,

  BLB_TRANSFORM_TYPE_SET_POSITION = 0,
  BLB_TRANSFORM_TYPE_MOVE,
  BLB_TRANSFORM_TYPE_SET_ROTATION,
  BLB_TRANSFORM_TYPE_ROTATE,
  BLB_TRANSFORM_TYPE_SET_SCALE,
  BLB_TRANSFORM_TYPE_SCALE,
  BLB_TRANSFORM_TYPE_TRANSFORM,

  BLB_TRANSFORM_TYPE_COUNT
} BLB_TransformType;

/*
 * BLB_PHYSICS_SYNC_NONE — когда объект и тело не привязаны друг к другу, можно двигать, вращать и масштабировать объект и тело независимо друг от
 * друга.
 *
 * BLB_PHYSICS_SYNC_OBJECT_TO_BODY — объект -> физическое тело. Изменения объекта передаются телу.
 * BLB_PHYSICS_SYNC_BODY_TO_OBJECT — физическое тело -> объект. Изменения тела передаются объекту.
 *
 * BLB_PHYSICS_SYNC_BIDIRECTIONAL — когда объект и тело взаимно синхронизированны (взаимно привязаны друг к другу)
 */
typedef enum {
  /* Object and body are independent */
  BLB_PHYSICS_SYNC_NONE,

  /* Object drives body: Object -> Body */
  BLB_PHYSICS_SYNC_OBJECT_TO_BODY,

  /* Body drives object: Body -> Object */
  BLB_PHYSICS_SYNC_BODY_TO_OBJECT,

  /* Object and body synchronize with each other: Object <-> Body */
  BLB_PHYSICS_SYNC_BIDIRECTIONAL
} BLB_PhysicsSyncMode;

static inline int BLB_PhysicsSyncToBody(BLB_PhysicsSyncMode mode) {
  return mode == BLB_PHYSICS_SYNC_OBJECT_TO_BODY || mode == BLB_PHYSICS_SYNC_BIDIRECTIONAL;
}

static inline int BLB_PhysicsSyncToObject(BLB_PhysicsSyncMode mode) {
  return mode == BLB_PHYSICS_SYNC_BODY_TO_OBJECT || mode == BLB_PHYSICS_SYNC_BIDIRECTIONAL;
}

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

#define BLB_INVALID_OBJECT_ID UINT32_MAX

typedef struct {
  uint32_t id;
  char id_type[60];
} BLB_ObjectID;

extern BLB_ObjectID *BLB_OBJECTS_ID;
extern size_t BLB_OBJECTS_ID_COUNT;

void BLB_Object_Init(void);

#endif
