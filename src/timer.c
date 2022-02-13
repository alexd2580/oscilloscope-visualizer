#include <stdlib.h>
#include <time.h>

#include "buffers.h"
#include "timer.h"

struct Timer_ {
    Buffer buffer;
};

Timer create_timer(unsigned int index) {
    Timer timer = (Timer)malloc(sizeof(struct Timer_));
    timer->buffer = create_uniform_buffer(sizeof(float), index);
    return timer;
}

void copy_timer_to_gpu(Timer timer) {
    long current_time = clock();
    float seconds = (float)current_time / CLOCKS_PER_SEC;
    copy_buffer_to_gpu(timer->buffer, &seconds, 0, sizeof(float));
}

void delete_timer(Timer timer) {
    delete_buffer(timer->buffer);
    free(timer);
}
