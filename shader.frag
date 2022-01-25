#version 450

out vec4 color;
in vec2 pos;

// layout(std430, binding = 0) buffer settings_buffer {
//     int num_samples;
// };

layout(std430, binding = 2) buffer pcm_data {
    int pcm_samples;
    int sample_index;
    // `pcm` contains `2 * pcm_samples` entries.
    // These are the left channel values followed by the right channel values.
    float pcm[];
};

layout(std430, binding = 3) buffer dft_data {
    int dft_size;
    int dominant_frequency_period;
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

vec4 dft_blocks(vec2 p) {
    int data_size = dft_size / 2;

    float max_x = log2(float(data_size));
    float float_x = pow(2, max_x * p.x);
    int index = int(round(float_x));

    float y_raw = length(dft_at(index));
    float y_smooth = length(smooth_dft_at(index));

    if (p.y < y_raw / 256) {
        return vec4(1, 0, 0, 0);
    } else if (p.y < y_smooth / 256) {
        return vec4(1, 0.5, 0, 0);
    }
    return vec4(0);
}

vec4 pcm_moving_blocks(vec2 p, int block_size, int gap_size) {
    int window_size = block_size + gap_size;
    int relative_sample_index = window_size + int((pcm_samples - window_size) * p.x);
    int absolute_sample_index = sample_index - pcm_samples + relative_sample_index;
    int block_start_offset = absolute_sample_index % window_size;
    if (block_start_offset < gap_size) {
        return vec4(0);
    }
    int local_sample_index = relative_sample_index - block_start_offset;

    float value = 0;
    for (int i=0; i < window_size; i++) {
        value = max(abs(value), pcm[min(local_sample_index + i, pcm_samples - 1)]);
    }

    float lum = abs(value) > p.y ? 1.0 : 0.0;
    return vec4(lum, lum / 2, 0, 0);

    // float dist = abs(pcm[sample_num] - 2 * p.y);
    // float lum = 1.0 / (1000 * dist * dist + 1);
    // return vec4(lum, lum / 2, 0, 0);
}

vec4 pcm_still_blocks(vec2 p, int block_size, int gap_size) {
    int window_size = block_size + gap_size;
    int relative_sample_index = int((pcm_samples - window_size) * p.x);
    int block_start_offset = relative_sample_index % window_size;
    if (block_start_offset < gap_size) {
        return vec4(0);
    }
    int local_sample_index = relative_sample_index - block_start_offset;

    float value = 0;
    for (int i=0; i < window_size; i++) {
        value = max(abs(value), pcm[min(local_sample_index + i, pcm_samples - 1)]);
    }

    float lum = abs(value) > p.y ? 1.0 : 0.0;
    return vec4(lum, lum / 2, 0, 0);

    // float dist = abs(pcm[sample_num] - 2 * p.y);
    // float lum = 1.0 / (1000 * dist * dist + 1);
    // return vec4(lum, lum / 2, 0, 0);
}

vec4 pcm_line_32(vec2 p) {
    int sample_num = 199 * pcm_samples / 200 + int(pcm_samples / 200 * p.x);
    // sample_num -= sample_index % dominant_frequency_period;

    float dist = abs(pcm[sample_num] - 2.0 * p.y - 1.0);
    float lum = 1.0 / (1000 * dist * dist + 1);
    return vec4(lum, lum / 2, 0, 0);
}
vec4 pcm_line(vec2 p) {
    int sample_num = int(pcm_samples * p.x);

    float dist = abs(pcm[sample_num] - 2 * p.y);
    float lum = 1.0 / (1000 * dist * dist + 1);
    return vec4(lum, lum / 2, 0, 0);
}

vec2 scale(vec2 p, float x, float y) {
    return vec2(p.x * x, p.y * y);
}

vec2 translate(vec2 p, vec2 o) {
    return p + o;
}

void main() {
    // color = vec4(log2(dominant_frequency_period) / 20);
    // color = vec4(float(sample_index % 44100) / 44100);
    // return;
    // color = dft_blocks() + pcm_line(pos) + pcm_line_2(pos) + pcm_line_32(pos);
    color = pcm_still_blocks(pos, 500, 900);
    // main_xy();
    //
    // color = visualization(transform(pos));
}
