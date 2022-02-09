#ifndef INCLUDE_VIEW_H
#define INCLUDE_VIEW_H

#include "window.h"

struct View_;
typedef struct View_* View;

View create_view(Window window);
void copy_view_to_gpu(View view);
void move_camera(View view, float forward, float right, float up);
void rotate_camera(View view, float pitch, float roll, float yaw);
void delete_view(View view);

#endif
