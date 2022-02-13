#ifndef INCLUDE_VEC_H
#define INCLUDE_VEC_H

struct Vec3_ {
    float x;
    float y;
    float z;
    float _pad;
};
typedef struct Vec3_ Vec3;

__attribute__((const)) Vec3 vec3(float x, float y, float z);
__attribute__((const)) Vec3 scale(Vec3 vec, float factor);
__attribute__((const)) Vec3 add(Vec3 a, Vec3 b);
__attribute__((const)) Vec3 add4(Vec3 a, Vec3 b, Vec3 c, Vec3 d);
__attribute__((const)) float dot(Vec3 a, Vec3 b);

#endif
