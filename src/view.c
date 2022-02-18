#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "buffers.h"
#include "defines.h"
#include "vec.h"
#include "view.h"

struct View_ {
    struct ViewData {
        int width;
        int height;

        float fovy;
        float pitch;
        float roll;
        float yaw;

        int num_steps;

        // Need to match vec4 alignment.
        float _padding;

        Vec3 camera_origin;

        // Technically one of the following is redundand, but for ease of use on CPU side...
        Vec3 camera_right;
        Vec3 camera_ahead;
        Vec3 camera_up;
    } data;

    Buffer buffer;
};

void copy_view_to_gpu(View view) {
    float yaw = view->data.yaw, pitch = view->data.pitch;
    float sin_yaw = sinf(yaw), sin_pitch = sinf(pitch);
    float cos_yaw = cosf(yaw), cos_pitch = cosf(pitch);

    view->data.camera_right = vec3(cos_yaw, 0, -sin_yaw);
    Vec3 back = vec3(sin_yaw, 0, cos_yaw);
    Vec3 base_y = vec3(0, 1, 0);
    view->data.camera_up = add(scale(base_y, cos_pitch), scale(back, sin_pitch));
    view->data.camera_ahead = add(scale(base_y, sin_pitch), scale(back, -cos_pitch));

    copy_buffer_to_gpu(view->buffer, &view->data, 0, sizeof(struct ViewData));

    glViewport(0, 0, view->data.width, view->data.height);
}

void update_view_window_size(View view, struct WindowSize window_size) {
    view->data.width = window_size.w;
    view->data.height = window_size.h;
}

__attribute__((const)) Vec3 get_position(View view) {
    return view->data.camera_origin;
}

__attribute__((const)) int* mut_num_steps(View view) {
    return &view->data.num_steps;
}

__attribute__((const)) float* mut_fovy(View view) {
    return &view->data.fovy;
}

View create_view(struct WindowSize window_size, unsigned int index) {
    View view = (View)malloc(sizeof(struct View_));

    update_view_window_size(view, window_size);

    // 45 degrees.
    view->data.fovy = 2.f * PI / 8.f;
    view->data.num_steps = 50;
    view->data.camera_origin = vec3(0, 10, 10);

    // RHR with middle finger pointing from monitor to user.
    // These vectors will only be set before transfer to GPU.
    /* view->data.camera_right = vec3(1, 0, 0); */
    /* view->data.camera_up = vec3(0, 1, 0); */
    /* view->data.camera_ahead = vec3(0, 0, -1); */

    view->buffer = create_uniform_buffer(sizeof(struct ViewData), index);

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

    view->data.pitch = CLAMP(view->data.pitch + pitch, -.5f * PI, .5f * PI);
    /* view->roll; */
    view->data.yaw += yaw;
}

void delete_view(View view) { free(view); }
