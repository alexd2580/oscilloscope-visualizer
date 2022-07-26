#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "buffers.h"
#include "defines.h"
#include "dft.h"
#include "pcm.h"
#include "program.h"
#include "random.h"
#include "sdl.h"
#include "timer.h"
#include "window.h"

struct UserInput_ {
    bool quit_requested;
    int offset;
};
typedef struct UserInput_* UserInput;

UserInput create_user_input(void) {
    UserInput user_input = (UserInput)malloc(sizeof(struct UserInput_));
    user_input->quit_requested = false;
    user_input->offset = 0;
    return user_input;
}

void delete_user_input(UserInput user_input) { free(user_input); }

void handle_events(UserInput user_input, Random random) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            user_input->quit_requested = true;
            break;

        case SDL_KEYUP:
            user_input->quit_requested = event.key.keysym.sym == SDLK_ESCAPE;
            break;

        case SDL_WINDOWEVENT:
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                struct WindowSize new_window_size = {
                    .w = event.window.data1,
                    .h = event.window.data2,
                };
                // TODO: Update window size in window.
                update_random_window_size(random, new_window_size);
            }
            break;

        default:
            break;
        }
    }
}

void create_textures(GLuint textures[2], struct WindowSize window_size) {
    glGenTextures(2, textures);

    for(int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, window_size.w, window_size.h, 0, GL_RGBA, GL_FLOAT, NULL);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void swap_and_bind_textures(GLuint textures[2]) {
    GLuint tmp = textures[0];
    textures[0] = textures[1];
    textures[1] = tmp;

    glBindImageTexture(0, textures[0], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(1, textures[1], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    create_sdl();

    struct WindowSize window_size = {.w = 800, .h = 800};
    /* struct WindowSize window_size = {.w = 1920, .h = 1080}; */

    Window window = create_window(window_size);

    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);

    GLuint textures[2];
    create_textures(textures, window_size);

    Program program = create_program("assets/shaders/basic.comp");
    Timer timer = create_timer(1);
    Random random = create_random(window_size, 2);
    Pcm pcm = create_pcm(8 * 44100, 3);
    PcmStream pcm_stream = create_pcm_stream(pcm);
    DftData dft_data = create_dft_data(2048, 4);
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
        copy_timer_to_gpu(timer);

        // Prepare for next frame.
        swap_and_bind_textures(textures);

        // Render the next frame to textures[0].
        GLint w = (GLint)window_size.w;
        GLint h = (GLint)window_size.h;
        run_program(program, (GLuint)w, (GLuint)h);

        // Attach the rendered texture to a framebuffer.
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

        // Blit the framebuffer to the output framebuffer and flip window.
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        // Clearing technically not necessary because we recompute (blit) the entire frame each time.
        // glClear(GL_COLOR_BUFFER_BIT);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        update_display(window);

        time_t t5 = clock();
        render_cycles += t5 - t4;

        handle_events(user_input, random);
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
    delete_dft_data(dft_data);
    delete_pcm_stream(pcm_stream);
    delete_pcm(pcm);
    delete_random(random);
    delete_timer(timer);
    delete_program(program);
    delete_window(window);
    delete_sdl();

    return 0;
}
