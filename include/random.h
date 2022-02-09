#ifndef INCLUDE_RANDOM_H
#define INCLUDE_RANDOM_H

#include "buffers.h"
#include "window.h"

struct Random_;
typedef struct Random_* Random;

void initialize_random(Random random, struct WindowSize window_size);
Random create_random(struct WindowSize window_size);
void deinitialize_random(Random random);
void reinitialize_random(Random random, struct WindowSize window_size);
void delete_random(Random random);

#endif
