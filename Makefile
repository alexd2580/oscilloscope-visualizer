build:
	gcc -std=c11 sdl_oscilloscope.c program.c pcm.c buffers.c `sdl2-config --libs --cflags` -lGL -Wall -Wextra -g -lpthread -lfftw3f -lm -O2

run:
	parec --raw --format=float32le | ./a.out
