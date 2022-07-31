#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <fftw3.h>

#include "buffers.h"
#include "defines.h"
#include "dft.h"
#include "peak_analysis.h"

struct PeakAnalysis_ {
    float* peaks;

    Buffer buffer;
};

PeakAnalysis create_peak_analysis(int dft_size, unsigned int index) {
    PeakAnalysis peak_analysis = (PeakAnalysis)malloc(sizeof(struct PeakAnalysis_));

    peak_analysis->peaks = malloc((size_t)dft_size * sizeof(float));

    int gpu_buffer_size = dft_size * isizeof(float);
    peak_analysis->buffer = create_storage_buffer(gpu_buffer_size, index);

    return peak_analysis;
}

void compute_and_copy_peak_analysis_to_gpu(DftData dft_data, PeakAnalysis peak_analysis) {
    float last_value = 0;
    bool going_up = true;
    int num_frequencies = dft_size(dft_data);

    // Iterate through all frequencies.
    for(int index = 0; index < num_frequencies; index++) {
        float current_value = dft_at(dft_data, index);

        // Currently we're going ...
        if(current_value > last_value) { // ... up.
            going_up = true;
        } else { // ... down.
            if(going_up) {
                // We were previously going up,
                // so the previous index was a positive peak.
                float min_value = 999999.0;
                bool all_less = true;
                for (int j= index - 6; j < index + 5; j++) {
                    float index_value = dft_at(dft_data, j);
                    min_value = MIN(min_value, index_value);
                    if (j != index - 1 && index_value > last_value) {
                        all_less = false;
                        break;
                    }
                }
                if (all_less) {
                    peak_analysis->peaks[index - 1] += (int)(last_value - min_value);
                }
            }
            going_up = false;
        }
        last_value = current_value;
    }

    int buffer_size = num_frequencies * isizeof(float);
    copy_buffer_to_gpu(peak_analysis->buffer, (char*)peak_analysis->peaks, 0, buffer_size);
}

void delete_peak_analysis(PeakAnalysis peak_analysis) {
    free(peak_analysis->peaks);
    free(peak_analysis);
}
