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

layout(binding = 1) uniform timer {
    float seconds;
};

layout(std430, binding = 2) buffer random {
    uint64_t random_seed[];
};

layout(std430, binding = 3) buffer pcm_data {
    int pcm_samples;
    int sample_index;
    // `pcm` contains `2 * pcm_samples` entries.
    // These are the left channel values followed by the right channel values.
    float pcm[];
};

layout(std430, binding = 4) buffer dft_data {
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

struct Primitive {
    int type;
    int i1;
    int i2;
    int i3;
    float f1;
    float f2;
    float f3;
    float f4;
};

layout(std430, binding = 5) buffer scene {
    int num_max_primitives;
    int num_primitives;
    int root_node;

    int _padding;

    Primitive primitives[];
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

#define UINT64_T_MAX (~uint64_t(0))
#define UINT32_T_MAX (~uint32_t(0))

uint64_t random_uint64(void) {
    int x = int((uv.x / 2.0 + 0.5) * width);
    int y = int((uv.y / 2.0 + 0.5) * height);

    int base_index = 4 * (y * width + x);

    const uint64_t result = rotl(random_seed[base_index + 0] + random_seed[base_index + 3], 23) + random_seed[base_index + 0];
    const uint64_t t = random_seed[base_index + 1] << 17;

    random_seed[base_index + 2] ^= random_seed[base_index + 0];
    random_seed[base_index + 3] ^= random_seed[base_index + 1];
    random_seed[base_index + 1] ^= random_seed[base_index + 2];
    random_seed[base_index + 0] ^= random_seed[base_index + 3];

    random_seed[base_index + 2] ^= t;

    random_seed[base_index + 3] = rotl(random_seed[base_index + 3], 45);

    return result;
}

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

float sdf_cylinder(vec3 pos, float radius) {
    return length(pos.yz) - radius;
}

float sdf_plane(vec3 pos, float offset) {
    return pos.y - offset;
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

float s = sin(0.22 * seconds);
float c = cos(0.096 * seconds);
vec3 light_pos = vec3(0, 1e2, 0) + vec3(1e3 * c, 0, 0) + vec3(0, 0, 1e3 * s);

#define HIGHEST_POSITIVE_BIT (1 << 30)

#define RAY_MARCH_STACK_SIZE 10
float sdf_scene(vec3 start_pos) {
    // Stack of nodes traversed.
    // Loop ends when the stack underflows.
    int node[RAY_MARCH_STACK_SIZE];
    node[0] = root_node;
    int node_index = 0;

    // Current position, pushed/ popped on transformation type primitives.
    // upon exiting a tranformation node to the top.
    vec3 pos[RAY_MARCH_STACK_SIZE];
    pos[0] = start_pos;
    int pos_index = 0;

    // Stack of distances previously computed (reverse polish notation?).
    float distance[RAY_MARCH_STACK_SIZE];
    int next_distance_index = 0;

    int iterations = 0;
    while(node_index >= 0 && iterations < 20) {
        iterations++;

        int primitive_index = node[node_index--];
        struct Primitive primitive = primitives[primitive_index & ~HIGHEST_POSITIVE_BIT];
        switch(primitive.type | (primitive_index & HIGHEST_POSITIVE_BIT)) {
            case 1: // PlaneType
                distance[next_distance_index++] = sdf_plane(pos[pos_index], primitive.f1);
                break;
            case 2: // CubeType
                distance[next_distance_index++] = sdf_cube(pos[pos_index], primitive.f1);
                break;
            case 3: // SphereType
                distance[next_distance_index++] = sdf_sphere(pos[pos_index], primitive.f1);
                break;
            case 4: // CylinderType
                distance[next_distance_index++] = sdf_cylinder(pos[pos_index], primitive.f1);
                break;
            case 5: // TranslationType
                break;
            case 6: // RotationType
                break;
            case 7: // ScalingType
                break;
            case 8: // RepetitionType
                break;
            case 1073741833: // (9 | HIGHEST_POSITIVE_BIT): // UnionType (children resolved)
                distance[next_distance_index - 2] = sdf_union(distance[next_distance_index - 2], distance[next_distance_index - 1]);
                next_distance_index -= 1;
                break;
            case 1073741834: // (10 | HIGHEST_POSITIVE_BIT): // IntersectionType (children resolved)
                distance[next_distance_index - 2] = sdf_intersect(distance[next_distance_index - 2], distance[next_distance_index  - 1]);
                next_distance_index -= 1;
                break;
            case 1073741835: // (11 | HIGHEST_POSITIVE_BIT): // ComplementType (children resolved)
                distance[next_distance_index - 2] = sdf_subtract(distance[next_distance_index - 2], distance[next_distance_index - 1]);
                next_distance_index -= 1;
                break;
            case 9: // UnionType
            case 10: // IntersectionType
            case 11: // ComplemenType
                // Push the negated type onto the stack, followed by right child, then left child.
                node[++node_index] = primitive_index | HIGHEST_POSITIVE_BIT;
                node[++node_index] = primitive.i2;
                node[++node_index] = primitive.i1;
                break;
            default:
                break;
        }
    }

    return distance[0];
}

vec3 normal_scene(vec3 pos) {
    const float eps = 0.1;
    const vec2 h = vec2(eps, 0);
    float d_pos = sdf_scene(pos);
    float dx = d_pos - sdf_scene(pos - h.xyy);
    float dy = d_pos - sdf_scene(pos - h.yxy);
    float dz = d_pos - sdf_scene(pos - h.yyx);
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
    vec3 start = point + 1 * normal;
    float max_dist = dot(light_pos - point, to_light);
    float light_visibility = 1.0 * ray_march_soft_shadow(start, to_light, max_dist);
    float illumination = light_visibility * max(dot(to_light, normal), 0);

    float ambient = 0.2;
    float diffuse = 0.8;

    return vec3(ambient + diffuse * illumination);
}

void main() {
    float aspect_ratio = 1920.0 / 1080.0;
    vec3 ray_origin = camera_origin;
    float fov_factor = tan(fovy / 2);
    vec3 ray_dir = normalize(camera_ahead + fov_factor * (uv.x * aspect_ratio * camera_right + uv.y * camera_up));
    vec3 ray_hit = ray_march(ray_origin, ray_dir);

    color = vec4(lighting(ray_hit), 1);
}
