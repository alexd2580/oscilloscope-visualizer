#include <math.h>

#include "globals.h"
#include "vec.h"

#define MARGS(...) __VA_ARGS__
#define LAMBDA(RTYPE, FNAME, ARGS, EXPR)                                                                               \
    __attribute__((const)) RTYPE FNAME(ARGS) { return EXPR; }

LAMBDA(Vec3, vec3, MARGS(float x, float y, float z), ((Vec3){.x = x, .y = y, .z = z, ._pad = 0}))
LAMBDA(Vec3, scale, MARGS(Vec3 vec, float factor), vec3(vec.x* factor, vec.y* factor, vec.z* factor))
LAMBDA(Vec3, add, MARGS(Vec3 a, Vec3 b), vec3(a.x + b.x, a.y + b.y, a.z + b.z))
LAMBDA(Vec3, add4, MARGS(Vec3 a, Vec3 b, Vec3 c, Vec3 d),
       vec3(a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y, a.z + b.z + c.z + d.z))

LAMBDA(float, vlength, Vec3 v, sqrtf(dot(v, v)))
LAMBDA(Vec3, vnormalize, Vec3 v, scale(v, 1.0f / vlength(v)))
LAMBDA(float, dot, MARGS(Vec3 a, Vec3 b), a.x* b.x + a.y * b.y + a.z * b.z)
LAMBDA(Vec3, vabs, Vec3 a, vec3(fabsf(a.x), fabsf(a.y), fabsf(a.z)))
LAMBDA(Vec3, vmin, MARGS(Vec3 a, float f), vec3(fminf(a.x, f), fminf(a.y, f), fminf(a.z, f)))
LAMBDA(Vec3, vmax, MARGS(Vec3 a, float f), vec3(fmaxf(a.x, f), fmaxf(a.y, f), fmaxf(a.z, f)))
LAMBDA(float, maxcomp, Vec3 a, fmaxf(a.x, fmaxf(a.y, a.z)))

#undef LAMBDA
#undef MARGS
