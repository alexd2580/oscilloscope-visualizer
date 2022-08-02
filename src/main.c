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
#include "analysis.h"
#include "program.h"
#include "random.h"
#include "sdl.h"
#include "textures.h"
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

void handle_events(UserInput user_input, Random random, Textures textures, struct Size* size) {
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
                *size = (struct Size){.w = event.window.data1, .h = event.window.data2};

                update_random_window_size(random, *size);
                update_textures_window_size(textures, *size);
            }
            break;

        default:
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    create_sdl();

    struct Size size = {.w = 800, .h = 800};
    Window window = create_window(size);
    Textures textures = create_textures(size);
    Program program = create_program("assets/shaders/basic.comp");
    Timer timer = create_timer(1);
    Random random = create_random(size, 2);
    Pcm pcm = create_pcm(8 * 44100, 3);
    PcmStream pcm_stream = create_pcm_stream(pcm);
    DftData dft_data = create_dft_data(4096, 4);
    Analysis analysis = create_analysis(pcm, dft_data, 5);
    UserInput user_input = create_user_input();

    while(!user_input->quit_requested) {
        // Recompile shader?
        if(program_source_modified(program)) {
            reinstall_program_if_valid(program);
        }

        // Copy data.
        copy_timer_to_gpu(timer);
        copy_pcm_to_gpu(pcm);
        compute_and_copy_dft_data_to_gpu(pcm, dft_data);
        compute_and_copy_analysis_to_gpu(dft_data, analysis);

        // Render and display.
        // Prepare for next frame.
        swap_and_bind_textures(textures);
        // Render the next frame to the back texture.
        run_program(program, (GLuint)size.w, (GLuint)size.h);

        display_texture(window, get_back_texture(textures), size);

        // Events.
        handle_events(user_input, random, textures, &size);
    }

    delete_user_input(user_input);
    delete_analysis(analysis);
    delete_dft_data(dft_data);
    delete_pcm_stream(pcm_stream);
    delete_pcm(pcm);
    delete_random(random);
    delete_timer(timer);
    delete_program(program);
    delete_textures(textures);
    delete_window(window);
    delete_sdl();

    return 0;
}
