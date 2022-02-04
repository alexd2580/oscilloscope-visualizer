#version 450
#extension GL_NV_gpu_shader5 : enable
#extension GL_ARB_gpu_shader_int64 : enable

out vec4 color;
in vec2 uv;

layout(binding = 0) uniform view {
    int width;
    int height;

    float fovy;
    float pitch;
    float roll;
    float yaw;

    int num_steps;

    float _pad;

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

layout(std430, binding = 4) buffer random {
    uint64_t random_seed[4];
};

float length_vec2_squared(vec2 v) {
    return dot(v, v);
}

float length_vec3_squared(vec3 v) {
    return dot(v, v);
}

uint64_t rotl(const uint64_t x, int k) {
   return (x << k) | (x >> (64 - k));
}

uint64_t random_uint64(void) {
    int x = int((uv.x / 2.0 + 0.5) * width);
    int y = int((uv.y / 2.0 + 0.5) * height);
    int base_index = 4 * (y * width + x);

    const uint64_t result = rotl(random_seed[base_index + 0] + random_seed[base_index + 3], 23) + random_seed[0];
    const uint64_t t = random_seed[base_index + 1] << 17;

    random_seed[base_index + 2] ^= random_seed[base_index + 0];
    random_seed[base_index + 3] ^= random_seed[base_index + 1];
    random_seed[base_index + 1] ^= random_seed[base_index + 2];
    random_seed[base_index + 0] ^= random_seed[base_index + 3];

    random_seed[base_index + 2] ^= t;

    random_seed[base_index + 3] = rotl(random_seed[base_index + 3], 45);

    return result;
}

#define UINT64_T_MAX (~uint64_t(0))
#define UINT32_T_MAX (~uint32_t(0))

float random_float(void) {
    return float(random_uint64()) / float(UINT64_T_MAX);
}

vec2 random_float2(void) {
    uint64_t a = random_uint64();
    return vec2(float(a >> 32) / float(UINT32_T_MAX), float(a & (UINT32_T_MAX)) / float(UINT32_T_MAX));
}

vec3 generate_nonlinear(vec3 v) {
    return vec3(1, float(v.y == 0 && v.z == 0), 0);
}

vec3 random_cosine_vec3(vec3 normal) {
    for(int i = 0; i < 100; i++) {
        vec2 a = 2 * random_float2() - vec2(1);
        float a_len_squared = length_vec2_squared(a);

        if (a_len_squared > 1.0) {
            continue;
        }

        vec3 nonlinear = generate_nonlinear(normal);
        vec3 n1 = normalize(cross(normal, nonlinear));
        vec3 n2 = cross(normal, n1);
        float height = sqrt(1 - a_len_squared);

        return height * normal + a.x * n1 + a.y * n2;
    }
}

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

float maxcomp(vec3 vec) {
    return max(vec.x, max(vec.y, vec.z));
}

// SDF Functions.

float sdf_sphere(vec3 pos, float radius) {
    return length(pos) - radius;
}

float sdf_cube(vec3 pos, float side_len) {
    vec3 to_corner = abs(pos) - vec3(side_len / 2);
    return length(max(to_corner, 0)) + min(maxcomp(to_corner), 0);
}

float sdf_infinite_cylinder(vec3 pos, vec3 direction, float radius) {
    return length(pos - dot(pos, direction) * direction) - radius;
}

float sdf_plane(vec3 pos, vec3 normal, float offset) {
    return abs(dot(pos, normal) - offset);
}

float sdf_subtract(float a, float b) {
    return max(a, -b);
}

float sdf_union(float a, float b) {
    return min(a, b);
}

float sdf_intersect(float a, float b) {
    return max(a, b);
}

float sdf_union_4(float a, float b, float c, float d) {
    return min(min(a, b), min(c, d));
}

// Position functions.

vec3 scale_inv(vec3 pos, vec3 scale) {
    return pos * scale;
}

vec3 at(vec3 pos, vec3 o) {
    return pos - o;
}

vec3 repeat_pos(vec3 pos, vec3 size_repeat) {
    return mod(pos + size_repeat * 0.5, size_repeat) - size_repeat * 0.5;
}

vec3 repeat_pos_int(vec3 pos, vec3 size_repeat, vec3 num_repeat) {
    vec3 wrapd = repeat_pos(pos, size_repeat);
    vec3 num_repeated = (pos - wrapd) / size_repeat;
    vec3 surplus_wraps = min(num_repeated + num_repeat, 0) + max(num_repeated - num_repeat, 0);
    return wrapd + surplus_wraps * size_repeat;
}

// Global.

float s = 0.3 * sin(0.22 * sample_index / 44100.0);
float c = 0.2 + 0.8 * (cos(0.096 * sample_index / 44100.0) + 1) / 2;
vec3 light_pos = 1e6 * (c * vec3(0, 1, 0) + s * vec3(1, 0, 1));

float sdf_scene(vec3 pos) {
    float p = sdf_plane(pos, vec3(0, 1, 0), 0);

    float cu1 = sdf_cube(at(pos, vec3(0, 5, 20)), 5.0);
    vec3 repeated_cube_pos = repeat_pos_int(at(pos, vec3(0, 20, 20)), vec3(5.5), vec3(2));
    float cu2 = sdf_cube(repeated_cube_pos, 5.0);
    float cubes = sdf_union(cu1, cu2);

    vec3 repeated_cylinder_pos = repeat_pos_int(at(pos, vec3(0, 10, 0)), vec3(sin(0.1 * sample_index / 44100) * 66), vec3(0, 1, 0));
    float cy1 = sdf_infinite_cylinder(repeated_cylinder_pos, vec3(1, 0, 0), 5.0);
    float cy2 = sdf_infinite_cylinder(repeated_cylinder_pos, vec3(1, 0, 0), 2.5);

    /* vec3 repeated_sphere_pos = repeat_pos(at(pos, vec3(0, 100, 0)), vec3(200)); */
    vec3 repeated_sphere_pos = repeat_pos_int(at(pos, vec3(0, 10, 0)), vec3(22), vec3(1, 3, 0));
    float s0 = sdf_sphere(repeated_sphere_pos, 10);
    float spheres = sdf_subtract(s0, cy1);

    float light = sdf_sphere(at(pos, light_pos), 10000);

    /* float sphere_bounds = sdf_cube(at(pos, vec3(0, 100, 0)), 3 * 200 * (1 + cos(0.2 * sample_index / 44100.0))); */
    /* float sphere_bounds_2 = sdf_cube(at(pos, vec3(0, 100, -500)), 3 * 200 * 2); */
    /* float limited_spheres = sdf_intersect(sdf_intersect(sphere_bounds, spheres), sphere_bounds_2); */

    return sdf_union_4(p, cubes, cy2, spheres);
}

vec3 normal_scene(vec3 pos) {
    const float eps = 0.1;
    const vec2 h = vec2(eps, 0);
    float dx = sdf_scene(pos + h.xyy) - sdf_scene(pos - h.xyy);
    float dy = sdf_scene(pos + h.yxy) - sdf_scene(pos - h.yxy);
    float dz = sdf_scene(pos + h.yyx) - sdf_scene(pos - h.yyx);
    return normalize(vec3(dx, dy, dz));
}

vec3 ray_march(vec3 ray_origin, vec3 ray_dir) {
    for (int i=0; i < num_steps; i++) {
        float distance_scene = sdf_scene(ray_origin);
        if (distance_scene < 0.01) {
            return ray_origin;
        }
        ray_origin += distance_scene * ray_dir;
    }
    return vec3(1.0 / 0.0);
}

float ray_march_soft_shadow(vec3 ray_origin, vec3 ray_dir, float max_dist) {
    float light = 1.0;
    float dist = 0.0;
    for (int i=0; i < num_steps; i++) {
        float distance_scene = sdf_scene(ray_origin + dist * ray_dir);
        if (distance_scene < 0.01) {
            return 0.0;
        }
        light = min(light, 30 * distance_scene / dist);
        dist += distance_scene;

        if (dist > max_dist) {
            return light;
        }
    }
    return 0.0;
}

vec3 lighting(vec3 point) {
    if(dot(point, point) > 1e30) {
        return vec3(0);
    }

    vec3 to_light = normalize(light_pos - point);
    vec3 normal = normal_scene(point);
    vec3 start = point + 0.1 * normal;
    float max_dist = dot(light_pos - point, to_light);
    float lightness = ray_march_soft_shadow(start, to_light, max_dist);
    float illumination = lightness * max(dot(to_light, normal), 0);

    float ambient = 0.2;
    float diffuse = 0.8;

    return vec3(ambient + diffuse * illumination);

    // vec3 c = vec3(1);
    // float ambient = 0.2;
    // vec3 normal = normal_scene(point);
    // float diffuse = dot(normal, normalize(light_pos - point));
    // float illumination = ambient + diffuse;
    // return vec3(illumination) * c;

    /* vec3 normal = normal_scene(point); */
    /* vec3 nonlinear = generate_nonlinear(normal); */
    /* vec3 n1 = normalize(cross(normal, nonlinear)); */
    /* vec3 n2 = cross(normal, n1); */
    /*  */
    /* vec3 accum = vec3(0.1); */
    /*  */
    /* for (int i1 = -1; i1 < 2; i1++) { */
    /*     for (int i2 = -1; i2 < 2; i2++) { */
    /*         vec3 p = point + 0.1 * i1 * n1 + 0.1 * i2 * n2; */
    /*         vec3 light_to_point = normalize(p - light_pos); */
    /*         vec3 light_bounce = ray_march(light_pos + 200000 * light_to_point, light_to_point); */
    /*         if (length_vec3_squared(light_bounce - p) < .1) { */
    /*             accum += 0.1111 * vec3(dot(normal, -light_to_point)); */
    /*         } */
    /*     } */
    /* } */


    /* for (int i = 0; i < 10; i++) { */
    /*     vec3 normal = normal_scene(point); */
    /*     vec3 random_ray = random_cosine_vec3(normal); */
    /*     vec3 bounce = ray_march(point + 0.02 * normal, random_ray); */
    /*     if (bounce.x < 10000000000.0) { */
    /*         vec3 light_to_point = normalize(bounce - light_pos); */
    /*         vec3 light_bounce = ray_march(light_pos, light_to_point); */
    /*         if (length_vec3_squared(light_bounce - bounce) < 1) { */
    /*             accum += vec3(0.1); */
    /*         } */
    /*     } */
    /* } */
    /* return accum; */
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
    float fov_factor = tan(fovy / 2);
    vec3 ray_dir = normalize(camera_ahead + fov_factor * (uv.x * aspect_ratio * camera_right + uv.y * camera_up));
    vec3 ray_hit = ray_march(ray_origin, ray_dir);
    color = vec4(lighting(ray_hit), 1);
}
