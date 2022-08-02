#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "defines.h"
#include "sdl.h"
#include "textures.h"
#include "window.h"

struct Textures_ {
    GLuint front;
    GLuint back;

    struct Size size;
};

void init_tex_params(GLuint texture, struct Size size) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, size.w, size.h, 0, GL_RGBA, GL_FLOAT, NULL);
}

void initialize_textures(Textures textures) {
    glGenTextures(1, &textures->front);
    init_tex_params(textures->front, textures->size);
    glGenTextures(1, &textures->back);
    init_tex_params(textures->back, textures->size);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Textures create_textures(struct Size size) {
    Textures textures = (Textures)malloc(sizeof(struct Textures_));
    textures->size = size;
    initialize_textures(textures);
    return textures;
}

__attribute__((pure)) struct Size get_texture_size(Textures textures) { return textures->size; }

void deinitialize_textures(Textures textures) {
    glDeleteTextures(1, &textures->front);
    glDeleteTextures(1, &textures->back);
}

void delete_textures(Textures textures) {
    deinitialize_textures(textures);
    free(textures);
}

void update_textures_window_size(Textures textures, struct Size size) {
    deinitialize_textures(textures);
    textures->size = size;
    initialize_textures(textures);
}

void swap_and_bind_textures(Textures textures) {
    GLuint tmp = textures->front;
    textures->front = textures->back;
    textures->back = tmp;

    glBindImageTexture(0, textures->back, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, textures->front, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
}

__attribute__((pure)) GLuint get_back_texture(Textures textures) { return textures->back; }
