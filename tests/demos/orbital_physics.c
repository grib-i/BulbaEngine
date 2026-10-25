#include "orbital_physics.h"

/* JPL approximate elements at J2000. Earth uses the Earth-Moon barycenter.
 * Initial osculating ellipses are approximations, not a Horizons ephemeris.
 * All subsequent motion is integrated Newtonian nine-body gravity. */
const Solar_Elements Solar_Planets[SOLAR_BODY_COUNT] = {
    {"SUN", "sun", 0, 0, 0, 0, 0, 0, 1, 695700, 7.25, 25.38},
    {"MERCURY", "mercury", .38709927, .20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593, 1.66012e-7, 2439.7, .034, 58.646},
    {"VENUS", "venus_atmosphere", .72333566, .00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255, 2.44784e-6, 6051.8, 177.36, 243.025},
    {"EARTH", "earth_daymap", 1.00000261, .01671123, -.00001531, 100.46457166, 102.93768193, 0, 3.04043e-6, 6371, 23.44, .99727},
    {"MARS", "mars", 1.52371034, .09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891, 3.22715e-7, 3389.5, 25.19, 1.02596},
    {"JUPITER", "jupiter", 5.20288700, .04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909, .0009547919, 69911, 3.13, .41354},
    {"SATURN", "saturn", 9.53667594, .05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448, .0002858857, 58232, 26.73, .44401},
    {"URANUS", "uranus", 19.18916464, .04725744, .77263783, 313.23810451, 170.95427630, 74.01692503, .0000436624, 25362, 97.77, .71833},
    {"NEPTUNE", "neptune", 30.06992276, .00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574, .0000515139, 24622, 28.32, .67125}};

static Solar_Vector rotate_orbit(size_t body, double x, double y) {
  const Solar_Elements *p = &Solar_Planets[body];
  const double radians = 0.017453292519943295;
  double w = (p->perihelion - p->node) * radians;
  double n = p->node * radians;
  double i = p->inclination * radians;
  double u = cos(w) * x - sin(w) * y;
  double v = sin(w) * x + cos(w) * y;

  /* Render convention: Y is ecliptic north, X/Z span the orbital plane. */
  return (Solar_Vector){cos(n) * u - sin(n) * cos(i) * v, sin(i) * v, sin(n) * u + cos(n) * cos(i) * v};
}

Solar_Vector Solar_OrbitPoint(size_t body, double eccentric_anomaly) {
  if (body == 0 || body >= SOLAR_BODY_COUNT)
    return (Solar_Vector){0};

  const Solar_Elements *p = &Solar_Planets[body];

  return rotate_orbit(body, p->a * (cos(eccentric_anomaly) - p->e), p->a * sqrt(1 - p->e * p->e) * sin(eccentric_anomaly));
}

void Solar_Initialize(Solar_State *state) {
  *state = (Solar_State){0};
  Solar_Vector center = {0}, drift = {0};
  double total_mass = 1;

  for (size_t i = 1; i < SOLAR_BODY_COUNT; ++i) {
    const Solar_Elements *p = &Solar_Planets[i];
    double mean = remainder((p->longitude - p->perihelion) * .017453292519943295, 6.283185307179586);
    double eccentric = mean;

    for (unsigned iteration = 0; iteration < 12; ++iteration)
      eccentric -= (eccentric - p->e * sin(eccentric) - mean) / (1 - p->e * cos(eccentric));

    double rate = sqrt(SOLAR_MU * (1 + p->mass_ratio) / (p->a * p->a * p->a)) / (1 - p->e * cos(eccentric));

    state->position[i] = Solar_OrbitPoint(i, eccentric);
    state->velocity[i] = rotate_orbit(i, -p->a * sin(eccentric) * rate, p->a * sqrt(1 - p->e * p->e) * cos(eccentric) * rate);
    center.x += p->mass_ratio * state->position[i].x;
    center.y += p->mass_ratio * state->position[i].y;
    center.z += p->mass_ratio * state->position[i].z;
    drift.x += p->mass_ratio * state->velocity[i].x;
    drift.y += p->mass_ratio * state->velocity[i].y;
    drift.z += p->mass_ratio * state->velocity[i].z;
    total_mass += p->mass_ratio;
  }

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    state->position[i].x -= center.x / total_mass;
    state->position[i].y -= center.y / total_mass;
    state->position[i].z -= center.z / total_mass;
    state->velocity[i].x -= drift.x / total_mass;
    state->velocity[i].y -= drift.y / total_mass;
    state->velocity[i].z -= drift.z / total_mass;
  }
}

static void accelerations(const Solar_State *state, Solar_Vector acceleration[SOLAR_BODY_COUNT]) {
  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i)
    acceleration[i] = (Solar_Vector){0};

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    for (size_t j = i + 1; j < SOLAR_BODY_COUNT; ++j) {
      double x = state->position[j].x - state->position[i].x;
      double y = state->position[j].y - state->position[i].y;
      double z = state->position[j].z - state->position[i].z;
      double r2 = x * x + y * y + z * z;
      double factor = SOLAR_MU / (r2 * sqrt(r2));
      double fi = factor * Solar_Planets[j].mass_ratio;
      double fj = factor * Solar_Planets[i].mass_ratio;

      acceleration[i].x += fi * x;
      acceleration[i].y += fi * y;
      acceleration[i].z += fi * z;
      acceleration[j].x -= fj * x;
      acceleration[j].y -= fj * y;
      acceleration[j].z -= fj * z;
    }
  }
}

void Solar_Step(Solar_State *state, double days) {
  Solar_Vector acceleration[SOLAR_BODY_COUNT];

  accelerations(state, acceleration);
  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    state->velocity[i].x += .5 * days * acceleration[i].x;
    state->velocity[i].y += .5 * days * acceleration[i].y;
    state->velocity[i].z += .5 * days * acceleration[i].z;
    state->position[i].x += days * state->velocity[i].x;
    state->position[i].y += days * state->velocity[i].y;
    state->position[i].z += days * state->velocity[i].z;
  }

  accelerations(state, acceleration);
  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    state->velocity[i].x += .5 * days * acceleration[i].x;
    state->velocity[i].y += .5 * days * acceleration[i].y;
    state->velocity[i].z += .5 * days * acceleration[i].z;
  }

  state->days += days;
}

void Solar_Advance(Solar_State *state, double days, double fixed_step) {
  if (!state || !isfinite(days) || days < 0 || !isfinite(fixed_step) || fixed_step <= 0)
    return;

  state->accumulator += days;
  while (state->accumulator + 1e-12 >= fixed_step) {
    Solar_Step(state, fixed_step);
    state->accumulator -= fixed_step;
  }
}

double Solar_Energy(const Solar_State *state) {
  double energy = 0;

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    Solar_Vector v = state->velocity[i];
    double mass = Solar_Planets[i].mass_ratio;

    energy += .5 * mass * (v.x * v.x + v.y * v.y + v.z * v.z);
    for (size_t j = i + 1; j < SOLAR_BODY_COUNT; ++j) {
      double x = state->position[j].x - state->position[i].x;
      double y = state->position[j].y - state->position[i].y;
      double z = state->position[j].z - state->position[i].z;

      energy -= SOLAR_MU * mass * Solar_Planets[j].mass_ratio / sqrt(x * x + y * y + z * z);
    }
  }

  return energy;
}

Solar_Vector Solar_AngularMomentum(const Solar_State *state) {
  Solar_Vector angular = {0};

  for (size_t i = 0; i < SOLAR_BODY_COUNT; ++i) {
    Solar_Vector r = state->position[i], v = state->velocity[i];
    double mass = Solar_Planets[i].mass_ratio;

    angular.x += mass * (r.y * v.z - r.z * v.y);
    angular.y += mass * (r.z * v.x - r.x * v.z);
    angular.z += mass * (r.x * v.y - r.y * v.x);
  }

  return angular;
}
