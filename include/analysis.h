#ifndef INCLUDE_ANALYSIS_H
#define INCLUDE_ANALYSIS_H

#include "dft.h"
#include "pcm.h"

struct Analysis_;
typedef struct Analysis_* Analysis;

Analysis create_analysis(Pcm pcm, DftData dft_data, unsigned int index);
void compute_and_copy_analysis_to_gpu(DftData dft_data, Analysis analysis);
void delete_analysis(Analysis analysis);

#endif
