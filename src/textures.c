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
    GLuint back;
    GLuint front;

    int basis;
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
    // Check the texture size.
    glEnable(GL_TEXTURE_2D);
    int max_texture_size = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

    int w = textures->size.w * textures->basis;
    int h = textures->size.h * textures->basis;

    if (w >= max_texture_size || h >= max_texture_size) {
        printf("Can't create buffer texture with %d times the size of the screen\n", textures->basis);
        printf("Screen (w, h): %d, %d; Max texture side length: %d\n", textures->size.w, textures->size.h, max_texture_size);
        exit(1);
    }

    struct Size buffer_tex_size = { .w = w, .h = h};

    // Just hope for the best lol.
    GLuint* t = &textures->present;
    glGenTextures(3, t);
    init_tex_params(t[0], textures->size);
    init_tex_params(t[1], buffer_tex_size);
    init_tex_params(t[2], buffer_tex_size);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Textures create_textures(struct Size size, int basis) {
    Textures textures = (Textures)malloc(sizeof(struct Textures_));
    textures->basis = basis;
    textures->size = size;

    initialize_textures(textures);
    return textures;
}

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
    swap(&textures->back, &textures->front);

    glBindImageTexture(0, textures->present, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, textures->back, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(2, textures->front, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
}

__attribute__((pure)) GLuint get_present_texture(Textures textures) { return textures->present; }
