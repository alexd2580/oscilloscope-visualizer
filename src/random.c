#include <stdint.h>
#include <stdlib.h>

#include "buffers.h"
#include "defines.h"
#include "random.h"
#include "window.h"

struct Random_ {
    uint64_t* seed;

    Buffer buffer;
};

static uint64_t splitmix_seed;
uint64_t splitmix_next(void) {
    uint64_t z = (splitmix_seed += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
}

void initialize_random(Random random, struct WindowSize window_size) {
    int num_uints = 4 * window_size.w * window_size.h;
    random->seed = malloc((unsigned int)num_uints * sizeof(uint64_t));
    for(int i = 0; i < num_uints; i++) {
        random->seed[i] = splitmix_next();
    }

    int gpu_buffer_size = num_uints * isizeof(float);
    random->buffer = create_storage_buffer(gpu_buffer_size, 4);
    copy_buffer_to_gpu(random->buffer, random->seed, 0, gpu_buffer_size);
}

Random create_random(struct WindowSize window_size) {
    Random random = (Random)malloc(sizeof(struct Random_));
    initialize_random(random, window_size);
    return random;
}

void deinitialize_random(Random random) {
    delete_buffer(random->buffer);
    free(random->seed);
}

void update_random_window_size(Random random, struct WindowSize window_size) {
    deinitialize_random(random);
    initialize_random(random, window_size);
}

void delete_random(Random random) {
    deinitialize_random(random);
    free(random);
}
