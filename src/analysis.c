#include <alloca.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <fftw3.h>

#include "analysis.h"
#include "buffers.h"
#include "defines.h"
#include "dft.h"
#include "pcm.h"

static int const frequencies[8] = {16, 60, 250, 500, 2000, 4000, 6000, 22000};

struct Analysis_ {
    int sample_rate;
    int dft_size;

    int frequency_indices[8];

    /* BEAT DETECTION */
    int num_beat_frequencies;

    int* beat_frequencies;
    int* beat_frequency_indices;

    // Time of last beat detection.
    time_t last_time;
    // Duration between runs of beat_detection.
    // Used to adjust the amount of samples for the short term average.
    float moving_average_process_time_ms;

    // No beat detected below this value.
    float* noise_multipliers;
    // beat <=> current > multiplier * average && average > noise
    float* beat_threshold_multipliers;

    // Interleaved format. Use `num_beat_frequencies * i + [position of beat frequency]` to access.
    int short_term_average_ms;
    int num_beat_samples;
    int sample_index;
    float* dft_values;

    // Sum of frequency "strengths" during last X seconds. (Computed from `dft_values`)
    float* sums_short_average;
    float* sum_squares_short_average;
    float* long_term_moving_averages;

    /* GPU DATA */

    // Using struct here to align memory.
    struct GpuData {
        int is_beat;
        int beats;
        int bpm;
        int other;

        struct BandData {
            float accumulated;
            float window;
            float smooth_window;
            // second derivative
            float delta2;
            float movement;
            float other1, other2, other3;
        } bands[7];
    } data;

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

__attribute__((const)) float mix(float a, float b, float x) { return a + (b - a) * x; }

Analysis create_analysis(Pcm pcm, DftData dft_data, unsigned int index) {
    Analysis analysis = (Analysis)malloc(sizeof(struct Analysis_));

    // Core data.
    int sample_rate = sample_rate_of_pcm(pcm);
    analysis->sample_rate = sample_rate;
    int dft_size = size_of_dft(dft_data);
    analysis->dft_size = dft_size;

    // Band border frequenci indices.
    int max_index = analysis->dft_size / 2 + 1;
    for(int f = 0; f < 8; f++) {
        int fi = index_of_frequency(frequencies[f], analysis->sample_rate, analysis->dft_size);
        analysis->frequency_indices[f] = CLAMP(fi, 1, max_index);
    }

    // Beat detection.
    analysis->num_beat_frequencies = 4;
    analysis->beat_frequencies = (int*)malloc((size_t)analysis->num_beat_frequencies * sizeof(int));
    analysis->beat_frequencies[0] = 60;
    analysis->beat_frequencies[1] = 75;
    analysis->beat_frequencies[2] = 95;
    analysis->beat_frequencies[3] = 110;
    analysis->num_beat_frequencies = 3;

    analysis->beat_frequency_indices = (int*)malloc((size_t)analysis->num_beat_frequencies * sizeof(int));
    for(int i = 0; i < analysis->num_beat_frequencies; i++) {
        analysis->beat_frequency_indices[i] = index_of_frequency(analysis->beat_frequencies[i], sample_rate, dft_size);
    }

    analysis->last_time = clock();
    analysis->moving_average_process_time_ms = 16; // one second

    analysis->noise_multipliers = (float*)malloc((size_t)analysis->num_beat_frequencies * sizeof(float));
    analysis->noise_multipliers[0] = 0.25f;
    analysis->noise_multipliers[1] = 0.25f;
    analysis->noise_multipliers[2] = 0.25f;
    analysis->noise_multipliers[3] = 0.25f;

    analysis->beat_threshold_multipliers = (float*)malloc((size_t)analysis->num_beat_frequencies * sizeof(float));
    analysis->beat_threshold_multipliers[0] = 3.5f;
    analysis->beat_threshold_multipliers[1] = 3.5f;
    analysis->beat_threshold_multipliers[2] = 3.5f;
    analysis->beat_threshold_multipliers[3] = 3.5f;

    analysis->short_term_average_ms = 8000;
    analysis->num_beat_samples =
        (int)((float)analysis->short_term_average_ms / analysis->moving_average_process_time_ms);
    analysis->sample_index = 0;
    int num_dft_values = analysis->num_beat_frequencies * analysis->num_beat_samples;
    analysis->dft_values = (float*)malloc((size_t)num_dft_values * sizeof(float));
    for(int i = 0; i < num_dft_values; i++) {
        analysis->dft_values[i] = 0.f;
    }

    analysis->sums_short_average = (float*)malloc((size_t)analysis->num_beat_frequencies * sizeof(float));
    analysis->sum_squares_short_average = (float*)malloc((size_t)analysis->num_beat_frequencies * sizeof(float));
    analysis->long_term_moving_averages = (float*)malloc((size_t)analysis->num_beat_frequencies * sizeof(float));

    for(int i = 0; i < analysis->num_beat_frequencies; i++) {
        analysis->sums_short_average[i] = 0.f;
        analysis->sum_squares_short_average[i] = 0.f;
        analysis->long_term_moving_averages[i] = 1.f;
    }

    // CPU side buffer data.
    analysis->data.is_beat = false;
    analysis->data.beats = 0;
    analysis->data.bpm = 0;
    analysis->data.other = 0;
    for(int band = 0; band < 7; band++) {
        analysis->data.bands[band].accumulated = 0.0f;
        analysis->data.bands[band].window = 0.0f;
        analysis->data.bands[band].smooth_window = 0.0f;
        analysis->data.bands[band].delta2 = 0.0f;
        analysis->data.bands[band].movement = 0.0f;
    }

    // GPU buffer.
    analysis->gpu_buffer_size = isizeof(analysis->data);
    analysis->buffer = create_storage_buffer(analysis->gpu_buffer_size, index);

    return analysis;
}

void compute_and_copy_analysis_to_gpu(DftData dft_data, Analysis analysis) {
    // Bands.
    for(int band = 0; band < 7; band++) {
        float window = 0.0f;
        for(int index = analysis->frequency_indices[band]; index < analysis->frequency_indices[band + 1]; index++) {
            window += dft_at(dft_data, index);
        }
        /* if (band == 1) { */
        /*     printf("\n"); */
        /* } */

        float delta2 = fabsf(analysis->data.bands[band].window - window);
        // Smoothing?
        /* float avg_delta2 = mix(analysis->bands[band].avg_delta2, cur_delta2, 0.2f); */

        /* if (band == 1) { */
        /* printf("wind: %.2f \t last wind: %.2f \t avg delta: %.2f \t cur_delta: %.2f \n", (double)windowed,
         * (double)analysis->bands[band].windowed, (double)analysis->bands[band].avg_delta2, (double)cur_delta2); */
        /* } */

        analysis->data.bands[band].window = window;
        analysis->data.bands[band].smooth_window = mix(analysis->data.bands[band].smooth_window, window, 0.02f);
        analysis->data.bands[band].accumulated += window;

        analysis->data.bands[band].delta2 = delta2;
        analysis->data.bands[band].movement += delta2;
    }

    // Beat detection.
    // Check and adjust timing.
    time_t this_time = clock();
    float const s_to_ms = 1000.0f;
    float delta_ms = s_to_ms * (float)(this_time - analysis->last_time) / CLOCKS_PER_SEC;
    analysis->last_time = this_time;
    float old_avg_process_time = analysis->moving_average_process_time_ms;
    analysis->moving_average_process_time_ms = mix(old_avg_process_time, delta_ms, 0.01f);
    int num_old_samples = analysis->num_beat_samples;
    int num_new_samples = (int)((float)analysis->short_term_average_ms / analysis->moving_average_process_time_ms);

    float difference_percent = fabsf((float)(num_new_samples - num_old_samples)) / (float)(num_old_samples);
    if(difference_percent > 0.15f) {
        printf("Too much beat buffer size difference:\n");
        printf("\tPrev delta %.2f\tCurrent delta %.2f\tNew delta %.2f\n", (double)old_avg_process_time,
               (double)delta_ms, (double)analysis->moving_average_process_time_ms);
        printf("\tPrev #samples %d\tNew #samples %d\n", num_old_samples, num_new_samples);

        int num_freqs = analysis->num_beat_frequencies;
        float* new_dft_values = (float*)malloc((size_t)(num_new_samples * num_freqs) * sizeof(float));

        // Reset the sums.
        for(int i = 0; i < num_freqs; i++) {
            analysis->sums_short_average[i] = 0.f;
            analysis->sum_squares_short_average[i] = 0.f;
        }

        // Copy all dft values to the new buffer (recompute_sum).
        int num_values_to_copy = MIN(num_old_samples, num_new_samples);
        for(int sample_index = 0; sample_index < num_values_to_copy; sample_index++) {
            // For each beat frequency.
            for(int freq_index = 0; freq_index < num_freqs; freq_index++) {
                int old_index = num_freqs * ((analysis->sample_index + sample_index) % num_old_samples) + freq_index;
                float value = analysis->dft_values[old_index];
                int new_index = num_freqs * sample_index + freq_index;
                new_dft_values[new_index] = value;
                analysis->sums_short_average[freq_index] += value;
                analysis->sum_squares_short_average[freq_index] += value * value;
            }
        }

        // For each beat frequency.
        for(int freq_index = 0; freq_index < num_freqs; freq_index++) {
            // Fill potential extension with average build from copied portion of values.
            float freq_average = analysis->sums_short_average[freq_index] / (float)num_values_to_copy;
            for(int sample_index = num_values_to_copy; sample_index < num_new_samples; sample_index++) {
                int new_index = num_freqs * sample_index + freq_index;
                new_dft_values[new_index] = freq_average;
                analysis->sums_short_average[freq_index] += freq_average;
                analysis->sum_squares_short_average[freq_index] += freq_average * freq_average;
            }
        }

        analysis->num_beat_samples = num_new_samples;
        // Index unwrolled to 0.
        analysis->sample_index = 0;
        free(analysis->dft_values);
        analysis->dft_values = new_dft_values;
    }

    bool total_is_beat = false;
    bool* is_beat = (bool*)malloc((size_t)analysis->num_beat_frequencies * sizeof(bool));
    float* sd = (float*)malloc((size_t)analysis->num_beat_frequencies * sizeof(float));

    for(int i = 0; i < analysis->num_beat_frequencies; i++) {
        // Adjust average TODO recompute entirely every X frames (it's not an int, it's a float!)?
        // Subtract value i'm going to replace.
        int dft_value_index = analysis->sample_index * analysis->num_beat_frequencies + i;
        float prev_value = analysis->dft_values[dft_value_index];
        analysis->sums_short_average[i] -= prev_value;
        analysis->sum_squares_short_average[i] -= prev_value * prev_value;

        // Add the current value.
        float current_value = dft_at(dft_data, analysis->beat_frequency_indices[i]);
        analysis->dft_values[dft_value_index] = current_value;
        analysis->sums_short_average[i] += current_value;
        analysis->sum_squares_short_average[i] += current_value * current_value;
        analysis->sample_index++;
        analysis->sample_index = analysis->sample_index % analysis->num_beat_samples;

        // Compute the average.
        float short_average = analysis->sums_short_average[i] / (float)analysis->num_beat_samples;
        float average_long_term = mix(analysis->long_term_moving_averages[i], short_average, 0.0001f);
        analysis->long_term_moving_averages[i] = average_long_term;

        // Compute SD as `sqrt(average of squares - average squared)`.
        float average_of_squares = analysis->sum_squares_short_average[i] / (float)analysis->num_beat_samples;
        float squared_average = powf(analysis->sums_short_average[i] / (float)analysis->num_beat_samples, 2);
        sd[i] = sqrtf(average_of_squares - squared_average);

        // Detect beat.
        bool is_not_noise = short_average > analysis->noise_multipliers[i] * average_long_term;
        bool is_over_sd = current_value > short_average + 2.4f * sd[i];
        bool is_local_beat = current_value > analysis->beat_threshold_multipliers[i] * short_average;
        is_beat[i] = is_not_noise && is_over_sd; // && is_local_beat;
        total_is_beat |= is_beat[i];
    }

    if(total_is_beat) {
        analysis->data.beats += analysis->data.is_beat ? 0 : 1;
        /* printf("BEAT %d\n", analysis->data.beats); */

        if(/* show debug*/ false) {
            for(int i = 0; i < analysis->num_beat_frequencies; i++) {
                float avg = analysis->sums_short_average[i] / (float)analysis->num_beat_samples;
                printf("hz %d\tidx %d\tnoize %.2f\tavg %.2f\tthresh %.2f\tcur %.2f\tbeat? %d\n",
                       analysis->beat_frequencies[i], analysis->beat_frequency_indices[i],
                       (double)(analysis->noise_multipliers[i] * analysis->long_term_moving_averages[i]), (double)avg,
                       (double)(analysis->beat_threshold_multipliers[i] * avg),
                       (double)dft_at(dft_data, analysis->beat_frequency_indices[i]), is_beat[i]);
            }
        }
    }

    if(/* show print*/ true) {
        printf("%d", analysis->data.beats);
        for(int i = 0; i < analysis->num_beat_frequencies; i++) {
            float avg = analysis->sums_short_average[i] / (float)analysis->num_beat_samples;
            /* hz idx noize avg thresh cur sd is_beat */
            printf(",%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%d", analysis->beat_frequencies[i],
                   analysis->beat_frequency_indices[i],
                   (double)(analysis->noise_multipliers[i] * analysis->long_term_moving_averages[i]), (double)avg,
                   (double)(analysis->beat_threshold_multipliers[i] * avg),
                   (double)dft_at(dft_data, analysis->beat_frequency_indices[i]), avg + 2.4f * sd[i], is_beat[i]);
        }
        printf("\n");
    }

    free(sd);
    free(is_beat);

    analysis->data.is_beat = total_is_beat;

    copy_buffer_to_gpu(analysis->buffer, (char*)&analysis->data, 0, analysis->gpu_buffer_size);
}

void delete_analysis(Analysis analysis) {
    free(analysis->long_term_moving_averages);
    free(analysis->sum_squares_short_average);
    free(analysis->sums_short_average);
    free(analysis->dft_values);
    free(analysis->beat_threshold_multipliers);
    free(analysis->noise_multipliers);
    free(analysis->beat_frequency_indices);
    free(analysis->beat_frequencies);
    delete_buffer(analysis->buffer);
    free(analysis);
}
