#ifndef INCLUDE_DEFINES_H
#define INCLUDE_DEFINES_H

#define PI 3.14159f

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, l, u) MAX((l), MIN((x), (u)))

#define COMPILE_TIME_ASSERT(e) extern char ct_assert2[(e) - 1]

#define isizeof(x) ((int)sizeof(x))

#endif
