#ifndef INCLUDE_VIEW_H
#define INCLUDE_VIEW_H

#include "window.h"

struct View_;
typedef struct View_* View;

View create_view(struct WindowSize window_size, unsigned int index);
void copy_view_to_gpu(View view);
void update_view_window_size(View view, struct WindowSize window_size);
int* mut_num_steps(View view);
float* mut_fovy(View view);
void move_camera(View view, float forward, float right, float up);
void rotate_camera(View view, float pitch, float roll, float yaw);
void delete_view(View view);

#endif
