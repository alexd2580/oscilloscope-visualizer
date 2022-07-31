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
#include "peak_analysis.h"
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

    /* struct WindowSize window_size = {.w = 800, .h = 800}; */
    struct WindowSize window_size = {.w = 2560, .h = 1440};

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
    DftData dft_data = create_dft_data(4096, 4);
    PeakAnalysis peak_analysis = create_peak_analysis(4096, 5);
    UserInput user_input = create_user_input();

    GLint w = (GLint)window_size.w;
    GLint h = (GLint)window_size.h;

    while(!user_input->quit_requested) {
        // Recompile shader?
        if(program_source_modified(program)) {
            reinstall_program_if_valid(program);
        }

        // Copy data.
        copy_timer_to_gpu(timer);
        copy_pcm_to_gpu(pcm);
        compute_and_copy_dft_data_to_gpu(pcm, dft_data);
        compute_and_copy_peak_analysis_to_gpu(dft_data, peak_analysis);

        // Render and display.
        // Prepare for next frame.
        swap_and_bind_textures(textures);
        // Render the next frame to textures[0].
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

        // Events.
        handle_events(user_input, random);
    }

    delete_user_input(user_input);
    delete_peak_analysis(peak_analysis);
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
