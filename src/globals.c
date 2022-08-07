#include"globals.h"

__attribute__((const)) float mix(float a, float b, float x) { return a + (b - a) * x; }
