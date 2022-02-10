#ifndef INCLUDE_VEC_H
#define INCLUDE_VEC_H

struct Vec3_ {
    float x;
    float y;
    float z;
    float _pad;
};
typedef struct Vec3_ Vec3;

__attribute__((const)) Vec3 vec3(float x, float y, float z) { return (Vec3){.x = x, .y = y, .z = z, ._pad = 0}; }

__attribute__((const)) Vec3 scale(Vec3 vec, float factor) {
    return vec3(vec.x * factor, vec.y * factor, vec.z * factor);
}

__attribute__((const)) Vec3 add(Vec3 a, Vec3 b) { return vec3(a.x + b.x, a.y + b.y, a.z + b.z); }

__attribute__((const)) Vec3 add4(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    return vec3(a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y, a.z + b.z + c.z + d.z);
}

#endif
