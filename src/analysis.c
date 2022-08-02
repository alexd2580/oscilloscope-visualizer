#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <fftw3.h>

#include "analysis.h"
#include "buffers.h"
#include "defines.h"
#include "dft.h"
#include "pcm.h"

struct BandData {
    float accumulated;
    float window;
    float smooth_window;
    // second derivative
    float delta2;
    float movement;
    float other1, other2, other3;
};

static int const frequencies[8] = {16, 60, 250, 500, 2000, 4000, 6000, 22000};

struct Analysis_ {
    int sample_rate;
    int dft_size;

    int frequency_indices[8];

    struct BandData bands[7];

    int gpu_buffer_size;
    Buffer buffer;
};

__attribute__((const)) int index_of_frequency(int frequency, int sample_rate, int dft_size) {
    // For reference see
    // https://stackoverflow.com/questions/4364823/how-do-i-obtain-the-frequencies-of-each-value-in-an-fft
    // 0:   0 * 44100 / 1024 =     0.0 Hz
    // 1:   1 * 44100 / 1024 =    43.1 Hz
    // 2:   2 * 44100 / 1024 =    86.1 Hz
    // 3:   3 * 44100 / 1024 =   129.2 Hz

    return (int)(roundf((float)frequency * (float)dft_size / (float)sample_rate));
}

__attribute__((const)) float mix(float a, float b, float x) {
    return a + (b - a) * x;
}

Analysis create_analysis(Pcm pcm, DftData dft_data, unsigned int index) {
    Analysis analysis = (Analysis)malloc(sizeof(struct Analysis_));

    analysis->sample_rate = sample_rate_of_pcm(pcm);
    analysis->dft_size = size_of_dft(dft_data);

    for(int band = 0; band < 7; band++) {
        analysis->bands[band].accumulated = 0.0f;
        analysis->bands[band].window = 0.0f;
        analysis->bands[band].smooth_window = 0.0f;
        analysis->bands[band].delta2 = 0.0f;
        analysis->bands[band].movement = 0.0f;
    }

    int max_index = analysis->dft_size / 2 + 1;
    for(int f = 0; f < 8; f++) {
        int fi = index_of_frequency(frequencies[f], analysis->sample_rate, analysis->dft_size);
        analysis->frequency_indices[f] = CLAMP(fi, 1, max_index);
    }

    analysis->gpu_buffer_size = isizeof(analysis->bands);
    analysis->buffer = create_storage_buffer(analysis->gpu_buffer_size, index);

    return analysis;
}

void compute_and_copy_analysis_to_gpu(DftData dft_data, Analysis analysis) {
    for(int band = 0; band < 7; band++) {
        float window = 0.0f;
        for(int index = analysis->frequency_indices[band]; index < analysis->frequency_indices[band + 1]; index++) {
            window += dft_at(dft_data, index);
            /* if (band == 1) { */
            /*     printf("%.2f\t", (double)dft_at(dft_data, index)); */
            /* } */
        }
        /* if (band == 1) { */
        /*     printf("\n"); */
        /* } */

        float delta2 = fabsf(analysis->bands[band].window - window);
        // Smoothing?
        /* float avg_delta2 = mix(analysis->bands[band].avg_delta2, cur_delta2, 0.2f); */

        /* if (band == 1) { */
        /* printf("wind: %.2f \t last wind: %.2f \t avg delta: %.2f \t cur_delta: %.2f \n", (double)windowed, (double)analysis->bands[band].windowed, (double)analysis->bands[band].avg_delta2, (double)cur_delta2); */
        /* } */

        analysis->bands[band].window = window;
        analysis->bands[band].smooth_window = mix(analysis->bands[band].smooth_window, window, 0.02f);
        analysis->bands[band].accumulated += window;

        analysis->bands[band].delta2 = delta2;
        analysis->bands[band].movement += delta2;


    }

    copy_buffer_to_gpu(analysis->buffer, (char*)&analysis->bands, 0, analysis->gpu_buffer_size);
}

void delete_analysis(Analysis analysis) {
    delete_buffer(analysis->buffer);
    free(analysis);
}
