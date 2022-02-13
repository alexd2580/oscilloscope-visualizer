#ifndef INCLUDE_TIMER_H
#define INCLUDE_TIMER_H

struct Timer_;
typedef struct Timer_* Timer;

Timer create_timer(unsigned int index);
void copy_timer_to_gpu(Timer time);
void delete_timer(Timer time);

#endif
