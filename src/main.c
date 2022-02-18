#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "buffers.h"
#include "defines.h"
#include "dft.h"
#include "pcm.h"
#include "program.h"
#include "random.h"
#include "render_quad.h"
#include "scene.h"
#include "sdl.h"
#include "timer.h"
#include "view.h"
#include "window.h"

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

void handle_events(float dt, Window window, Scene scene, UserInput user_input, View view, Random random) {
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        SDL_Keycode key = event.key.keysym.sym;
        switch(event.type) {
        case SDL_QUIT:
            user_input->quit_requested = true;
            break;
        case SDL_KEYDOWN:
            *mut_num_steps(view) = MAX(1, *mut_num_steps(view) + (key == SDLK_KP_PLUS) - (key == SDLK_KP_MINUS));

            if(event.key.repeat) {
                break;
            }

            user_input->v_fb += key == SDLK_w ? 1.f : 0.f;
            user_input->v_fb += key == SDLK_s ? -1.f : 0.f;
            user_input->v_rl += key == SDLK_d ? 1.f : 0.f;
            user_input->v_rl += key == SDLK_a ? -1.f : 0.f;
            user_input->v_ud += key == SDLK_SPACE ? 1.f : 0.f;
            user_input->v_ud += key == SDLK_LSHIFT ? -1.f : 0.f;

            break;
        case SDL_KEYUP:
            user_input->quit_requested = key == SDLK_ESCAPE;
            if(key == SDLK_l) {
                trap_mouse(window, false);
            }

            user_input->v_fb -= key == SDLK_w ? 1.f : 0.f;
            user_input->v_fb -= key == SDLK_s ? -1.f : 0.f;
            user_input->v_rl -= key == SDLK_d ? 1.f : 0.f;
            user_input->v_rl -= key == SDLK_a ? -1.f : 0.f;
            user_input->v_ud -= key == SDLK_SPACE ? 1.f : 0.f;
            user_input->v_ud -= key == SDLK_LSHIFT ? -1.f : 0.f;

            break;

        case SDL_MOUSEMOTION:
            if(is_mouse_trapped(window)) {
                float d_pitch = -(float)event.motion.yrel / 2000.f;
                float d_yaw = -(float)event.motion.xrel / 2000.f;
                rotate_camera(view, d_pitch, 0.f, d_yaw);
            }
            break;

        case SDL_MOUSEWHEEL:
            if(is_mouse_trapped(window)) {
                const float ONE_DEG_IN_RAD = 2.f * PI / 360.f;
                float d_fovy = ONE_DEG_IN_RAD * (float)event.wheel.y / 5.f;
                *mut_fovy(view) += d_fovy;
            }
            break;
        case SDL_WINDOWEVENT:
            if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                struct WindowSize new_window_size = {
                    .w = event.window.data1,
                    .h = event.window.data2,
                };
                update_view_window_size(view, new_window_size);
                update_random_window_size(random, new_window_size);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            trap_mouse(window, true);
            break;

        default:
            break;
        }
    }

    // Movement speed.
    float u_per_s = 50.0;
    Vec3 position = get_position(view);
    float closest_distance = distance_to_scene(scene, position) * 0.25f;

    float u_per_frame = fminf(closest_distance, u_per_s * dt);
    printf("%f\n", u_per_frame);

    move_camera(view, u_per_frame * user_input->v_fb, u_per_frame * user_input->v_rl, u_per_frame * user_input->v_ud);
    copy_view_to_gpu(view);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    create_sdl();
    struct WindowSize window_size = {.w = 1920, .h = 1080};
    Window window = create_window(window_size);
    Program program = create_program("assets/shaders/shader.vert", "assets/shaders/shader.frag");
    RenderQuad render_quad = create_render_quad();
    View view = create_view(window_size, 0);
    Timer timer = create_timer(1);
    Random random = create_random(window_size, 2);
    Pcm pcm = create_pcm(8 * 44100, 3);
    PcmStream pcm_stream = create_pcm_stream(pcm);
    DftData dft_data = create_dft_data(2048, 4);
    Scene scene = create_scene(5);
    UserInput user_input = create_user_input();

    int plane = add_primitive(scene, (struct Primitive){.type = PlaneType, .f1 = -10.f});

    int sphere = add_primitive(scene, (struct Primitive){.type = SphereType, .f1 = 5.f});
    int cylinder3 = add_primitive(scene, (struct Primitive){.type = CylinderType, .f1 = 3.f});
    int cutout = add_primitive(scene, (struct Primitive){.type = ComplemenType, .i1 = sphere, .i2 = cylinder3});

    int cylinder1 = add_primitive(scene, (struct Primitive){.type = CylinderType, .f1 = 1.f});
    int shashlik = add_primitive(scene, (struct Primitive){.type = UnionType, .i1 = cutout, .i2 = cylinder1});

    int union_ = add_primitive(scene, (struct Primitive){.type = UnionType, .i1 = plane, .i2 = shashlik});
    set_root_primitive(scene, union_);

    copy_scene_to_gpu(scene);

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
        update_display(window);
        time_t t5 = clock();
        render_cycles += t5 - t4;

        handle_events(dt, window, scene, user_input, view, random);
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
    delete_scene(scene);
    delete_dft_data(dft_data);
    delete_pcm_stream(pcm_stream);
    delete_pcm(pcm);
    delete_random(random);
    delete_timer(timer);
    delete_view(view);
    delete_render_quad(render_quad);
    delete_program(program);
    delete_window(window);
    delete_sdl();

    return 0;
}
