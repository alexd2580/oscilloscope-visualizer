#version 450

out vec4 color;
in vec2 uv;

layout(binding = 0) uniform view {
    int width;
    int height;

    float _pad1;
    float _pad2;

    float fov;
    float pitch;
    float roll;
    float yaw;

    vec3 camera_origin;
    vec3 camera_right;
    vec3 camera_ahead;
    vec3 camera_up;
};

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

// void main_xy() {
//     float volume = 1.0;
//     float min_dist_2 = 256 * 256;
//     for(int i = 0; i < 1024; i++) {
//         int ix = pcm_samples - 1024 + i;
//         vec2 pcm_pos = vec2(volume * pcm[ix], volume * pcm[ix + pcm_samples]);
//         vec2 diff = pos - pcm_pos;
//         min_dist_2 = min(min_dist_2, diff.x * diff.x + diff.y * diff.y);
//     }
//     float lum = 1.0 / (10000 * min_dist_2 + 1);
//     color = vec4(lum / 2, lum, 0, 0);
// }

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
    return vec4(lum, 0.0, lum, 0);

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
    return vec4(lum, 0.0, lum, 0);
}

vec2 scale(vec2 pos, float x, float y) {
    return vec2(pos.x * x, pos.y * y);
}

vec2 translate(vec2 pos, vec2 o) {
    return pos + o;
}

float sdf_sphere(vec3 pos, vec3 sphere, float radius) {
    return length(pos - sphere) - radius;
}

float sdf_xz_plane(vec3 pos, float plane_y) {
    return abs(pos.y - plane_y);
}

float sphere_radius = 0.0;

float sdf_scene(vec3 pos) {
    float d = min(sdf_sphere(pos, vec3(0, 1, 0), 20 * sphere_radius), sdf_xz_plane(pos, 0));

    for (int i = 0; i< 50; i++) {
        d = min(d, sdf_sphere(pos, vec3(-500, 0, -25 + float(i)), dft[2 + i + dft_size]));
    }

    return d;
}

vec3 normal_scene(vec3 pos) {
    const float eps = 0.0001;
    const vec2 h = vec2(eps, 0);
    float dx = sdf_scene(pos + h.xyy) - sdf_scene(pos - h.xyy);
    float dy = sdf_scene(pos + h.yxy) - sdf_scene(pos - h.yxy);
    float dz = sdf_scene(pos + h.yyx) - sdf_scene(pos - h.yyx);
    return normalize(vec3(dx, dy, dz));
}

void main() {
    // color = vec4(log2(dominant_frequency_period) / 20);
    // color = vec4(float(sample_index % 44100) / 44100);
    // return;
    // color = dft_blocks() + pcm_line(pos) + pcm_line_2(pos) + pcm_line_32(pos);
    // color = dft_blocks(pos, 500, 900);
    // color = dft_blocks(pos) + pcm_moving_blocks(pos, 2000, 2000);
    // main_xy();
    //
    // color = visualization(transform(pos));

    float aspect_ratio = 1920.0 / 1080.0;
    vec3 ray_origin = camera_origin;
    vec3 ray_dir = normalize(camera_ahead + uv.x * aspect_ratio * camera_right + uv.y * camera_up);

    for (int i=0; i< 500; i++) {
        sphere_radius += abs(pcm[pcm_samples - 1 - i]);
    }
    sphere_radius /= 1000.0;

    for (int i=0; i< 50; i++) {
        float distance_scene = sdf_scene(ray_origin);
        if (distance_scene < 0.1) {
            break;
        }
        ray_origin += distance_scene * ray_dir;
    }

    vec3 light_pos = vec3(sin(sample_index / 44100.0), 10, cos(sample_index / 44100.0));

    vec4 c = vec4(1, 0, 1, 1);
    float ambient = 0.2;
    float diffuse = dot(normal_scene(ray_origin), normalize(light_pos - ray_origin));
    float illumination = ambient + diffuse;
    color = vec4(vec3(illumination), 1) * c;
}
