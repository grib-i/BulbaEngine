#ifndef BULBA_TEST_H
#define BULBA_TEST_H

#include "bulba/bulba.h"
#include "bulba/core/math3v/ray.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CHECK(expr) Test_Check((expr), #expr, __FILE__, __LINE__)
#define TEST_NEAR(a, b, eps) TEST_CHECK(fabs((double)(a) - (double)(b)) <= (eps))
#define TEST_REQUIRE(expr)                                                                                                                           \
  do {                                                                                                                                               \
    if (!TEST_CHECK(expr))                                                                                                                           \
      return;                                                                                                                                        \
  } while (0)
#define TEST_CASE(name)                                                                                                                              \
  {                                                                                                                                                  \
#name, name                                                                                                                                      \
  }

typedef void (*Test_Function)(void);
typedef struct {
  const char *name;
  Test_Function function;
} Test_Case;
typedef struct {
  const char *name;
  const Test_Case *cases;
  size_t count;
} Test_Suite;

typedef struct Test_Settings {
  unsigned repetitions;
  unsigned samples;
  unsigned iterations;
  unsigned cycles;
  unsigned ticks;
} Test_Settings;

Test_Settings Test_DefaultSettings(void);
int Test_RunConfigured(const char *suite, const Test_Settings *settings);
void Test_Initialize(void);
const char *Test_Font(void);
bool Test_Check(bool success, const char *expression, const char *file, int line);
int Test_Run(const char *suite);
int Test_Benchmarks(const Test_Settings *settings);
int Test_Lifecycle(const Test_Settings *settings);
void Test_Destroy2D(BLB_Object2D *object);
void Test_Destroy3D(BLB_Object3D *object);
const Test_Suite *Test_GetSuites(size_t *count);

#endif
