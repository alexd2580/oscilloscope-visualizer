#ifndef INCLUDE_GLOBALS
#define INCLUDE_GLOBALS

#define PI 3.14159f

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define CLAMP(x, l, u) MAX((l), MIN((x), (u)))

#define COMPILE_TIME_ASSERT(e) extern char ct_assert2[(e) - 1]

#define isizeof(x) ((int)sizeof(x))

#define ALLOCATE(n, t) ((t*)malloc((size_t)(n) * sizeof(t)))

#define FORI(lo, hi) for(int i=(lo); i<(hi); i++)

__attribute__((const)) float mix(float a, float b, float x);

#endif
