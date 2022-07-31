#ifndef INCLUDE_RANDOM_H
#define INCLUDE_RANDOM_H

#include "buffers.h"
#include "size.h"

struct Random_;
typedef struct Random_* Random;

void initialize_random(Random random, struct Size size);
Random create_random(struct Size window_size, unsigned int index);
void deinitialize_random(Random random);
void update_random_window_size(Random random, struct Size size);
void delete_random(Random random);

#endif
