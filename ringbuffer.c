// Required for ftruncate...
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include<string.h>

// shm_open
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include<assert.h>

struct RingBuffer {
    int mmap_size;
    void* mmap_a;
    void* mmap_b;

    int size;
    void* data;
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

    // To simplify things we use multiples of page_size.
    int alloc_size = (size / page_size) * page_size + page_size;

    // Tell the os that we need `size` of memory.
    ftruncate(fd, alloc_size);

    // Allocate a page_aligned buffer of twice the size (+ page_size) to reserve
    // a contiguous address space for the memory mapping.
    void* target = mmap(NULL, 2 * alloc_size, PROT_NONE, MAP_PRIVATE, fd, 0);
    perror("target");
    munmap(target, 2 * alloc_size);

    // Now map the fd into the first and second sections of `target_aligned`.
    ring_buffer.mmap_size = alloc_size;
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_FIXED;
    ring_buffer.mmap_a = mmap(target, alloc_size, prot, flags, fd, 0);
    ring_buffer.mmap_b = mmap(target + alloc_size, alloc_size, prot, flags, fd, 0);
    close(fd);

    // To make the buffer wrap around at exactly `size` bytes, we need
    // to return a pointer that is offset by `page_size - (size % page_size)`.
    ring_buffer.data = ring_buffer.mmap_a + page_size - (size % page_size);

    return ring_buffer;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

void memcpy_to_ringbuffer(struct RingBuffer* ring_buffer, void* data, int size) {
    int write_size = MIN(size, ring_buffer->size);
    int offset = (ring_buffer->offset + MAX(0, size - ring_buffer->size)) % ring_buffer->size;
    memcpy(ring_buffer->data + offset, data, write_size);
    ring_buffer->offset = (offset + write_size) % ring_buffer->size;
}

void delete_ring_buffer(struct RingBuffer ring_buffer) {
    munmap(ring_buffer.mmap_a, ring_buffer.mmap_size);
    munmap(ring_buffer.mmap_b, ring_buffer.mmap_size);
}

int main(void) {
    int size = 10;
    int* ints = malloc(size * sizeof(int));
    for (int i=0; i < size; i++) {
        ints[i] = i;
    }

    struct RingBuffer ring_buffer = create_ring_buffer(size * sizeof(int));
    ring_buffer.offset = (size - 5) * sizeof(int);

    memcpy_to_ringbuffer(&ring_buffer, ints, size * sizeof(int));

    int* ring_ints = (int*)ring_buffer.data;

    for (int i=0; i<size; i++) {
        assert(ring_ints[i] == ints[(i + size) % size]);
    }

    delete_ring_buffer(ring_buffer);
    free(ints);
    return 0;
}
