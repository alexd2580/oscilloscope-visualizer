#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "buffers.h"
#include "globals.h"
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
    Textures textures = create_textures(size, 3);

    Program basic = create_program("assets/shaders/basic.comp");
    Program basic_present = create_program("assets/shaders/basic_present.comp");

    Timer timer = create_timer(1);
    Random random = create_random(size, 2);
    Pcm pcm = create_pcm(4 * 44100, 3);
    PcmStream pcm_stream = create_pcm_stream(pcm);
    DftData dft_data = create_dft_data(4096, 4);
    Analysis analysis = create_analysis(pcm, dft_data, 5);
    UserInput user_input = create_user_input();

    int cycles = 0;
    float s_per_frame = 0.016f; // 60 FPS?
    time_t last = clock();

    /* float const nanos_per_ms = 1000000.f; */
    /*  */
    /* float avg_data = 1.f; */
    /* float avg_comp_1 = 1.f; */
    /* float avg_comp_2 = 1.f; */
    /* float avg_display = 1.f; */
    /*  */
    /* GLuint queries[5]; */
    /* glGenQueries(5, queries); */

    while(!user_input->quit_requested) {
        reinstall_program_if_modified(basic);
        reinstall_program_if_modified(basic_present);

        /* glQueryCounter(queries[0], GL_TIMESTAMP); */

        // Copy data.
        copy_timer_to_gpu(timer);
        copy_pcm_to_gpu(pcm);
        compute_and_copy_dft_data_to_gpu(pcm, dft_data);
        compute_and_copy_analysis_to_gpu(dft_data, analysis);

        /* glQueryCounter(queries[1], GL_TIMESTAMP); */

        // Render and display.
        // Prepare for next frame.
        swap_and_bind_textures(textures);
        // Render the next frame to the back texture.
        run_program(basic, (GLuint)size.w, (GLuint)size.h);

        /* glQueryCounter(queries[2], GL_TIMESTAMP); */

        run_program(basic_present, (GLuint)size.w, (GLuint)size.h);

        /* glQueryCounter(queries[3], GL_TIMESTAMP); */

        display_texture(window, get_present_texture(textures), size);

        /* glQueryCounter(queries[4], GL_TIMESTAMP); */

        // Events.
        handle_events(user_input, random, textures, &size);

        // Compute FPS and timing stuff.
        /* int avail = 0; */
        /* time_t c1 = clock(); */
        /* while (!avail) { */
        /*     glGetQueryObjectiv(queries[4], GL_QUERY_RESULT_AVAILABLE, &avail); */
        /* } */
        /* time_t c2 = clock(); */
        /*  */
        /* GLuint64 time_a, time_b, time_c, time_d, time_e; */
        /* glGetQueryObjectui64v(queries[0], GL_QUERY_RESULT, &time_a); */
        /* glGetQueryObjectui64v(queries[1], GL_QUERY_RESULT, &time_b); */
        /* glGetQueryObjectui64v(queries[2], GL_QUERY_RESULT, &time_c); */
        /* glGetQueryObjectui64v(queries[3], GL_QUERY_RESULT, &time_d); */
        /* glGetQueryObjectui64v(queries[4], GL_QUERY_RESULT, &time_e); */
        /*  */
        /* avg_data = mix(avg_data, (float)(time_b - time_a) / nanos_per_ms, 0.01f); */
        /* avg_comp_1 = mix(avg_comp_1, (float)(time_c - time_b) / nanos_per_ms, 0.01f); */
        /* avg_comp_2 = mix(avg_comp_2, (float)(time_d - time_c) / nanos_per_ms, 0.01f); */
        /* avg_display = mix(avg_display, (float)(time_e - time_d) / nanos_per_ms, 0.01f); */

        time_t c = clock();
        float delta = (float)(c - last) / (float)CLOCKS_PER_SEC;
        last = c;
        s_per_frame = mix(s_per_frame, delta, 0.01f);

        if (cycles % 100 == 0) {
            /* printf("Query: %.4f\n", (double)(c2 - c1) * 1000.0 / (double)CLOCKS_PER_SEC); */
            printf("FPS: %.4f\n", (double)(1.f / s_per_frame));
            /* printf("Data to gpu: %.4f\n", (double)avg_data); */
            /* printf("Comp 1: %.4f\n", (double)avg_comp_1); */
            /* printf("Comp 2: %.4f\n", (double)avg_comp_2); */
            /* printf("Display: %.4f\n", (double)avg_display); */
        }

        cycles++;
    }

    /* glDeleteQueries(5, queries); */

    delete_user_input(user_input);
    delete_analysis(analysis);
    delete_dft_data(dft_data);
    delete_pcm_stream(pcm_stream);
    delete_pcm(pcm);
    delete_random(random);
    delete_timer(timer);
    delete_program(basic_present);
    delete_program(basic);
    delete_textures(textures);
    delete_window(window);
    delete_sdl();

    return 0;
}
