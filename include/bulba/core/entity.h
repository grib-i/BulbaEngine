#ifndef BULBA_CORE_ENTITY_H
#define BULBA_CORE_ENTITY_H

#include <stdint.h>

typedef uint64_t BLB_EntityId;
typedef uint64_t BLB_ComponentMask;

#define BLB_INVALID_ENTITY_ID ((BLB_EntityId)0)

enum {
  BLB_COMPONENT_TRANSFORM = 1ull << 0,
  BLB_COMPONENT_RENDERABLE = 1ull << 1,
  BLB_COMPONENT_RIGIDBODY = 1ull << 2,
  BLB_COMPONENT_COLLIDER = 1ull << 3,
  BLB_COMPONENT_LIGHT = 1ull << 4,
  BLB_COMPONENT_SCRIPT = 1ull << 5,
  BLB_COMPONENT_AUDIO = 1ull << 6
};

#endif
