#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>

#include "render_quad.h"

struct RenderQuad_ {
    GLuint vao;
    GLuint vbo;
};

RenderQuad create_render_quad(void) {
    RenderQuad render_quad = (RenderQuad)malloc(sizeof(struct RenderQuad_));

    // Why do i need this still?
    glGenVertexArrays(1, &render_quad->vao);
    glBindVertexArray(render_quad->vao);

    glGenBuffers(1, &render_quad->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, render_quad->vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

#define XY(x, y) x, y
    const float vertex_buffer_data[] = {XY(-1, -1), XY(1, -1), XY(1, 1), XY(-1, -1), XY(1, 1), XY(-1, 1)};
#undef XY

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    return render_quad;
}

void delete_render_quad(RenderQuad primitives_buffers) {
    glDeleteBuffers(1, &primitives_buffers->vbo);
    glDeleteVertexArrays(1, &primitives_buffers->vao);
    free(primitives_buffers);
}
