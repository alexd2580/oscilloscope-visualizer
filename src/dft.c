#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include <fftw3.h>

#include "defines.h"
#include "buffers.h"
#include "dft.h"
#include "pcm.h"

struct DftData_ {
    int size;
    float* hamming_window;
    float* in;
    float* out;
    fftwf_plan plan;

    float* smoothed;

    Buffer buffer;
};

DftData create_dft_data(int dft_size, unsigned int index) {
    assert(dft_size % 2 == 0);
    DftData dft_data = (DftData)malloc(sizeof(struct DftData_));

    dft_data->size = dft_size;
    dft_data->hamming_window = malloc((size_t)dft_size * sizeof(float));
    dft_data->in = fftwf_malloc((size_t)dft_size * sizeof(float));
    dft_data->out = fftwf_malloc((size_t)dft_size * sizeof(fftwf_complex));
    dft_data->smoothed = malloc((size_t)dft_size * sizeof(fftwf_complex));
    dft_data->plan = fftwf_plan_r2r_1d(dft_size, dft_data->in, dft_data->out, FFTW_R2HC, 0);

    for(int i = 0; i < dft_data->size; i++) {
        dft_data->hamming_window[i] = 0.54f - (0.46f * cosf(2.f * PI * ((float)i / (float)(dft_data->size - 1))));
    }

    for(int i = 0; i < dft_size; i++) {
        dft_data->smoothed[i] = 0.0f;
    }

    // The following doesn't even have to be a comment!
    // For reference see:
    // https://www.uni-weimar.de/fileadmin/user/fak/medien/professuren/Computer_Graphics/CG_WS_18_19/CG/06_ShaderBuffers.pdf

    int gpu_buffer_size = 2 * isizeof(int) + 2 * dft_size * isizeof(float);

    dft_data->buffer = create_storage_buffer(gpu_buffer_size, index);
    copy_buffer_to_gpu(dft_data->buffer, (char*)&dft_size, 0, sizeof(int));

    return dft_data;
}

void compute_and_copy_dft_data_to_gpu(Pcm pcm, DftData dft_data) {
    copy_pcm_mono_to_buffer(dft_data->in, pcm, dft_data->size);

    // Multiply with Hamming window.
    for(int i = 0; i < dft_data->size; i++) {
        dft_data->in[i] *= dft_data->hamming_window[i];
    }
    fftwf_execute(dft_data->plan);

    float max_value = 0.0;
    int max_index = 0;

    for(int i = 0; i < dft_data->size / 2 + 1; i++) {
        float re = dft_data->out[i];
        float im = i > 0 && i < dft_data->size / 2 - 1 ? dft_data->out[i] : 0.0f;
        float amp_2 = re * re + im * im;
        if(amp_2 > max_value) {
            max_value = amp_2;
            max_index = i;
        }
    }

    int dominant_freq_period = dft_data->size / (max_index + 1);

    for(int i = 0; i < dft_data->size; i++) {
        dft_data->smoothed[i] = MAX(0.95f * dft_data->smoothed[i], dft_data->out[i]);
    }

    copy_buffer_to_gpu(dft_data->buffer, (char*)&dominant_freq_period, sizeof(int), sizeof(int));
    int buffer_offset = 2 * sizeof(int);
    int buffer_size = dft_data->size * isizeof(float);
    copy_buffer_to_gpu(dft_data->buffer, (char*)dft_data->out, buffer_offset, buffer_size);
    copy_buffer_to_gpu(dft_data->buffer, (char*)dft_data->smoothed, buffer_offset + buffer_size, buffer_size);
}

void delete_dft_data(DftData dft_data) {
    delete_buffer(dft_data->buffer);

    free(dft_data->smoothed);
    fftwf_destroy_plan(dft_data->plan);
    fftwf_free(dft_data->in);
    fftwf_free(dft_data->out);
    free(dft_data->hamming_window);
    free(dft_data);
}
