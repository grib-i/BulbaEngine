#ifndef BULBA_DEMO_CELESTIAL_H
#define BULBA_DEMO_CELESTIAL_H

#include "demo_common.h"
#include "orbital_physics.h"

BLB_Object3D *Demo_Planet(Demo_Context *demo, const char *texture, BLB_Vec3 position, float radius);
BLB_Object3D *Demo_SaturnRings(Demo_Context *demo, BLB_Vec3 position, float planet_radius);
void Demo_Stars(Demo_Context *demo);
BLB_Vec3 Demo_OrbitDisplay(Solar_Vector position, float scale);

#endif
