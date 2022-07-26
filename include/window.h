#ifndef INCLUDE_WINDOW_H
#define INCLUDE_WINDOW_H

#include <stdbool.h>

struct Window_;
typedef struct Window_* Window;

struct WindowSize {
    int w;
    int h;
};

Window create_window(struct WindowSize window_size);
void update_display(Window window);
struct WindowSize get_window_size(Window window);
void delete_window(Window window);

#endif
