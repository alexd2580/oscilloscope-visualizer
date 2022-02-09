#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL_opengl.h>

#include "buffers.h"

Buffer create_buffer(GLenum target, int size, unsigned int index) {
    Buffer buffer;
    buffer.target = target;
    glGenBuffers(1, &buffer.buffer);
    glBindBuffer(target, buffer.buffer);
    glBufferData(target, size, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(target, index, buffer.buffer);
    glBindBuffer(target, 0);
    return buffer;
}

Buffer create_uniform_buffer(int size, unsigned int index) {
    return create_buffer(GL_UNIFORM_BUFFER, size, index);
}

Buffer create_storage_buffer(int size, unsigned int index) {
    return create_buffer(GL_SHADER_STORAGE_BUFFER, size, index);
}

void copy_buffer_to_gpu(Buffer buffer, void* data, int buffer_offset, int size) {
    glBindBuffer(buffer.target, buffer.buffer);
    glBufferSubData(buffer.target, buffer_offset, size, data);
    glBindBuffer(buffer.target, 0);
}

void copy_ringbuffer_to_gpu(Buffer buffer, void* data, int buffer_offset, int size, int wrap_offset) {
    int tail_size = size - wrap_offset;
    copy_buffer_to_gpu(buffer, ((char*)data) + wrap_offset, buffer_offset, tail_size);
    copy_buffer_to_gpu(buffer, data, buffer_offset + tail_size, wrap_offset);
}

void delete_buffer(Buffer buffer) {
    glDeleteBuffers(1, &buffer.buffer);
}
