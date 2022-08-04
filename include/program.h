#ifndef INCLUDE_PROGRAM_H
#define INCLUDE_PROGRAM_H

#include <SDL2/SDL_opengl.h>
#include <time.h>
#include <stdbool.h>

struct Program_;
typedef struct Program_* Program;

// Initialize and install a program.
Program create_program(char const* compute_shader_path);

// Check if the program source has been modified since last read.
bool program_source_modified(Program program);

// Try to compile and install a program, keep the old one if something fails.
void reinstall_program_if_valid(Program program);

// Combine the modified check and validity check.
void reinstall_program_if_modified(Program program);

// Run the program and wait for completion.
void run_program(Program program, GLuint w, GLuint h);

// Deinitializa all program resources.
void delete_program(Program program);

#endif
