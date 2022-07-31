#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "size.h"
#include "window.h"

struct Window_ {
    SDL_Window* window;
    SDL_GLContext context;
    GLuint framebuffer;
};

Window create_window(struct Size size) {
    Window window = (Window)malloc(sizeof(struct Window_));

    int pos = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    window->window = SDL_CreateWindow("", pos, pos, size.w, size.h, flags);
    window->context = SDL_GL_CreateContext(window->window);

    glDisable(GL_DEPTH_TEST);
    // glClearColor(0.0, 0.0, 0.0, 0.0);

    glGenFramebuffers(1, &window->framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, window->framebuffer);

    return window;
}

void display_texture(Window window, GLuint texture, struct Size size) {
    // Textures cannot be bound to the default frame buffer, therefore this
    // complex wrangler. For reference see:
    // https://stackoverflow.com/questions/21469784/is-it-possible-to-attach-textures-as-render-target-to-the-default-framebuffer

    // `texture` is the texture i want to display now (back).
    // Attach the texture to the tmp framebuffer.
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    // Clearing technically not necessary because we recompute (blit) the entire frame each time.
    // glClear(GL_COLOR_BUFFER_BIT);
    // Blit (copy) the tmp framebuffer (current READ) to the output framebuffer (0, default DRAW).
    int w = size.w;
    int h = size.h;
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // Actually swap real back and front buffers.
    SDL_GL_SwapWindow(window->window);
}

struct Size get_window_size(Window window) {
    struct Size size;
    SDL_GetWindowSize(window->window, &size.w, &size.h);
    return size;
}

void delete_window(Window window) {
    glDeleteFramebuffers(1, &window->framebuffer);
    SDL_GL_DeleteContext(window->context);
    SDL_DestroyWindow(window->window);
    free(window);
}
