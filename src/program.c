// See https://antongerdelan.net/opengl/compute.html for reference.
#include <stdbool.h>
#include <stdio.h>
#include <time.h> // TODO

#include <sys/stat.h>
#include <sys/types.h>

#define GL_GLEXT_PROTOTYPES

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>

#include "program.h"

struct Program_ {
    char const* compute_shader_path;
    time_t compute_shader_mtime;
    GLuint compute_shader;

    GLuint program;
};

char* read_file(char const* path, int* size) {
    FILE* fp = fopen(path, "r");
    if(fp == NULL) {
        fprintf(stderr, "Cannot open file %s\n", path);
        exit(1);
    }

    if(fseek(fp, 0L, SEEK_END) != 0) {
        fprintf(stderr, "Failed to fseek\n");
        exit(1);
    }

    int bufsize = (int)ftell(fp);
    if(bufsize == -1) {
        fprintf(stderr, "Failed to ftell\n");
        exit(1);
    }

    if(size != NULL) {
        *size = bufsize;
    }

    char* data = malloc(sizeof(char) * (size_t)(bufsize + 1));

    if(fseek(fp, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to fseek\n");
        exit(1);
    }

    fread(data, sizeof(char), (size_t)bufsize, fp);
    if(ferror(fp) != 0) {
        fprintf(stderr, "Failed to fread\n");
        exit(1);
    }

    data[bufsize] = '\0';
    fclose(fp);

    return data;
}

time_t get_mtime(char const* path) {
    struct stat attr;
    stat(path, &attr);
    return attr.st_mtime;
}

Program create_program(char const* compute_shader_path) {
    Program program = (struct Program_*)malloc(sizeof(struct Program_));

    program->compute_shader_path = compute_shader_path;
    program->compute_shader = (GLuint)-1;
    program->program = (GLuint)-1;

    reinstall_program_if_valid(program);

    return program;
}

GLuint compile_shader(GLenum type, char const* source_path) {
    GLuint shader = glCreateShader(type);

    int source_len;
    char* source = read_file(source_path, &source_len);
    char const* const_source = source;

    glShaderSource(shader, 1, (GLchar const**)&const_source, &source_len);
    glCompileShader(shader);

    free(source);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE) {
        int max_size = 10000;
        GLchar* log = (GLchar*)malloc((size_t)max_size * sizeof(GLchar));
        GLsizei length;
        glGetShaderInfoLog(shader, max_size, &length, log);
        fprintf(stderr, "%s: shader compilation failed\n%.*s", source_path, length, log);
        free(log);
        glDeleteShader(shader);
        return (GLuint)-1;
    }

    return shader;
}

bool program_source_modified(Program program) {
    return get_mtime(program->compute_shader_path) > program->compute_shader_mtime;
}

void uninstall_program(Program program) {
    glDeleteProgram(program->program);
    glDeleteShader(program->compute_shader);
}

void reinstall_program_if_valid(Program program) {
    time_t t = time(NULL);
    struct tm* localized_time = localtime(&t);
    char s[1000];
    strftime(s, 1000, "%F %T", localized_time);
    printf("[%s] Compiling %s...\n", s, program->compute_shader_path);

    program->compute_shader_mtime = get_mtime(program->compute_shader_path);

    GLuint compute_shader = compile_shader(GL_COMPUTE_SHADER, program->compute_shader_path);
    if(compute_shader == (GLuint)-1) {
        return;
    }

    GLuint prgm = glCreateProgram();
    glAttachShader(prgm, compute_shader);
    glLinkProgram(prgm);

    GLint status;
    glGetProgramiv(prgm, GL_LINK_STATUS, &status);
    if(status == GL_FALSE) {
        int max_size = 10000;
        GLchar* log = (GLchar*)malloc((size_t)max_size * sizeof(GLchar));
        GLsizei length;
        glGetProgramInfoLog(prgm, max_size, &length, log);
        fprintf(stderr, "program linking failed\n%.*s", length, log);
        free(log);

        glDeleteProgram(prgm);
        glDeleteShader(compute_shader);

        return;
    }

    if(program->program != (GLuint)-1) {
        uninstall_program(program);
    }

    program->compute_shader = compute_shader;
    program->program = prgm;

    glUseProgram(prgm);
}

void reinstall_program_if_modified(Program program) {
    if(program_source_modified(program)) {
        reinstall_program_if_valid(program);
    }
}

void run_program(Program program, GLuint w, GLuint h) {
    glUseProgram(program->program);
    glDispatchCompute(w / 8, h / 8, 1);
    // Make sure writing to image has finished before read.
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void delete_program(Program program) { uninstall_program(program); }
