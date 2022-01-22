/*
    Minimal SDL2 + OpenGL3 example.
    Author: https://github.com/koute
    This file is in the public domain; you can do whatever you want with it.
    In case the concept of public domain doesn't exist in your jurisdiction
    you can also use this code under the terms of Creative Commons CC0 license,
    either version 1.0 or (at your option) any later version; for details see:
        http://creativecommons.org/publicdomain/zero/1.0/
    This software is distributed without any warranty whatsoever.
    Compile and run with: gcc opengl3_hello.c `sdl2-config --libs --cflags` -lGL -Wall && ./a.out
 *
 * Taken from https://gist.github.com/koute/7391344
*/

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h> // TODO
#include <unistd.h>
#include <assert.h>

#include <fftw3.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, l, u) MAX((l), MIN((x), (u)))

static const char* vertex_shader_path = "shader.vert";
static const char* fragment_shader_path = "shader.frag";

char* read_file(char const* path, int* size) {
    FILE* fp = fopen(path, "r");
    if(fp == NULL) {
        fprintf(stderr, "Cannot open file %s\n", path);
        exit(1);
    }

    if(fseek(fp, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Failed to fseek\n");
        exit(1);
    }

    long bufsize = ftell(fp);
    if(bufsize == -1) {
        fprintf(stderr, "Failed to ftell\n");
        exit(1);
    }

    if(size != NULL) {
        *size = bufsize;
    }

    char* data = malloc(sizeof(char) * (bufsize + 1));

    if(fseek(fp, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to fseek\n");
        exit(1);
    }

    fread(data, sizeof(char), bufsize, fp);
    if(ferror(fp) != 0) {
        fprintf(stderr, "Failed to fread\n");
        exit(1);
    }

    data[bufsize] = '\0';
    fclose(fp);

    return data;
}

void initialize_sdl() {
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

/* WINDOW */

struct Window {
    SDL_Window* window;
    SDL_GLContext context;
};

struct Window create_window() {
    struct Window window;

    static const int width = 1920;
    static const int height = 1080;

    int pos = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    window.window = SDL_CreateWindow("", pos, pos, width, height, flags);
    window.context = SDL_GL_CreateContext(window.window);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, width, height);

    return window;
}

void update_display(struct Window window) {
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    SDL_GL_SwapWindow(window.window);
}

void delete_window(struct Window window) {
    SDL_GL_DeleteContext(window.context);
    SDL_DestroyWindow(window.window);
}

/* SHADER PROGRAM */

struct Program {
    GLuint vs;
    GLuint fs;
    GLuint program;
};

GLuint create_shader(GLenum type, char const* source_path) {
    GLuint shader = glCreateShader(type);

    int source_len;
    char* source = read_file(source_path, &source_len);

    glShaderSource(shader, 1, (const GLchar**)&source, &source_len);
    glCompileShader(shader);

    free(source);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        size_t max_size = 10000;
        GLchar* log = (GLchar*)malloc(max_size * sizeof(GLchar));
        GLsizei length;
        glGetShaderInfoLog(shader, max_size, &length, log);

        fprintf(stderr, "shader compilation failed\n%.*s", length, log);
        exit(1);
    }

    return shader;
}

struct Program create_program() {
    struct Program program;

    program.vs = create_shader(GL_VERTEX_SHADER, vertex_shader_path);
    program.fs = create_shader(GL_FRAGMENT_SHADER, fragment_shader_path);
    program.program = glCreateProgram();
    glAttachShader(program.program, program.vs);
    glAttachShader(program.program, program.fs);

    glBindAttribLocation(program.program, 0, "position");
    glLinkProgram(program.program);
    glUseProgram(program.program);

    return program;
}

void delete_program(struct Program program) {
    glDeleteProgram(program.program);
    glDeleteShader(program.vs);
    glDeleteShader(program.fs);
}

/* RENDERING PRIMITIVES */

struct PrimitivesBuffers {
    GLuint vao;
    GLuint vbo;
};

struct PrimitivesBuffers create_primitives_buffers() {
    struct PrimitivesBuffers primitives_buffers;

    // Why do i need this still?
    glGenVertexArrays(1, &primitives_buffers.vao);
    glBindVertexArray(primitives_buffers.vao);

    glGenBuffers(1, &primitives_buffers.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, primitives_buffers.vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

#define XY(x, y) x, y
    const float vertex_buffer_data[] = {XY(-1, -1), XY(1, -1), XY(1, 1), XY(-1, -1), XY(1, 1), XY(-1, 1)};
#undef XY

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    return primitives_buffers;
}

void delete_primitives_buffers(struct PrimitivesBuffers primitives_buffers) {
    glDeleteBuffers(1, &primitives_buffers.vbo);
    glDeleteVertexArrays(1, &primitives_buffers.vao);
}

/* GENERIC GPU BUFFER OPS */

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

/* PCM DATA */

struct PcmData {
    int num_samples;

    float* ring_left;
    float* ring_right;
    int offset;

    GLuint gpu_buffer;
};

struct PcmData create_pcm_data(int num_samples) {
    struct PcmData pcm_data = {.num_samples = num_samples,
                               .ring_left = malloc(num_samples * sizeof(float)),
                               .ring_right = malloc(num_samples * sizeof(float)),
                               .offset = 0};
    for(int i = 0; i < num_samples; i++) {
        pcm_data.ring_left[i] = 0.0f;
        pcm_data.ring_right[i] = 0.0f;
    }

    glGenBuffers(1, &pcm_data.gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pcm_data.gpu_buffer);
    int gpu_buffer_size = sizeof(int) + 2 * num_samples * sizeof(float);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(int), &num_samples);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, pcm_data.gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return pcm_data;
}

void copy_pcm_data_to_gpu(struct PcmData pcm_data) {
    GLuint buffer = pcm_data.gpu_buffer;
    char* left = (char*)pcm_data.ring_left;
    char* right = (char*)pcm_data.ring_right;
    int pcm_size = pcm_data.num_samples * sizeof(float);
    int pcm_wrap_offset = pcm_data.offset * sizeof(float);
    copy_ringbuffer_to_gpu(buffer, left, sizeof(int), pcm_size, pcm_wrap_offset);
    copy_ringbuffer_to_gpu(buffer, right, sizeof(int) + pcm_size, pcm_size, pcm_wrap_offset);
}

void delete_pcm_data(struct PcmData pcm_data) {
    glDeleteBuffers(1, &pcm_data.gpu_buffer);
    free(pcm_data.ring_left);
    free(pcm_data.ring_right);
}

/* PCM INPUT STREAM */

struct ThreadData {
    bool close_requested;
    struct PcmData* pcm_data;
};

void* input_thread_function(void* data_raw) {
    struct ThreadData* data = (struct ThreadData*)data_raw;
    struct PcmData* pcm = data->pcm_data;

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

struct InputStream {
    pthread_t thread;
    struct ThreadData* thread_data;
};

struct InputStream create_input_stream(struct PcmData* pcm_data) {
    struct InputStream input_stream;
    input_stream.thread_data = malloc(sizeof(struct ThreadData));

    input_stream.thread_data->close_requested = false;
    input_stream.thread_data->pcm_data = pcm_data;

    int failure = pthread_create(&input_stream.thread, NULL, input_thread_function, (void*)input_stream.thread_data);
    if(failure) {
        fprintf(stderr, "Failed to pthread_create\n");
        exit(1);
    }

    return input_stream;
}

void delete_input_stream(struct InputStream input_stream) {
    input_stream.thread_data->close_requested = true;
    pthread_join(input_stream.thread, NULL);
    free(input_stream.thread_data);
}

/* DFT DATA */

struct DftData {
    int size;
    float* in;
    float* out;
    fftwf_plan plan;

    float* smoothed;

    GLuint gpu_buffer;
};

struct DftData create_dft_data(int dft_size) {
    assert(dft_size % 2 == 0);
    struct DftData dft_data = {.size = dft_size,
                               .in = fftwf_malloc(dft_size * sizeof(float)),
                               .out = fftwf_malloc(dft_size * sizeof(fftwf_complex)),
                               .smoothed = malloc(dft_size * sizeof(fftwf_complex))};
    dft_data.plan = fftwf_plan_r2r_1d(dft_size, dft_data.in, dft_data.out, FFTW_R2HC, 0);

    for(int i = 0; i < dft_size; i++) {
        dft_data.smoothed[i] = 0.0f;
    }

    glGenBuffers(1, &dft_data.gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, dft_data.gpu_buffer);
    int gpu_buffer_size = sizeof(int) + 2 * dft_size * sizeof(float);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(int), &dft_size);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, dft_data.gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return dft_data;
}

#define PI 3.1415

void compute_and_copy_dft_data_to_gpu(struct PcmData pcm_data, struct DftData dft_data) {
    int floats_to_start = MIN(pcm_data.offset, dft_data.size);
    int floats_from_end = dft_data.size - floats_to_start;
    memcpy(dft_data.in, pcm_data.ring_left + pcm_data.num_samples - floats_from_end, floats_from_end * sizeof(float));
    float* second_half = pcm_data.ring_left + pcm_data.offset - floats_to_start;
    memcpy(dft_data.in + floats_from_end, second_half, floats_to_start * sizeof(float));

    // Multiply witn Hamming window.
    for(int i = 0; i < dft_data.size; i++) {
        dft_data.in[i] *= 0.54 - (0.46 * cos(2 * PI * (i / (dft_data.size - 1.0))));
    }
    fftwf_execute(dft_data.plan);

    for(int i = 0; i < dft_data.size; i++) {
        dft_data.smoothed[i] = MAX(0.95 * dft_data.smoothed[i], dft_data.out[i]);
    }

    int buffer_size = dft_data.size * sizeof(float);
    copy_buffer_to_gpu(dft_data.gpu_buffer, (char*)dft_data.out, sizeof(int), buffer_size);
    copy_buffer_to_gpu(dft_data.gpu_buffer, (char*)dft_data.smoothed, sizeof(int) + buffer_size, buffer_size);
}

void delete_dft_data(struct DftData dft_data) {
    glDeleteBuffers(1, &dft_data.gpu_buffer);

    free(dft_data.smoothed);
    fftwf_destroy_plan(dft_data.plan);
    fftwf_free(dft_data.in);
    fftwf_free(dft_data.out);
}

/* EVENT HANDLING AND OTHER */

struct UserContext {
    bool quit_requested;
    int offset;
};

void handle_events(struct UserContext* user_context) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            user_context->quit_requested = true;
            break;
        case SDL_KEYUP:
            if(event.key.keysym.sym == SDLK_ESCAPE) {
                user_context->quit_requested = true;
            }
            break;
        case SDL_MOUSEWHEEL:
            user_context->offset = CLAMP(user_context->offset + event.wheel.y, 0, 900);
            if(user_context->offset == 0) {
                break;
            }

            // Offset gives the distance between the x pointer and the y pointer.
            // If the distance is 90 degrees (a quarter of a period) od a certain frequency,
            // we get a circle, let's call that the resonating frequency.
            // Given an initial signal sample rate, we can compute the HZ value.
            int const signal_sample_rate = 44100;

            // The offset equals a quarter of a period, meaning that a period is 4 times offset.
            int const period = 4 * user_context->offset;
            int const resonating_frequency = signal_sample_rate / period;

            printf("Offset: %d Hz\n", resonating_frequency);

            break;
        }
    }
}

int main(int argc, char* argv[]) {
    initialize_sdl();
    struct Window window = create_window();
    struct Program program = create_program();
    struct PrimitivesBuffers primitives_buffers = create_primitives_buffers();

    struct PcmData pcm_data = create_pcm_data(44100);
    struct InputStream input_stream = create_input_stream(&pcm_data);
    struct DftData dft_data = create_dft_data(2048);

    struct UserContext user_context;
    user_context.quit_requested = false;
    user_context.offset = 0;

    while(!user_context.quit_requested) {
        copy_pcm_data_to_gpu(pcm_data);
        compute_and_copy_dft_data_to_gpu(pcm_data, dft_data);
        update_display(window);
        handle_events(&user_context);
    }

    delete_dft_data(dft_data);
    delete_input_stream(input_stream);
    delete_pcm_data(pcm_data);
    delete_primitives_buffers(primitives_buffers);
    delete_program(program);
    delete_window(window);
    SDL_Quit();

    return 0;
}
