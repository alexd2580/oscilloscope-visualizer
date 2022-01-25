#include "buffers.h"

void copy_buffer_to_gpu(GLuint buffer, char* data, int buffer_offset, int size) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, buffer_offset, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void copy_ringbuffer_to_gpu(GLuint buffer, char* data, int buffer_offset, int size, int wrap_offset) {
    int tail_size = size - wrap_offset;
    copy_buffer_to_gpu(buffer, data + wrap_offset, buffer_offset, tail_size);
    copy_buffer_to_gpu(buffer, data, buffer_offset + tail_size, wrap_offset);
}
