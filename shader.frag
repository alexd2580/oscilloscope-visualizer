#version 450

out vec4 color;
in vec2 pos;

layout(std430, binding = 3) buffer main_layout
{
    int offset;
    vec2 pcm[];
};

void main() {
    float volume = 1.0;

    float min_dist_2 = 256 * 256;
    for(int i = 0; i < 1024 - offset; i++) {
        float x_dist_2 = (pos.x - volume * pcm[i].x) * (pos.x - volume * pcm[i].x);
        float y_dist_2 = (pos.y - volume * pcm[i + offset].y) * (pos.y - volume * pcm[i + offset].y);
        min_dist_2 = min(min_dist_2, x_dist_2 + y_dist_2);
    }
    float lum = 1.0 / (10000 * min_dist_2 + 1);
    color = vec4(lum / 2, lum, 0, 0);
}
