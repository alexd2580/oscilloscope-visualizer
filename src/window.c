#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "window.h"

struct Window_ {
    SDL_Window* window;
    SDL_GLContext context;
};

Window create_window(struct WindowSize window_size) {
    Window window = (Window)malloc(sizeof(struct Window_));

    int pos = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    window->window = SDL_CreateWindow("", pos, pos, window_size.w, window_size.h, flags);
    window->context = SDL_GL_CreateContext(window->window);

    glDisable(GL_DEPTH_TEST);
    // glClearColor(0.0, 0.0, 0.0, 0.0);

    return window;
}

void update_display(Window window) {
    SDL_GL_SwapWindow(window->window);
}

struct WindowSize get_window_size(Window window) {
    struct WindowSize window_size;
    SDL_GetWindowSize(window->window, &window_size.w, &window_size.h);
    return window_size;
}

void delete_window(Window window) {
    SDL_GL_DeleteContext(window->context);
    SDL_DestroyWindow(window->window);
    free(window);
}
