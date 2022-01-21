build:
	gcc -std=c11 sdl_oscilloscope.c `sdl2-config --libs --cflags` -lGL -Wall -g -lpthread -lfftw3f

run:
	parec --raw --format=float32le | ./a.out
