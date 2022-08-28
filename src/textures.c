#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "globals.h"
#include "sdl.h"
#include "textures.h"
#include "window.h"

struct Textures_ {
    // Output texture will be shown on-screen.
    GLuint present;

    // Auxiliary textures.
    GLuint back_a;
    GLuint front_a;
    GLuint back_b;
    GLuint front_b;
    GLuint back_c;
    GLuint front_c;

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
    // Just hope for the best lol.
    GLuint* t = &textures->present;
    glGenTextures(7, t);
    FORI(0, 7) {
        init_tex_params(t[i], textures->size);
    }
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
    // Just hope for the best again rofl.
    glDeleteTextures(3, &textures->present);
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

void swap(GLuint* a, GLuint* b) {
    GLuint tmp = *a;
    *a = *b;
    *b = tmp;
}

void swap_and_bind_textures(Textures textures) {
    swap(&textures->back_a, &textures->front_a);
    swap(&textures->back_b, &textures->front_b);
    swap(&textures->back_c, &textures->front_c);

    glBindImageTexture(0, textures->present, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, textures->back_a, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(2, textures->front_a, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(3, textures->back_b, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(4, textures->front_b, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(5, textures->back_c, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(6, textures->front_c, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

__attribute__((pure)) GLuint get_present_texture(Textures textures) { return textures->present; }
