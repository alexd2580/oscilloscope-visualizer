#version 450

in vec2 position;
out vec2 uv;

void main() {
    uv = position;
    gl_Position = vec4(position, 0.0, 1.0);
}
