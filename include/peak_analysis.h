#ifndef INCLUDE_PEAK_ANALYSIS_H
#define INCLUDE_PEAK_ANALYSIS_H

#include "dft.h"

struct PeakAnalysis_;
typedef struct PeakAnalysis_* PeakAnalysis;

PeakAnalysis create_peak_analysis(int dft_size, unsigned int index);
void compute_and_copy_peak_analysis_to_gpu(DftData dft_data, PeakAnalysis peak_analysis);
void delete_peak_analysis(PeakAnalysis peak_analysis);

#endif
