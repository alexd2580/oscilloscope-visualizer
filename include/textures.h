#ifndef INCLUDE_TEXTURES_H
#define INCLUDE_TEXTURES_H

#include <SDL2/SDL_opengl.h>

#include "size.h"

typedef struct Textures_* Textures;

Textures create_textures(struct Size size);
void swap_and_bind_textures(Textures textures);
void update_textures_window_size(Textures textures, struct Size size);
void delete_textures(Textures textures);
GLuint get_back_texture(Textures textures);

#endif
