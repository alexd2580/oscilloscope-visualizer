#ifndef INCLUDE_DEFINES_H
#define INCLUDE_DEFINES_H

#define PI 3.14159f

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, l, u) fmaxf((l), fminf((x), (u)))

#define COMPILE_TIME_ASSERT(e) extern char ct_assert2[(e) - 1]

#define isizeof(x) ((int)sizeof(x))

#endif
