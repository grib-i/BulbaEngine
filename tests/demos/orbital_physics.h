#ifndef BULBA_DEMO_ORBITAL_PHYSICS_H
#define BULBA_DEMO_ORBITAL_PHYSICS_H

#include <math.h>
#include <stddef.h>

#define SOLAR_BODY_COUNT 9
#define SOLAR_MU 0.0002959122082855911 /* AU^3 / day^2: Sun GM. */

typedef struct {
  double x, y, z;
} Solar_Vector;

typedef struct {
  const char *name;
  const char *texture;
  double a, e, inclination, longitude, perihelion, node; /* JPL J2000 degrees / AU. */
  double mass_ratio, radius_km, obliquity, spin_days;
} Solar_Elements;

typedef struct {
  Solar_Vector position[SOLAR_BODY_COUNT];
  Solar_Vector velocity[SOLAR_BODY_COUNT];
  double days;
  double accumulator;
} Solar_State;

extern const Solar_Elements Solar_Planets[SOLAR_BODY_COUNT];
void Solar_Initialize(Solar_State *state);
void Solar_Step(Solar_State *state, double days);
void Solar_Advance(Solar_State *state, double days, double fixed_step);
Solar_Vector Solar_OrbitPoint(size_t body, double eccentric_anomaly);
double Solar_Energy(const Solar_State *state);
Solar_Vector Solar_AngularMomentum(const Solar_State *state);

#endif
