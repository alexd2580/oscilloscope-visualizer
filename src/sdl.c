#include <sys/stat.h>
#include <sys/types.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_opengl.h>

#include "buffers.h"
#include "defines.h"

void create_sdl(void) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

/* RENDERING PRIMITIVES */

struct PrimitivesBuffers_ {
    GLuint vao;
    GLuint vbo;
};
typedef struct PrimitivesBuffers_* PrimitivesBuffers;

PrimitivesBuffers create_primitives_buffers(void) {
    PrimitivesBuffers primitives_buffers = (PrimitivesBuffers)malloc(sizeof(struct PrimitivesBuffers_));

    // Why do i need this still?
    glGenVertexArrays(1, &primitives_buffers->vao);
    glBindVertexArray(primitives_buffers->vao);

    glGenBuffers(1, &primitives_buffers->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, primitives_buffers->vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

#define XY(x, y) x, y
    const float vertex_buffer_data[] = {XY(-1, -1), XY(1, -1), XY(1, 1), XY(-1, -1), XY(1, 1), XY(-1, 1)};
#undef XY

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    return primitives_buffers;
}

void delete_sdl(Sdl sdl) {
    glDeleteBuffers(1, &sdl->vbo);
    glDeleteVertexArrays(1, &sdl->vao);
    free(primitives_buffers);
}

    SDL_Quit();

