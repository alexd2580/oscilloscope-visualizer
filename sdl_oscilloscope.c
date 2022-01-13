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

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

static const char* vertex_shader_path = "shader.vert";
static const char* fragment_shader_path = "shader.frag";

char* read_file(char const* path) {
    FILE* fp = fopen(path, "r");
    if(fp == NULL) {
        fprintf(stderr, "Cannot open file %s\n", path);
        exit(1);
    }

    if(fseek(fp, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Failed to fseek\n");
        exit(1);
    }

    long bufsize = ftell(fp);
    if(bufsize == -1) {
        fprintf(stderr, "Failed to ftell\n");
        exit(1);
    }

    char* data = malloc(sizeof(char) * (bufsize + 1));

    if(fseek(fp, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to fseek\n");
        exit(1);
    }

    fread(data, sizeof(char), bufsize, fp);
    if(ferror(fp) != 0) {
        fprintf(stderr, "Failed to fread\n");
        exit(1);
    }

    data[bufsize] = '\0';
    fclose(fp);

    return data;
}

void compile_shader(GLuint shader, char const* source) {
    int source_len = strlen(source);
    glShaderSource(shader, 1, (const GLchar**)&source, &source_len);
    glCompileShader(shader);
}

GLuint create_shader(GLenum type, char const* source_path) {
    GLuint shader = glCreateShader(type);
    char* source = read_file(source_path);
    compile_shader(shader, source);
    free(source);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        size_t max_size = 10000;
        GLchar* log = (GLchar*)malloc(max_size * sizeof(GLchar));
        GLsizei length;
        glGetShaderInfoLog(shader, max_size, &length, log);

        fprintf(stderr, "shader compilation failed\n%.*s", length, log);
        exit(1);
    }

    return shader;
}

int main(int argc, char* argv[]) {
    // Initialize SDL.
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

    // Initialize window.
    static const int width = 800;
    static const int height = 600;

    SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    SDL_GLContext context = SDL_GL_CreateContext(window);

    // Initialize shader.
    GLuint vs = create_shader(GL_VERTEX_SHADER, vertex_shader_path);
    GLuint fs = create_shader(GL_FRAGMENT_SHADER, fragment_shader_path);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);

    glUseProgram(program);

    // Initialize view.
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, width, height);

    // Create vertex data.
    GLuint vao, vbo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

#define XY(x, y) x, y
    const GLfloat vertex_buffer_data[] = {XY(-1, -1), XY(1, -1), XY(1, 1), XY(-1, -1), XY(1, 1), XY(-1, 1)};
#undef XY

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    // Create the audio data buffers.
    GLuint pcm_gpu;
    glGenBuffers(1, &pcm_gpu);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, pcm_gpu);

    int num_pcm_samples = 1024;
    int pcm_buffer_size = num_pcm_samples * 2 * sizeof(GLfloat);

    // Using float32 here.
    int const num_buffer_floats = 1024;
    int const buffer_size = num_buffer_floats * 4;
    char* buffer_bytes = malloc(buffer_size + 1);
    float* buffer_floats = (float*)buffer_bytes;
    int buffer_offset = 0;

    GLfloat* pcm_host = (GLfloat*)malloc(pcm_buffer_size);
    int position = 0;

    glBufferData(GL_SHADER_STORAGE_BUFFER, pcm_buffer_size, pcm_host, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, pcm_gpu);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    // Main loop.
    while(true) {
        glClear(GL_COLOR_BUFFER_BIT);

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
            case SDL_KEYUP:
                if(event.key.keysym.sym == SDLK_ESCAPE) {
                    return 0;
                }
                break;
            }
        }
        int num_available_samples;

        do {
            int num_read = read(STDIN_FILENO, buffer_bytes + buffer_offset, buffer_size - buffer_offset);
            // Only use multiples of 2 floats.
            num_available_samples = (buffer_offset + num_read) / 8;
            int buffer_rest = (buffer_offset + num_read) % 8;
            for (int i = 0; i < num_available_samples; i++) {
                pcm_host[2 * position] = buffer_floats[2 * i];
                pcm_host[2 * position + 1] = buffer_floats[2 * i + 1];

                position++;
                if (position == num_pcm_samples) {
                    position = 0;
                }
            }

            memcpy(buffer_bytes, buffer_bytes + 8 * num_available_samples, buffer_rest);
        } while(num_available_samples != 0);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pcm_gpu);
        glBufferData(GL_SHADER_STORAGE_BUFFER, pcm_buffer_size, pcm_host, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        SDL_GL_SwapWindow(window);
        /* SDL_Delay(1); */
    }

    free(buffer_bytes);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
