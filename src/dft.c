#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include <fftw3.h>

#include "buffers.h"
#include "globals.h"
#include "dft.h"
#include "pcm.h"

struct DftData_ {
    int size;
    float* hamming_window;
    float* in;
    float* out;
    fftwf_plan plan;

    Buffer buffer;
};

DftData create_dft_data(int dft_size, unsigned int index) {
    assert(dft_size % 2 == 0);
    DftData dft_data = (DftData)malloc(sizeof(struct DftData_));

    dft_data->size = dft_size;
    dft_data->hamming_window = malloc((size_t)dft_size * sizeof(float));
    dft_data->in = fftwf_malloc((size_t)dft_size * sizeof(float));
    dft_data->out = fftwf_malloc((size_t)dft_size * sizeof(fftwf_complex));
    dft_data->plan = fftwf_plan_r2r_1d(dft_size, dft_data->in, dft_data->out, FFTW_R2HC, 0);

    for(int i = 0; i < dft_data->size; i++) {
        dft_data->hamming_window[i] = 0.54f - (0.46f * cosf(2.f * PI * ((float)i / (float)(dft_data->size - 1))));
    }

    // The following doesn't even have to be a comment!
    // For reference see:
    // https://www.uni-weimar.de/fileadmin/user/fak/medien/professuren/Computer_Graphics/CG_WS_18_19/CG/06_ShaderBuffers.pdf

    int gpu_buffer_size = isizeof(int) + dft_size * isizeof(float);

    dft_data->buffer = create_storage_buffer(gpu_buffer_size, index);
    copy_buffer_to_gpu(dft_data->buffer, (char*)&dft_size, 0, sizeof(int));

    return dft_data;
}

__attribute__((pure)) int size_of_dft(DftData dft_data) { return dft_data->size; }

__attribute__((pure)) float dft_at(DftData dft_data, int index) {
    bool first_or_last = index == 0 || index == dft_data->size / 2;
    float second = first_or_last ? 0.0f : dft_data->out[dft_data->size - index];
    return sqrtf(powf(dft_data->out[index], 2.0) + powf(second, 2));
}

void compute_and_copy_dft_data_to_gpu(Pcm pcm, DftData dft_data) {
    copy_pcm_mono_to_buffer(dft_data->in, pcm, dft_data->size);

    // Multiply with Hamming window.
    for(int i = 0; i < dft_data->size; i++) {
        dft_data->in[i] *= dft_data->hamming_window[i];
    }
    fftwf_execute(dft_data->plan);

    int buffer_size = dft_data->size * isizeof(float);
    copy_buffer_to_gpu(dft_data->buffer, (char*)dft_data->out, sizeof(int), buffer_size);
}

void delete_dft_data(DftData dft_data) {
    delete_buffer(dft_data->buffer);

    fftwf_destroy_plan(dft_data->plan);
    fftwf_free(dft_data->in);
    fftwf_free(dft_data->out);
    free(dft_data->hamming_window);
    free(dft_data);
}
