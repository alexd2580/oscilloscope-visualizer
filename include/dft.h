#ifndef INCLUDE_DFT_H
#define INCLUDE_DFT_H

#include "pcm.h"

struct DftData_;
typedef struct DftData_* DftData;

DftData create_dft_data(int dft_size, unsigned int index);
void compute_and_copy_dft_data_to_gpu(Pcm pcm, DftData dft_data);
void delete_dft_data(DftData dft_data);

#endif
