#version 450

in vec2 position;
out vec2 pos;

void main() {
    pos = position * 0.5 + vec2(0.5);
    gl_Position = vec4(position, 0.0, 1.0);
}
