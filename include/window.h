#ifndef INCLUDE_WINDOW_H
#define INCLUDE_WINDOW_H

#include <stdbool.h>

#include <SDL2/SDL_opengl.h>

#include "size.h"

struct Window_;
typedef struct Window_* Window;

Window create_window(struct Size size);
void display_texture(Window window, GLuint texture, struct Size size);
struct Size get_window_size(Window window);
void delete_window(Window window);

#endif
