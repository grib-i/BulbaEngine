#ifndef BULBA_CORE_PHYSICS_H
#define BULBA_CORE_PHYSICS_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t BLB_PhysicsLayerMask;

typedef struct {
  uint8_t layer;
  BLB_PhysicsLayerMask mask;
  int32_t group;
} BLB_PhysicsFilter;

static inline BLB_PhysicsLayerMask BLB_PhysicsLayer(uint8_t layer) { return UINT64_C(1) << layer; }

#endif
