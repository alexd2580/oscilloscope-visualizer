#ifndef INCLUDE_BUFFERS_H
#define INCLUDE_BUFFERS_H

// For reference, see:
// https://blog.techlab-xe.net/wp-content/uploads/2013/12/fig1_uniform_buffer.png

struct Buffer_ {
    unsigned int target; // GLenum
    unsigned int buffer; // GLuint
};
typedef struct Buffer_ Buffer;

Buffer create_uniform_buffer(int size, unsigned int index);
Buffer create_storage_buffer(int size, unsigned int index);

void copy_buffer_to_gpu(Buffer, void* data, int buffer_offset, int size);
void copy_ringbuffer_to_gpu(Buffer, void* data, int buffer_offset, int size, int wrap_offset);
void delete_buffer(Buffer);

#endif
