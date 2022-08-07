#include <alloca.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <fftw3.h>

#include "analysis.h"
#include "buffers.h"
#include "globals.h"
#include "dft.h"
#include "pcm.h"

static int const frequencies[8] = {16, 60, 250, 500, 2000, 4000, 6000, 22000};

struct Analysis_ {
    int sample_rate;
    int dft_size;

    int frequency_indices[8];

    /* BEAT DETECTION */
    int num_beat_frequencies;

    // Time of last beat detection: t0.
    time_t last_time;
    // Duration between runs of beat_detection: delta = E[t1 - t0].
    // Used to adjust the amount of samples for the short term average.
    float moving_average_process_time_ms;
    // Desired duration of the short average: T.
    float short_average_window_ms;
    // Number of samples to reach desired average duration: T / delta.
    int num_beat_samples;

    // Index of next dft value in ring buffer.
    int sample_index;

    struct BeatAnalysis {
        int hz;
        int dft_index;

        float noise_threshold_factor;
        float beat_threshold_factor;

        float* dft_values;

        // Slowly moving average.
        float long_moving_average;
        // Sum of last `num_beat_samples` `dft_values`.
        float short_sum;
        // Sum of squares of last `num_beat_samples` `dft_values`.
        float short_square_sum;
        // Standard deviation.
        float sd;

        bool is_beat;
    } * beat_analysis;

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

void set_band_border_indices(Analysis analysis) {
    int max_index = analysis->dft_size / 2 + 1;
    FORI(0, 8) {
        int fi = index_of_frequency(frequencies[i], analysis->sample_rate, analysis->dft_size);
        analysis->frequency_indices[i] = CLAMP(fi, 1, max_index);
    }
}

void initialize_beat_frequency(Analysis analysis, int beat_fequency_index) {
    struct BeatAnalysis* beat_analysis = analysis->beat_analysis + beat_fequency_index;

    beat_analysis->dft_index = index_of_frequency(beat_analysis->hz, analysis->sample_rate, analysis->dft_size);
    beat_analysis->noise_threshold_factor = 0.25f;
    beat_analysis->beat_threshold_factor = 3.5f;

    beat_analysis->dft_values = ALLOCATE(analysis->num_beat_samples, float);
    FORI(0, analysis->num_beat_samples) { beat_analysis->dft_values[i] = 0.f; }

    beat_analysis->long_moving_average = 1.f;
    beat_analysis->short_sum = 0.f;
    beat_analysis->short_square_sum = 0.f;
    beat_analysis->sd = 0.f;

    beat_analysis->is_beat = false;
}

void initialize_beat_detection(Analysis analysis) {
    // Common data.
    analysis->num_beat_frequencies = 3;
    analysis->last_time = clock();
    // Initialize to something... e.g. 16ms @60FPS = 1s.
    analysis->moving_average_process_time_ms = 16;
    // 8 seconds.
    analysis->short_average_window_ms = 8000;
    analysis->num_beat_samples = (int)(analysis->short_average_window_ms / analysis->moving_average_process_time_ms);
    analysis->sample_index = 0;

    analysis->beat_analysis = ALLOCATE(analysis->num_beat_frequencies, struct BeatAnalysis);

    analysis->beat_analysis[0].hz = 60;
    analysis->beat_analysis[1].hz = 75;
    analysis->beat_analysis[2].hz = 95;
    /* analysis->beat_analysis[3].hz = 110; */

    /* analysis->num_beat_frequencies = 3; */
    FORI(0, analysis->num_beat_frequencies) { initialize_beat_frequency(analysis, i); }
}

void reinitialize_beat_frequency(Analysis analysis, int beat_fequency_index, int new_num_samples) {
    struct BeatAnalysis* beat = analysis->beat_analysis + beat_fequency_index;

    float* dft_values = ALLOCATE(new_num_samples, float);

    // I'm going to recompute the short sums.
    // SD is a computed value and the long average will adjust in time.
    // These two don't need to be proactively recomputed.
    beat->short_sum = 0.f;
    beat->short_square_sum = 0.f;

    // TODO THIS IS WRONG< WE NEED TO COPY THE VALUES IN REVERSE.
    // IT DOESN"T MAKE A HUGE DIFFERENCE< BUT THIS IS WRONG ANYWAY.

    // Copy all fitting dft values to the new buffer and recompute_sums.
    int num_values_to_copy = MIN(analysis->num_beat_samples, new_num_samples);
    FORI(0, num_values_to_copy) {
        float old_value = beat->dft_values[(analysis->sample_index + i) % analysis->num_beat_samples];
        dft_values[i] = old_value;
        beat->short_sum += old_value;
        beat->short_square_sum += old_value * old_value;
    }

    // Fill potential extension with average built from copied portion of values.
    float avg = beat->short_sum / (float)num_values_to_copy;
    FORI(num_values_to_copy, new_num_samples) {
        dft_values[i] = avg;
        beat->short_sum += avg;
        beat->short_square_sum += avg * avg;
    }

    free(beat->dft_values);
    beat->dft_values = dft_values;
}

void reinitialize_beat_analysis(Analysis analysis, int new_num_samples) {
    FORI(0, analysis->num_beat_frequencies) { reinitialize_beat_frequency(analysis, i, new_num_samples); }

    analysis->num_beat_samples = new_num_samples;
    analysis->sample_index = 0; // Index unwrolled to 0.
}

Analysis create_analysis(Pcm pcm, DftData dft_data, unsigned int index) {
    Analysis analysis = ALLOCATE(1, struct Analysis_);

    // Core data.
    analysis->sample_rate = sample_rate_of_pcm(pcm);
    analysis->dft_size = size_of_dft(dft_data);

    // Band borders.
    set_band_border_indices(analysis);

    // Beat detection.
    initialize_beat_detection(analysis);

    // CPU side buffer data.
    analysis->data.is_beat = false;
    analysis->data.beats = 0;
    analysis->data.bpm = 0;
    analysis->data.other = 0;

    FORI(0, 7) {
        analysis->data.bands[i].accumulated = 0.0f;
        analysis->data.bands[i].window = 0.0f;
        analysis->data.bands[i].smooth_window = 0.0f;
        analysis->data.bands[i].delta2 = 0.0f;
        analysis->data.bands[i].movement = 0.0f;
    }

    // GPU buffer.
    analysis->gpu_buffer_size = isizeof(analysis->data);
    analysis->buffer = create_storage_buffer(analysis->gpu_buffer_size, index);

    return analysis;
}

void analyze_band(DftData dft_data, Analysis analysis, int band_index) {
    struct BandData* band = analysis->data.bands + band_index;

    float window = 0.0f;
    FORI(analysis->frequency_indices[band_index], analysis->frequency_indices[band_index + 1]) {
        window += dft_at(dft_data, i);
    }
    /* if (band == 1) { */
    /*     printf("\n"); */
    /* } */

    float delta2 = fabsf(band->window - window);
    // Smoothing?
    /* float avg_delta2 = mix(analysis->bands[band].avg_delta2, cur_delta2, 0.2f); */

    /* if (band == 1) { */
    /* printf("wind: %.2f \t last wind: %.2f \t avg delta: %.2f \t cur_delta: %.2f \n", (double)windowed,
     * (double)analysis->bands[band].windowed, (double)analysis->bands[band].avg_delta2, (double)cur_delta2); */
    /* } */

    band->window = window;
    band->smooth_window = mix(band->smooth_window, window, 0.02f);
    band->accumulated += window;

    band->delta2 = delta2;
    band->movement += delta2;
}

void analyze_bands(DftData dft_data, Analysis analysis) {
    FORI(0, 7) {
        analyze_band(dft_data, analysis, i);
    }
}

void analyze_beat(DftData dft_data, Analysis analysis, int beat_fequency_index) {
    struct BeatAnalysis* beat = analysis->beat_analysis + beat_fequency_index;

    // Adjust average TODO recompute entirely every X frames (it's not an int, it's a float!)?
    // Subtract value i'm going to replace.
    float prev_value = beat->dft_values[analysis->sample_index];
    beat->short_sum -= prev_value;
    beat->short_square_sum -= prev_value * prev_value;

    // Add the current value.
    float current_value = dft_at(dft_data, beat->dft_index);

    beat->dft_values[analysis->sample_index] = current_value;

    beat->long_moving_average = mix(beat->long_moving_average, current_value, 0.0001f);
    beat->short_sum += current_value;
    float short_average = beat->short_sum / (float)analysis->num_beat_samples;
    beat->short_square_sum += current_value * current_value;
    // Compute SD as `sqrt(average of squares - average squared)`.
    float average_of_squares = beat->short_square_sum / (float)analysis->num_beat_samples;
    beat->sd = sqrtf(average_of_squares - powf(short_average, 2));

    // Detect beat.
    bool is_not_noise = short_average > beat->noise_threshold_factor * beat->long_moving_average;
    bool is_over_sd = current_value > short_average + 2.4f * beat->sd;
    // Not so sure about this one... SD doesn't need per-frequency fine tuning... or at least less so.
    /* bool is_local_beat = current_value > beat->beat_threshold_factor * short_average; */
    beat->is_beat = is_not_noise && is_over_sd; // && is_local_beat;
}

void print_beat_analysis_debug(DftData dft_data, Analysis analysis) {
    printf("%d", analysis->data.beats);
    FORI(0, analysis->num_beat_frequencies) {
        struct BeatAnalysis* beat = analysis->beat_analysis + i;
        float avg = beat->short_sum / (float)analysis->num_beat_samples;
        /* hz idx noize avg thresh cur sd is_beat */
        printf(",%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%d", beat->hz, beat->dft_index,
               (double)(beat->noise_threshold_factor * beat->long_moving_average), (double)avg,
               (double)(beat->beat_threshold_factor * avg), (double)dft_at(dft_data, beat->dft_index),
               (double)(avg + 2.4f * beat->sd), beat->is_beat);
    }
    printf("\n");
}

void analyze_beats(DftData dft_data, Analysis analysis) {
    // Check and adjust timing.
    time_t this_time = clock();
    float const s_to_ms = 1000.0f;
    float delta_ms = s_to_ms * (float)(this_time - analysis->last_time) / CLOCKS_PER_SEC;
    analysis->last_time = this_time;

    // Adjust the moving average cycle time: delta.
    float old_avg_process_time = analysis->moving_average_process_time_ms;
    analysis->moving_average_process_time_ms = mix(old_avg_process_time, delta_ms, 0.01f);

    // Check that the timing is still right.
    int old_num_samples = analysis->num_beat_samples;
    int new_num_samples = (int)(analysis->short_average_window_ms / analysis->moving_average_process_time_ms);

    float difference_percent = fabsf((float)(new_num_samples - old_num_samples)) / (float)(old_num_samples);
    if(difference_percent > 0.15f) {
        printf("Too much beat buffer size difference:\n");
        printf("\tPrev delta %.2f\tCurrent delta %.2f\tNew delta %.2f\n", (double)old_avg_process_time,
               (double)delta_ms, (double)analysis->moving_average_process_time_ms);
        printf("\tPrev #samples %d\tNew #samples %d\n", old_num_samples, new_num_samples);

        reinitialize_beat_analysis(analysis, new_num_samples);
    }

    // Merge per-frequency results.
    bool is_beat = false;

    FORI(0, analysis->num_beat_frequencies) {
        analyze_beat(dft_data, analysis, i);
        is_beat |= analysis->beat_analysis[i].is_beat;
    }

    // Move the index along the ringbuffer.
    analysis->sample_index = (analysis->sample_index + 1) % analysis->num_beat_samples;

    if(is_beat) {
        analysis->data.beats += analysis->data.is_beat ? 0 : 1;
    }

    /* print_beat_analysis_debug(analysis); */

    analysis->data.is_beat = is_beat;
}

void compute_and_copy_analysis_to_gpu(DftData dft_data, Analysis analysis) {
    analyze_bands(dft_data, analysis);
    analyze_beats(dft_data, analysis);
    copy_buffer_to_gpu(analysis->buffer, (char*)&analysis->data, 0, analysis->gpu_buffer_size);
}

void delete_analysis(Analysis analysis) {
    FORI(0, analysis->num_beat_frequencies) { free(analysis->beat_analysis[i].dft_values); }
    free(analysis->beat_analysis);
    delete_buffer(analysis->buffer);
    free(analysis);
}
