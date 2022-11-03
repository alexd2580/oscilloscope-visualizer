#ifndef INCLUDE_TEXTURES_H
#define INCLUDE_TEXTURES_H

#include <SDL2/SDL_opengl.h>

#include "size.h"

typedef struct Textures_* Textures;

Textures create_textures(struct Size size, int basis);
void swap_and_bind_textures(Textures textures);
void update_textures_window_size(Textures textures, struct Size size);
void delete_textures(Textures textures);
__attribute__((pure)) GLuint get_present_texture(Textures textures);

#endif
