#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h> // TODO
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_opengl.h>

#include "buffers.h"
#include "defines.h"
#include "dft.h"
#include "pcm.h"
#include "program.h"
#include "view.h"
#include "window.h"
#include "random.h"
#include "timer.h"

void initialize_sdl(void) {
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

/* RENDERING PRIMITIVES */

struct PrimitivesBuffers_ {
    GLuint vao;
    GLuint vbo;
};
typedef struct PrimitivesBuffers_* PrimitivesBuffers;

PrimitivesBuffers create_primitives_buffers(void) {
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

void handle_events(float dt, Window window, UserInput user_input, View view, Random random) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        SDL_Keycode key = event.key.keysym.sym;
        switch(event.type) {
        case SDL_QUIT:
            user_input->quit_requested = true;
            break;
        case SDL_KEYDOWN:
            view->data.num_steps = MAX(1, view->data.num_steps + (key == SDLK_KP_PLUS) - (key == SDLK_KP_MINUS));

            if(event.key.repeat) {
                break;
            }

            user_input->v_fb += key == SDLK_w ? 1.0 : 0.0;
            user_input->v_fb += key == SDLK_s ? -1.0 : 0.0;
            user_input->v_rl += key == SDLK_d ? 1.0 : 0.0;
            user_input->v_rl += key == SDLK_a ? -1.0 : 0.0;
            user_input->v_ud += key == SDLK_SPACE ? 1.0 : 0.0;
            user_input->v_ud += key == SDLK_LSHIFT ? -1.0 : 0.0;

            break;
        case SDL_KEYUP:
            user_input->quit_requested = key == SDLK_ESCAPE;
            if(key == SDLK_l) {
                trap_mouse(window, false);
            }

            user_input->v_fb -= key == SDLK_w ? 1.0 : 0.0;
            user_input->v_fb -= key == SDLK_s ? -1.0 : 0.0;
            user_input->v_rl -= key == SDLK_d ? 1.0 : 0.0;
            user_input->v_rl -= key == SDLK_a ? -1.0 : 0.0;
            user_input->v_ud -= key == SDLK_SPACE ? 1.0 : 0.0;
            user_input->v_ud -= key == SDLK_LSHIFT ? -1.0 : 0.0;

            break;

        case SDL_MOUSEMOTION:
            if(is_mouse_trapped(window)) {
                float d_pitch = -event.motion.yrel / 2000.0;
                float d_yaw = -event.motion.xrel / 2000.0;
                rotate_camera(view, d_pitch, 0.0, d_yaw);
            }
            break;

        case SDL_MOUSEWHEEL:
            if(is_mouse_trapped(window)) {
                const float ONE_DEG_IN_RAD = 2.f * PI / 360.f;
                float d_fovy = ONE_DEG_IN_RAD * event.wheel.y / 5.0;
                view->data.fovy += d_fovy;
            }
            break;
        case SDL_WINDOWEVENT:
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                view->data.width = event.window.data1;
                view->data.height = event.window.data2;
                reinitialize_random(random, view);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            trap_mouse(window, true);
            break;
        }
    }

    // Movement speed.
    float u_per_s = 100.0;
    move_camera(view, dt * u_per_s * user_input->v_fb, dt * u_per_s * user_input->v_rl,
                dt * u_per_s * user_input->v_ud);
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
    Random random = create_random(view);
    UserInput user_input = create_user_input();

    long program_check_cycles = 0;
    long pcm_copy_cycles = 0;
    long dft_process_cycles = 0;
    long render_cycles = 0;
    long event_handling_cycles = 0;
    long cycles = 0;

    time_t time_last_iter = clock();

    while(!user_input->quit_requested) {
        time_t time_this_iter = clock();
        float dt = (float)(time_this_iter - time_last_iter) / CLOCKS_PER_SEC;
        time_last_iter = time_this_iter;

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

        handle_events(dt, window, user_input, view, random);
        time_t t6 = clock();
        event_handling_cycles += t6 - t5;

        cycles++;

        if(cycles >= 100 && false) {
            long cycle_clocks = cycles * CLOCKS_PER_SEC;
            printf("Watchers:\t%ldms\nPCM:\t%ldms\nDFT:\t%ldms\nRender:\t%ldms\nEvents:\t%ldms\n",
                   1000 * program_check_cycles / cycle_clocks, 1000 * pcm_copy_cycles / cycle_clocks,
                   1000 * dft_process_cycles / cycle_clocks, 1000 * render_cycles / cycle_clocks,
                   1000 * event_handling_cycles / cycle_clocks);

            program_check_cycles = 0;
            pcm_copy_cycles = 0;
            dft_process_cycles = 0;
            render_cycles = 0;
            event_handling_cycles = 0;
            cycles = 0;
        }
    }

    delete_user_input(user_input);
    delete_random(random);
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
