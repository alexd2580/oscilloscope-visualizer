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

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h> // TODO
#include <unistd.h>

#include <fftw3.h>

#include <sys/stat.h>
#include <sys/types.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_opengl.h>

#include "buffers.h"
#include "pcm.h"
#include "program.h"

#define PI 3.1415

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, l, u) MAX((l), MIN((x), (u)))

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

struct Window_ {
    SDL_Window* window;
    SDL_GLContext context;
};
typedef struct Window_* Window;

Window create_window() {
    Window window = (Window)malloc(sizeof(struct Window_));

    static const int width = 1920;
    static const int height = 1080;

    int pos = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    window->window = SDL_CreateWindow("", pos, pos, width, height, flags);
    window->context = SDL_GL_CreateContext(window->window);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);

    SDL_SetRelativeMouseMode(SDL_ENABLE);
    /* SDL_SetWindowGrab(window->window, SDL_TRUE); */
    /* SDL_ShowCursor(SDL_DISABLE); */

    return window;
}

void update_display(Window window) {
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    SDL_GL_SwapWindow(window->window);
}

void delete_window(Window window) {
    SDL_GL_DeleteContext(window->context);
    SDL_DestroyWindow(window->window);
    free(window);
}

/* VIEW SETTINGS */

struct Vec3_ {
    float x;
    float y;
    float z;
    float _pad;
};
typedef struct Vec3_ Vec3;

Vec3 vec3(float x, float y, float z) {
    return (Vec3){.x = x, .y = y, .z = z, ._pad = 0};
}

Vec3 scale(Vec3 vec, float factor) {
    return vec3(vec.x * factor, vec.y * factor, vec.z * factor);
}

Vec3 add(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 add4(Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    return vec3(a.x + b.x + c.x + d.x, a.y + b.y + c.y + d.y, a.z + b.z + c.z + d.z);
}

struct View_ {
    struct ViewData {
        int width;
        int height;

        // Need to match vec4 alignment.
        float _padding[2];

        float fov;
        float pitch;
        float roll;
        float yaw;


        Vec3 camera_origin;

        // Technically one of the following is redundand, but for ease of use on CPU side...
        Vec3 camera_right;
        Vec3 camera_ahead;
        Vec3 camera_up;
    } data;

    GLuint gpu_buffer;
};
typedef struct View_* View;

void check_gl_error(char const* msg) {
    GLenum err;
    while((err = glGetError()) != GL_NO_ERROR) {
        fprintf(stdout, "%s: %d\n", msg, err);
    }
}

void copy_view_to_gpu(View view) {
    float sin_yaw = sin(view->data.yaw), cos_yaw = cos(view->data.yaw);
    float sin_pitch = sin(view->data.pitch), cos_pitch = cos(view->data.pitch);

    view->data.camera_right = vec3(cos_yaw, 0, -sin_yaw);
    Vec3 back = vec3(sin_yaw, 0, cos_yaw);
    Vec3 base_y = vec3(0, 1, 0);
    view->data.camera_up = add(scale(base_y, cos_pitch), scale(back, sin_pitch));
    view->data.camera_ahead = add(scale(base_y, sin_pitch), scale(back, -cos_pitch));

    glBindBuffer(GL_UNIFORM_BUFFER, view->gpu_buffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(struct ViewData), &view->data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glViewport(0, 0, view->data.width, view->data.height);
}

View create_view(Window window) {
    View view = (View)malloc(sizeof(struct View_));

    SDL_GetWindowSize(window->window, &view->data.width, &view->data.height);

    // 45 degrees.
    view->data.fov = 2.0 * PI / 8.0;

    view->data.camera_origin = vec3(-100, 1, 0);

    // RHR with middle finger pointing from monitor to user.
    // These vectors will only be set before transfer to GPU.
    /* view->data.camera_right = vec3(1, 0, 0); */
    /* view->data.camera_up = vec3(0, 1, 0); */
    /* view->data.camera_ahead = vec3(0, 0, -1); */

    // For reference see:
    // https://blog.techlab-xe.net/wp-content/uploads/2013/12/fig1_uniform_buffer.png

    glGenBuffers(1, &view->gpu_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, view->gpu_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(struct ViewData), NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, view->gpu_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    copy_view_to_gpu(view);

    return view;
}

void move_camera(View view, float forward, float right, float up) {
    Vec3 offset_forward = scale(view->data.camera_ahead, forward);
    Vec3 offset_right = scale(view->data.camera_right, right);
    Vec3 offset_up = scale(view->data.camera_up, up);
    view->data.camera_origin = add4(view->data.camera_origin, offset_up, offset_forward, offset_right);
}

void rotate_camera(View view, float pitch, float roll, float yaw) {
    (void)roll;

    view->data.pitch = CLAMP(view->data.pitch + pitch, -0.5 * PI, 0.5 * PI);
    /* view->roll; */
    view->data.yaw += yaw;
}

void delete_view(View view) { free(view); }

/* RENDERING PRIMITIVES */

struct PrimitivesBuffers_ {
    GLuint vao;
    GLuint vbo;
};
typedef struct PrimitivesBuffers_* PrimitivesBuffers;

PrimitivesBuffers create_primitives_buffers() {
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

void delete_primitives_buffers(PrimitivesBuffers primitives_buffers) {
    glDeleteBuffers(1, &primitives_buffers->vbo);
    glDeleteVertexArrays(1, &primitives_buffers->vao);
    free(primitives_buffers);
}

/* DFT DATA */

struct DftData_ {
    int size;
    float* in;
    float* out;
    fftwf_plan plan;

    float* smoothed;

    GLuint gpu_buffer;
};
typedef struct DftData_* DftData;

DftData create_dft_data(int dft_size) {
    assert(dft_size % 2 == 0);
    DftData dft_data = (DftData)malloc(sizeof(struct DftData_));

    dft_data->size = dft_size;
    dft_data->in = fftwf_malloc(dft_size * sizeof(float));
    dft_data->out = fftwf_malloc(dft_size * sizeof(fftwf_complex));
    dft_data->smoothed = malloc(dft_size * sizeof(fftwf_complex));
    dft_data->plan = fftwf_plan_r2r_1d(dft_size, dft_data->in, dft_data->out, FFTW_R2HC, 0);

    for(int i = 0; i < dft_size; i++) {
        dft_data->smoothed[i] = 0.0f;
    }

    // The following doesn't even have to be a comment!
    // For reference see:
    // https://www.uni-weimar.de/fileadmin/user/fak/medien/professuren/Computer_Graphics/CG_WS_18_19/CG/06_ShaderBuffers.pdf

    glGenBuffers(1, &dft_data->gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, dft_data->gpu_buffer);
    int gpu_buffer_size = 2 * sizeof(int) + 2 * dft_size * sizeof(float);
    glBufferData(GL_SHADER_STORAGE_BUFFER, gpu_buffer_size, NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(int), &dft_size);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, dft_data->gpu_buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return dft_data;
}

void compute_and_copy_dft_data_to_gpu(Pcm pcm, DftData dft_data) {
    copy_pcm_mono_to_buffer(dft_data->in, pcm, dft_data->size);

    // Multiply witn Hamming window.
    for(int i = 0; i < dft_data->size; i++) {
        dft_data->in[i] *= 0.54 - (0.46 * cos(2 * PI * (i / (dft_data->size - 1.0))));
    }
    fftwf_execute(dft_data->plan);

    float max_value = 0.0;
    int max_index = 0;

    for(int i = 0; i < dft_data->size / 2 + 1; i++) {
        float re = dft_data->out[i];
        float im = i > 0 && i < dft_data->size / 2 - 1 ? dft_data->out[i] : 0.0f;
        float amp_2 = re * re + im * im;
        if(amp_2 > max_value) {
            max_value = amp_2;
            max_index = i;
        }
    }

    int dominant_freq_period = dft_data->size / (max_index + 1);

    for(int i = 0; i < dft_data->size; i++) {
        dft_data->smoothed[i] = MAX(0.95 * dft_data->smoothed[i], dft_data->out[i]);
    }

    copy_buffer_to_gpu(dft_data->gpu_buffer, (char*)&dominant_freq_period, sizeof(int), sizeof(int));
    int buffer_offset = 2 * sizeof(int);
    int buffer_size = dft_data->size * sizeof(float);
    copy_buffer_to_gpu(dft_data->gpu_buffer, (char*)dft_data->out, buffer_offset, buffer_size);
    copy_buffer_to_gpu(dft_data->gpu_buffer, (char*)dft_data->smoothed, buffer_offset + buffer_size, buffer_size);
}

void delete_dft_data(DftData dft_data) {
    glDeleteBuffers(1, &dft_data->gpu_buffer);

    free(dft_data->smoothed);
    fftwf_destroy_plan(dft_data->plan);
    fftwf_free(dft_data->in);
    fftwf_free(dft_data->out);
    free(dft_data);
}

/* EVENT HANDLING AND OTHER */

struct UserInput_ {
    bool quit_requested;
    int offset;

    float v_fb;
    float v_rl;
    float v_ud;
};
typedef struct UserInput_* UserInput;

UserInput create_user_input(void) {
    UserInput user_input = (UserInput)malloc(sizeof(struct UserInput_));
    user_input->quit_requested = false;
    user_input->offset = 0;

    user_input->v_fb = 0.0;
    user_input->v_rl = 0.0;
    user_input->v_ud = 0.0;

    return user_input;
}

void delete_user_input(UserInput user_input) { free(user_input); }

void handle_events(UserInput user_input, View view) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            user_input->quit_requested = true;
            break;
        case SDL_KEYDOWN:
            if (event.key.repeat) {
                break;
            }

            user_input->v_fb += event.key.keysym.sym == SDLK_w ? 1.0 : 0.0;
            user_input->v_fb += event.key.keysym.sym == SDLK_s ? -1.0 : 0.0;
            user_input->v_rl += event.key.keysym.sym == SDLK_d ? 1.0 : 0.0;
            user_input->v_rl += event.key.keysym.sym == SDLK_a ? -1.0 : 0.0;
            user_input->v_ud += event.key.keysym.sym == SDLK_SPACE ? 1.0 : 0.0;
            user_input->v_ud += event.key.keysym.sym == SDLK_LSHIFT ? -1.0 : 0.0;

            break;
        case SDL_KEYUP:
            if(event.key.keysym.sym == SDLK_ESCAPE) {
                user_input->quit_requested = true;
            }

            user_input->v_fb -= event.key.keysym.sym == SDLK_w ? 1.0 : 0.0;
            user_input->v_fb -= event.key.keysym.sym == SDLK_s ? -1.0 : 0.0;
            user_input->v_rl -= event.key.keysym.sym == SDLK_d ? 1.0 : 0.0;
            user_input->v_rl -= event.key.keysym.sym == SDLK_a ? -1.0 : 0.0;
            user_input->v_ud -= event.key.keysym.sym == SDLK_SPACE ? 1.0 : 0.0;
            user_input->v_ud -= event.key.keysym.sym == SDLK_LSHIFT ? -1.0 : 0.0;

            break;

        case SDL_MOUSEMOTION:
            rotate_camera(view, -event.motion.yrel / 2000.0, 0.0, -event.motion.xrel / 2000.0);
            break;
        /* case SDL_MOUSEWHEEL: */
        /*     user_context->offset = CLAMP(user_context->offset + event.wheel.y, 0, 900); */
        /*     if(user_context->offset == 0) { */
        /*         break; */
        /*     } */
        /*  */
        /*     // Offset gives the distance between the x pointer and the y pointer. */
        /*     // If the distance is 90 degrees (a quarter of a period) od a certain frequency, */
        /*     // we get a circle, let's call that the resonating frequency. */
        /*     // Given an initial signal sample rate, we can compute the HZ value. */
        /*     int const signal_sample_rate = 44100; */
        /*  */
        /*     // The offset equals a quarter of a period, meaning that a period is 4 times offset. */
        /*     int const period = 4 * user_context->offset; */
        /*     int const resonating_frequency = signal_sample_rate / period; */
        /*  */
        /*     printf("Offset: %d Hz\n", resonating_frequency); */
        /*  */
        /*     break; */
        case SDL_WINDOWEVENT:
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                view->data.width = event.window.data1;
                view->data.height = event.window.data2;
            }
            break;
        }
    }

    move_camera(view, user_input->v_fb, user_input->v_rl, user_input->v_ud);
    copy_view_to_gpu(view);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    initialize_sdl();

    Window window = create_window();
    Program program = create_program("shader.vert", "shader.frag");
    PrimitivesBuffers primitives_buffers = create_primitives_buffers();
    View view = create_view(window);
    Pcm pcm = create_pcm(8 * 44100);
    PcmStream pcm_stream = create_pcm_stream(pcm);
    DftData dft_data = create_dft_data(2048);
    UserInput user_input = create_user_input();

    long program_check_cycles = 0;
    long pcm_copy_cycles = 0;
    long dft_process_cycles = 0;
    long render_cycles = 0;
    long event_handling_cycles = 0;
    long cycles = 0;

    while(!user_input->quit_requested) {
        time_t t1 = clock();
        if(program_source_modified(program)) {
            reinstall_program_if_valid(program);
        }
        time_t t2 = clock();
        program_check_cycles += t2 - t1;

        copy_pcm_to_gpu(pcm);
        time_t t3 = clock();
        pcm_copy_cycles += t3 - t2;

        compute_and_copy_dft_data_to_gpu(pcm, dft_data);
        time_t t4 = clock();
        dft_process_cycles += t4 - t3;

        update_display(window);
        time_t t5 = clock();
        render_cycles += t5 - t4;

        handle_events(user_input, view);
        time_t t6 = clock();
        event_handling_cycles += t6 - t5;

        cycles++;

        if(cycles >= 100 && false) {
            long cycle_clocks = cycles * CLOCKS_PER_SEC;
            printf("Watchers:\t%.0fms\nPCM:\t%.0fms\nDFT:\t%.0fms\nRender:\t%.0fms\nEvents:\t%.0fms\n",
                   1000.0 * program_check_cycles / cycle_clocks, 1000.0 * pcm_copy_cycles / cycle_clocks,
                   1000.0 * dft_process_cycles / cycle_clocks, 1000.0 * render_cycles / cycle_clocks,
                   1000.0 * event_handling_cycles / cycle_clocks);

            program_check_cycles = 0;
            pcm_copy_cycles = 0;
            dft_process_cycles = 0;
            render_cycles = 0;
            event_handling_cycles = 0;
            cycles = 0;
        }
    }

    delete_user_input(user_input);
    delete_dft_data(dft_data);
    delete_pcm_stream(pcm_stream);
    delete_pcm(pcm);
    delete_view(view);
    delete_primitives_buffers(primitives_buffers);
    delete_program(program);
    delete_window(window);
    SDL_Quit();

    return 0;
}

// Inotify file watcher.
/* https://www.thegeekstuff.com/2010/04/inotify-c-program-example/ */
