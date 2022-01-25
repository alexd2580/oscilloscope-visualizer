#ifndef INCLUDE_BUFFERS_H
#define INCLUDE_BUFFERS_H

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL_opengl.h>

void copy_buffer_to_gpu(GLuint buffer, char* data, int buffer_offset, int size);
void copy_ringbuffer_to_gpu(GLuint buffer, char* data, int buffer_offset, int size, int wrap_offset);

#endif
