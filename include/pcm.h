#ifndef INCLUDE_PCM_H
#define INCLUDE_PCM_H

struct Pcm_;
typedef struct Pcm_* Pcm;

Pcm create_pcm(int num_samples, unsigned int index);
__attribute__((const)) int sample_rate_of_pcm(Pcm pcm);
void copy_pcm_to_gpu(Pcm pcm);
void copy_pcm_mono_to_buffer(float* dst, Pcm pcm, int num_floats);
void delete_pcm(Pcm pcm);

struct PcmStream_;
typedef struct PcmStream_* PcmStream;

PcmStream create_pcm_stream(Pcm pcm);
void delete_pcm_stream(PcmStream pcm_stream);

#endif
