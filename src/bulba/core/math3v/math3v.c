#include "bulba/core/math3v/math3v.h"

#include <stdarg.h>

int max(const int *numbers, ...) {
  if (numbers == NULL)
    return 0;

  int max_val = numbers[0];
  va_list args;
  va_start(args, numbers);

  const int *next;
  while ((next = va_arg(args, const int *)) != NULL) {
    if (*next > max_val) {
      max_val = *next;
    }
  }

  va_end(args);
  return max_val;
}
