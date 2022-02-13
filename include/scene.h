#ifndef INCLUDE_SCENE_H
#define INCLUDE_SCENE_H

#include "vec.h"

struct Scene_;
typedef struct Scene_* Scene;

enum PrimitiveType {
    PlaneType = 1,
    CubeType = 2,
    SphereType = 3,
    CylinderType = 4,
    TranslationType = 5,
    RotationType = 6,
    ScalingType = 7,
    RepetitionType = 8,
    UnionType = 9,
    IntersectionType = 10,
    ComplemenType = 11,
};

struct Primitive {
    enum PrimitiveType type;
    int i1;
    int i2;
    int i3;
    float f1;
    float f2;
    float f3;
    float f4;

    // Actual primitives
    // PlaneType
    //  f1: <offset>
    // CubeType
    //  f1: <side length>
    // SphereType
    //  f1: <radius>
    // CylinderType
    //  f1: <radius>

    // // Transformation primitives
    // TranslationType
    //  int f1: <translation x>
    //  int f2: <translation y>
    //  int f3: <translation z>
    // RotationType
    //  int f1: <rotation x>
    //  int f2: <rotation y>
    //  int f3: <rotation z>
    // ScalingType
    //  int f1: <scaling x>
    //  int f2: <scaling y>
    //  int f3: <scaling z>
    // RepetitionType
    //  int i1: <rep x>
    //  int i2: <rep y>
    //  int i3: <rep z>
    //  int f1: <size x>
    //  int f2: <size y>
    //  int f3: <size z>

    // // Combination primitives
    // UnionType
    //  i1: <a>
    //  i2: <b>
    // IntersectionType
    //  i1: <a>
    //  i2: <b>
    // ComplemenType
    //  i1: <a>
    //  i2: <b>
};

Scene create_scene(unsigned int index);
int add_primitive(Scene scene, struct Primitive primitive);
void set_root_primitive(Scene scene, int root_primitive);

float distance_to_scene(Scene scene, Vec3 vec);

void copy_scene_to_gpu(Scene scene);
void delete_scene(Scene scene);

#endif
