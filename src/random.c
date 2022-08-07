#include <stdint.h>
#include <stdlib.h>

#include "buffers.h"
#include "globals.h"
#include "random.h"
#include "size.h"

struct Random_ {
    float* seed;

    unsigned int index;
    Buffer buffer;
};

static uint64_t splitmix_seed;
uint64_t splitmix_next(void) {
    uint64_t z = (splitmix_seed += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

union X {
    uint64_t u64;
    float f32;
};

void initialize_random(Random random, struct Size size) {
    int num_values = size.w * size.h;
    random->seed = ALLOCATE(num_values, float);
    FORI(0, num_values) {
        union X x;
        x.u64 = splitmix_next();
        random->seed[i] = x.f32;
    }

    int gpu_buffer_size = num_values * isizeof(float);
    random->buffer = create_storage_buffer(gpu_buffer_size, random->index);
    copy_buffer_to_gpu(random->buffer, random->seed, 0, gpu_buffer_size);
}

Random create_random(struct Size size, unsigned int index) {
    Random random = (Random)malloc(sizeof(struct Random_));
    random->index = index;
    initialize_random(random, size);
    return random;
}

void deinitialize_random(Random random) {
    delete_buffer(random->buffer);
    free(random->seed);
}

void update_random_window_size(Random random, struct Size size) {
    deinitialize_random(random);
    initialize_random(random, size);
}

void delete_random(Random random) {
    deinitialize_random(random);
    free(random);
}
