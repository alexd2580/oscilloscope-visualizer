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
    char const* vertex_shader_path;
    char const* fragment_shader_path;

    time_t vertex_shader_mtime;
    time_t fragment_shader_mtime;

    GLuint vertex_shader;
    GLuint fragment_shader;

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

Program create_program(char const* vertex_shader_path, char const* fragment_shader_path) {
    Program program = (struct Program_*)malloc(sizeof(struct Program_));

    program->vertex_shader_path = vertex_shader_path;
    program->fragment_shader_path = fragment_shader_path;
    program->program = (GLuint)-1;
    program->vertex_shader = (GLuint)-1;
    program->fragment_shader = (GLuint)-1;

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
        fprintf(stderr, "shader compilation failed\n%.*s", length, log);
        free(log);
        glDeleteShader(shader);
        return (GLuint)-1;
    }

    return shader;
}

bool program_source_modified(Program program) {
      bool vertex_shader_updated = get_mtime(program->vertex_shader_path) > program->vertex_shader_mtime;
      bool fragment_shader_updated = get_mtime(program->fragment_shader_path) > program->fragment_shader_mtime;
      return vertex_shader_updated || fragment_shader_updated;
}

void uninstall_program(Program program) {
    glDeleteProgram(program->program);
    glDeleteShader(program->vertex_shader);
    glDeleteShader(program->fragment_shader);
}

void reinstall_program_if_valid(Program program) {
    time_t t = time(NULL);
    struct tm * localized_time = localtime(&t);
    char s[1000];
    strftime(s, 1000, "%F %T", localized_time);
    printf("[%s] Compiling...\n", s);

    program->vertex_shader_mtime = get_mtime(program->vertex_shader_path);
    program->fragment_shader_mtime = get_mtime(program->fragment_shader_path);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, program->vertex_shader_path);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, program->fragment_shader_path);
    if (vs == (GLuint)-1 || fs == (GLuint)-1) {
        return;
    }

    GLuint prgm = glCreateProgram();
    glAttachShader(prgm, vs);
    glAttachShader(prgm, fs);
    glBindAttribLocation(prgm, 0, "position");
    glLinkProgram(prgm);

    GLint status;
    glGetProgramiv(prgm, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        int max_size = 10000;
        GLchar* log = (GLchar*)malloc((size_t)max_size * sizeof(GLchar));
        GLsizei length;
        glGetProgramInfoLog(prgm, max_size, &length, log);
        fprintf(stderr, "program linking failed\n%.*s", length, log);
        free(log);

        glDeleteProgram(prgm);
        glDeleteShader(vs);
        glDeleteShader(fs);

        return;
    }

    if (program->program != (GLuint)-1) {
        uninstall_program(program);
    }

    program->vertex_shader = vs;
    program->fragment_shader = fs;
    program->program = prgm;

    glUseProgram(prgm);
}

void delete_program(Program program) {
    uninstall_program(program);
}
