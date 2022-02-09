// Compile with gcc -std=c11 ringbuffer.c -lrt
//
// Required for ftruncate...
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <string.h>

// shm_open
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>

struct RingBuffer {
    int mmap_size;
    void* mmap_a;
    void* mmap_b;

    int size;
    int offset;
};

struct RingBuffer create_ring_buffer(int size) {
    int page_size = sysconf(_SC_PAGESIZE);
    assert(size % page_size == 0);

    struct RingBuffer ring_buffer;
    ring_buffer.size = size;

    // Get some memory.
    int fd = shm_open("/example", O_RDWR | O_CREAT | O_EXCL, 0600);
    shm_unlink("/example");

    // Tell the os that we need `size` of memory.
    ftruncate(fd, size);

    // Allocate a page_aligned buffer of twice the size to reserve
    // a contiguous address space for the memory mapping.
    void* target = mmap(NULL, 2 * size, PROT_NONE, MAP_PRIVATE, fd, 0);
    munmap(target, 2 * size);

    // Now map the fd into the first and second sections of `target_aligned`.
    ring_buffer.mmap_size = size;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_FIXED;
    ring_buffer.mmap_a = mmap(target, size, prot, flags, fd, 0);
    ring_buffer.mmap_b = mmap(target + size, size, prot, flags, fd, 0);
    close(fd);

    return ring_buffer;
}

void memcpy_to_ringbuffer(struct RingBuffer* ring_buffer, void* data, int size) {
    int write_size = MIN(size, ring_buffer->size);
    int offset = (ring_buffer->offset + MAX(0, size - ring_buffer->size)) % ring_buffer->size;
    memcpy(ring_buffer->mmap_a + offset, data, write_size);
    ring_buffer->offset = (offset + write_size) % ring_buffer->size;
}

void delete_ring_buffer(struct RingBuffer ring_buffer) {
    munmap(ring_buffer.mmap_a, ring_buffer.mmap_size);
    munmap(ring_buffer.mmap_b, ring_buffer.mmap_size);
}

int main(void) {
    int size = 4 * 4096;
    int* ints = malloc(size * sizeof(int));
    for(int i = 0; i < size; i++) {
        ints[i] = i;
    }

    struct RingBuffer ring_buffer = create_ring_buffer(size * sizeof(int));
    ring_buffer.offset = 5 * sizeof(int);

    memcpy_to_ringbuffer(&ring_buffer, ints, size * sizeof(int));

    int* ring_ints = (int*)ring_buffer.mmap_a;

    for(int i = 0; i < 2 * size; i++) {
        assert(ints[i % size] == ring_ints[(i + 5) % size]);
    }

    delete_ring_buffer(ring_buffer);
    free(ints);
    return 0;
}
