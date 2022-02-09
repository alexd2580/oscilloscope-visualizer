#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "buffers.h"
#include "pcm.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, l, u) MAX((l), MIN((x), (u)))

struct Pcm_ {
    int num_samples;

    int sample_index;
    float* ring_left;
    float* ring_right;
    int offset;

    GLuint gpu_buffer;
};

Pcm create_pcm(int num_samples) {
    Pcm pcm = (struct Pcm_*)malloc(sizeof(struct Pcm_));
    pcm->num_samples = num_samples;
    pcm->sample_index = 0;
    pcm->ring_left = malloc(num_samples * sizeof(float));
    pcm->ring_right = malloc(num_samples * sizeof(float));
    pcm->offset = 0;

    for(int i = 0; i < num_samples; i++) {
        pcm->ring_left[i] = 0.0f;
        pcm->ring_right[i] = 0.0f;
    }

    glGenBuffers(1, &pcm->gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pcm->gpu_buffer);
    int gpu_buffer_size = 3 * sizeof(int) + 2 * num_samples * sizeof(float);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(int), &num_samples);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pcm->gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return pcm;
}

void copy_pcm_to_gpu(Pcm pcm) {
    int sample_index = pcm->sample_index;
    int offset = pcm->offset;

    GLuint buffer = pcm->gpu_buffer;
    char* left = (char*)pcm->ring_left;
    char* right = (char*)pcm->ring_right;
    int pcm_size = pcm->num_samples * sizeof(float);
    int pcm_wrap_offset = offset * sizeof(float);

    copy_buffer_to_gpu(buffer, (char*)&sample_index, sizeof(int), sizeof(int));

    copy_ringbuffer_to_gpu(buffer, left, 2 * sizeof(int), pcm_size, pcm_wrap_offset);
    copy_ringbuffer_to_gpu(buffer, right, 2 * sizeof(int) + pcm_size, pcm_size, pcm_wrap_offset);
}

void copy_pcm_mono_to_buffer(float* dst, Pcm pcm, int num_floats) {
    int offset = pcm->offset;

    int floats_to_start = MIN(offset, num_floats);
    int floats_from_end = num_floats - floats_to_start;
    memcpy(dst, pcm->ring_left + pcm->num_samples - floats_from_end, floats_from_end * sizeof(float));
    float* second_half = pcm->ring_left + offset - floats_to_start;
    memcpy(dst + floats_from_end, second_half, floats_to_start * sizeof(float));
}

void delete_pcm(Pcm pcm) {
    glDeleteBuffers(1, &pcm->gpu_buffer);
    free(pcm->ring_left);
    free(pcm->ring_right);
    free(pcm);
}

struct ThreadData {
    bool close_requested;
    Pcm pcm;
};

void* input_stream_function(void* data_raw) {
    struct ThreadData* data = (struct ThreadData*)data_raw;
    Pcm pcm = data->pcm;

    // Initialize input byte buffer.
    int num_buffer_floats = 4096;
    int buffer_size = num_buffer_floats * sizeof(float);
    char* buffer_bytes = malloc(buffer_size + 1);
    float* buffer_floats = (float*)buffer_bytes;
    int buffer_offset = 0;

    // Unblock stdin.
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    while(!data->close_requested) {
        // Read as many as possible up to end of `buffer` from stdin.
        int res = read(STDIN_FILENO, buffer_bytes + buffer_offset, buffer_size - buffer_offset);
        if(res != -1) {
            buffer_offset += res;
        }

        int samples_available = (buffer_offset / 8);
        int samples_fitting = pcm->num_samples - pcm->offset;

        for(int i = 0; i < MIN(samples_fitting, samples_available); i++) {
            pcm->ring_left[pcm->offset + i] = buffer_floats[2 * i];
            pcm->ring_right[pcm->offset + i] = buffer_floats[2 * i + 1];
        }
        for(int i = samples_fitting; i < samples_available; i++) {
            pcm->ring_left[i] = buffer_floats[2 * i];
            pcm->ring_right[i] = buffer_floats[2 * i + 1];
        }

        pcm->sample_index += samples_available;
        pcm->offset = (pcm->offset + samples_available) % pcm->num_samples;

        // Move multiples of 2 floats over to ringbuffer and update position.
        // Can't use the memcpy aproach since i need to split the channels.
        /* float* dest = data->pcm_ring_buffer + data->pcm_offset; */
        /* memcpy(dest, buffer_bytes, floats_moved * sizeof(float)); */
        /* int floats_left = floats_available - floats_moved; */
        /* memcpy(data->pcm_ring_buffer, buffer_bytes + floats_moved * sizeof(float), floats_left * sizeof(float)); */
        /* data->pcm_offset = (data->pcm_offset + floats_available) % (data->pcm_samples * 2); */

        // Move rest of bytes to beginning of buffer.
        int bytes_processed = 2 * samples_available * sizeof(float);
        int bytes_left = buffer_offset - bytes_processed;
        memcpy(buffer_bytes, buffer_bytes + bytes_processed, bytes_left);
        buffer_offset = bytes_left;
    }

    free(buffer_bytes);

    return NULL;
}

struct PcmStream_ {
    pthread_t thread;
    struct ThreadData* thread_data;
};

PcmStream create_pcm_stream(Pcm pcm) {
    PcmStream pcm_stream = (struct PcmStream_*)malloc(sizeof(struct PcmStream_));
    pcm_stream->thread_data = malloc(sizeof(struct ThreadData));
    pcm_stream->thread_data->close_requested = false;
    pcm_stream->thread_data->pcm = pcm;

    int failure = pthread_create(&pcm_stream->thread, NULL, input_stream_function, (void*)pcm_stream->thread_data);
    if(failure) {
        fprintf(stderr, "Failed to pthread_create\n");
        exit(1);
    }

    return pcm_stream;
}

void delete_pcm_stream(PcmStream pcm_stream) {
    pcm_stream->thread_data->close_requested = true;
    pthread_join(pcm_stream->thread, NULL);
    free(pcm_stream->thread_data);
    free(pcm_stream);
}
