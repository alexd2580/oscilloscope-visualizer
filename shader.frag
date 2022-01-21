#version 450

out vec4 color;
in vec2 pos;

layout(std430, binding = 0) buffer settings_buffer {
    int num_samples;
};

layout(std430, binding = 1) buffer pcm_left_buffer {
    vec2 pcm_left[];
};

layout(std430, binding = 2) buffer pcm_right_buffer {
    vec2 pcm_right[];
};

layout(std430, binding = 3) buffer dft_buffer {
    float dft[];
};

/* void main() { */
/*     float volume = 1.0; */
/*  */
/*     float min_dist_2 = 256 * 256; */
/*     for(int i = 0; i < 1024 - offset; i++) { */
/*         float x_dist_2 = (pos.x - volume * pcm[i].x) * (pos.x - volume * pcm[i].x); */
/*         float y_dist_2 = (pos.y - volume * pcm[i + offset].y) * (pos.y - volume * pcm[i + offset].y); */
/*         min_dist_2 = min(min_dist_2, x_dist_2 + y_dist_2); */
/*     } */
/*     float lum = 1.0 / (10000 * min_dist_2 + 1); */
/*     color = vec4(lum / 2, lum, 0, 0); */
/* } */

void main_xy() {
    float volume = 1.0;
    float min_dist_2 = 256 * 256;
    for(int i = 0; i < 1024; i++) {
        int ix = num_samples - 1024 + i;
        vec2 pcm_pos = vec2(volume * pcm_left[ix].x, volume * pcm_left[ix].y);
        vec2 diff = pos - pcm_pos;
        min_dist_2 = min(min_dist_2, diff.x * diff.x + diff.y * diff.y);
    }
    float lum = 1.0 / (10000 * min_dist_2 + 1);
    color = vec4(lum / 2, lum, 0, 0);
}

float distance_to_line(vec2 p1, vec2 p2, vec2 x) {
    vec2 dir = p2 - p1;
    vec2 normal = vec2(dir.y, -dir.x) / length(dir);
    return abs(dot(normal, p1) - dot(normal, x));
}

// The following is bad...
void main_linear() {
    num_samples = 1000;
    int sample_num = int(num_samples * (pos.x + 1.0) / 2.0);

    vec2 prev_point = vec2(max(0, sample_num - 1), dft[max(0, sample_num - 1)]);
    vec2 this_point = vec2(sample_num, dft[sample_num]);
    vec2 next_point = vec2(min(sample_num + 1, num_samples - 1), dft[min(sample_num + 1, num_samples - 1)]);

    vec2 cur_pos = vec2(sample_num, pos.y);

    float dist_to_value = distance_to_line(prev_point, this_point, cur_pos);
    float lum = 1.0 / (1000 * dist_to_value * dist_to_value + 1);
    color = vec4(lum, lum / 2, 0, 0);
}

void main() {
    main_linear();
}
