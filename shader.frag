#version 450

out vec4 color;
in vec2 pos;

// layout(std430, binding = 0) buffer settings_buffer {
//     int num_samples;
// };

layout(std430, binding = 2) buffer pcm_data {
    int pcm_samples;
    // `pcm` contains `2 * pcm_samples` entries.
    // These are the left channel values followed by the right channel values.
    float pcm[];
};

layout(std430, binding = 3) buffer dft_data {
    int dft_size;
    // `dft` contains `dft_size` entries.
    // These are the raw dft values followed by the smoothed values.
    // Both raw and smooth sections follow the same pattern:
    // First come `dft_size / 2 + 1` real values.
    // Then come the `dft_size / 2` imaginary values in reverse, starting at
    // `dft_size / 2 - 1` (for even sizes). There is no imaginary part for the
    // first and last value.
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
        int ix = pcm_samples - 1024 + i;
        vec2 pcm_pos = vec2(volume * pcm[ix], volume * pcm[ix + pcm_samples]);
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

vec2 dft_at(int index) {
    return vec2(dft[index], index == 0 || index == (dft_size / 2) ? 0.0 : dft[dft_size - index]);
}

vec2 smooth_dft_at(int index) {
    return vec2(dft[dft_size + index], index == 0 || index == (dft_size / 2) ? 0.0 : dft[dft_size + dft_size - index]);
}

void main_dft() {
    int data_size = (dft_size / 2);
    float max_x = log2(float(data_size));
    float rel_x = (pos.x + 1.0) / 2.0;
    // For linear
    // float float_x = rel_x * data_size;
    // For logarithmic
    float float_x = pow(2, max_x * rel_x);

    int index_a = int(floor(float_x));
    int index_b = int(ceil(float_x));

    float y_a = length(dft_at(index_a));
    float y_b = length(dft_at(index_b));

    // float y_a = sin(floor(float_x));
    // float y_b = sin(ceil(float_x));
    float mix_ab = fract(float_x);
    float dft_y = mix(y_a, y_b, mix_ab);

    float vis_y = float_x * dft_y / 16000.0;
    float pos_y = 0.5 * pos.y + 0.5;

    float lum = pos_y < dft_y / 256 ? 1.0 : 0.0;
    color = vec4(lum, lum / 2, 0, 0);
}

void main_linear_pcm() {
    int sample_num = int(pcm_samples * (pos.x + 1.0) / 2.0);

    // vec2 prev_point = vec2(max(0, sample_num - 1), pcm[max(0, sample_num - 1)]);
    // vec2 this_point = vec2(sample_num, pcm[sample_num]);
    // vec2 next_point = vec2(min(sample_num + 1, pcm_samples - 1), pcm[min(sample_num + 1, pcm_samples - 1)]);
    //
    // vec2 cur_pos = vec2(sample_num, pos.y);
    //
    // float dist_to_value = distance_to_line(prev_point, this_point, cur_pos);
    float dist = abs(pcm[sample_num] - pos.y);
    float lum = 1.0 / (1000 * dist * dist + 1);
    color = color / 0.99 + 0.01 * vec4(lum, lum / 2, 0, 0);
}

void main() {
    main_dft();
    // main_xy();
}
