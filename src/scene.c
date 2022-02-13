#include <stdint.h>
#include <stdlib.h>

#include "buffers.h"
#include "vec.h"
#include "defines.h"
#include "scene.h"

struct Scene_ {
    struct Header_ {
        int num_max_primitives;
        int num_primitives;
        int root_node;

        int _padding;
    } header;

    struct Primitive* primitives;

    unsigned int index;
    Buffer buffer;
};

void initialize_gpu_buffer(Scene scene) {
    int gpu_header_buffer_size = isizeof(struct Header_);
    int gpu_primitives_buffer_size = scene->header.num_max_primitives * isizeof(struct Primitive);

    scene->buffer = create_storage_buffer(gpu_header_buffer_size + gpu_primitives_buffer_size, scene->index);
}

void deinitialize_gpu_buffer(Scene scene) {
    delete_buffer(scene->buffer);
}

void initialize_cpu_buffer(Scene scene, int num_max_primitives) {
    scene->header.num_max_primitives = num_max_primitives;
    int cpu_primitives_buffer_size = scene->header.num_max_primitives * isizeof(struct Primitive);
    scene->primitives = (struct Primitive*)malloc((size_t)cpu_primitives_buffer_size);
}

int printf(char const*, ...);
void grow_buffer(Scene scene, int num_max_primitives) {
    printf("lol");
    exit(15);
}

void deinitialize_cpu_buffer(Scene scene) {
    free(scene->primitives);
}

Scene create_scene(unsigned int index) {
    Scene scene = (Scene)malloc(sizeof(struct Scene_));
    scene->header.num_primitives = 0;
    scene->index = index;
    initialize_cpu_buffer(scene, 100);
    int root = add_primitive(scene, (struct Primitive){ .type = PlaneType, .f1 = 0.f });
    set_root_primitive(scene, root);
    initialize_gpu_buffer(scene);
    return scene;
}

int add_primitive(Scene scene, struct Primitive primitive) {
    if (scene->header.num_primitives == scene->header.num_max_primitives) {
        grow_buffer(scene, scene->header.num_max_primitives + 100);
    }

    scene->primitives[scene->header.num_primitives] = primitive;
    return scene->header.num_primitives++;
}

void set_root_primitive(Scene scene, int root_primitive) {
    scene->header.root_node = root_primitive;
}

float length(Vec3 vec) {
    return sqrt(dot(vec, vec));
}

float sdf_sphere(Vec3 pos, float radius) {
    return length(pos) - radius;
}

float sdf_cube(Vec3 pos, float side_len) {
    vec3 to_corner = abs(pos) - vec3(side_len / 2);
    return length(max(to_corner, 0)) + min(maxcomp(to_corner), 0);
}

float sdf_cylinder(vec3 pos, float radius) {
    return length(pos.yz) - radius;
}

float sdf_plane(vec3 pos, float offset) {
    return pos.y - offset;
}

float sdf_subtract(float a, float b) {
    return max(a, -b);
}

float sdf_union(float a, float b) {
    return min(a, b);
}

float sdf_intersect(float a, float b) {
    return max(a, b);
}

float sdf_union_4(float a, float b, float c, float d) {
    return min(min(a, b), min(c, d));
}

float distance_to_primitive(Scene scene, Vec3 vec, int index) {
    struct Primitive primitive = scene->primitives[index];
    switch(primitive.type) {
    case PlaneType:
        return sdf_plane(vec, primitive.f1);
    case CubeType:
        return sdf_cube(vec, primitive.f1);
    case SphereType:
        return sdf_sphere(vec, primitive.f1);
    case CylinderType:
        return sdf_cylinder(vec, primitive.f1);
    case TranslationType:
        break;
    case RotationType:
        break;
    case ScalingType:
        break;
    case RepetitionType:
        break;
    case UnionType:
        return sdf_union(distance_to_primitive(scene, vec, primitive.i1), distance_to_primitive(scene, vec, primitive.i2));
    case IntersectionType:
        return sdf_intersection(distance_to_primitive(scene, vec, primitive.i1), distance_to_primitive(scene, vec, primitive.i2));
    case ComplemenType:
        return sdf_complement(distance_to_primitive(scene, vec, primitive.i1), distance_to_primitive(scene, vec, primitive.i2));
    default:
        return 0.0f / 0.0f;
    }
}

float distance_to_scene(Scene scene, Vec3 vec) {
    return distance_to_primitive(scene, vec, scene->root_node);
}

void copy_scene_to_gpu(Scene scene) {
    int gpu_header_buffer_size = isizeof(struct Header_);
    int gpu_primitives_buffer_size = scene->header.num_max_primitives * isizeof(struct Primitive);
    copy_buffer_to_gpu(scene->buffer, &scene->header, 0, gpu_header_buffer_size);
    copy_buffer_to_gpu(scene->buffer, scene->primitives, gpu_header_buffer_size, gpu_primitives_buffer_size);
}

void delete_scene(Scene scene) {
    deinitialize_gpu_buffer(scene);
    deinitialize_cpu_buffer(scene);
    free(scene);
}
