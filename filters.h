#ifndef FILTERS_H
#define FILTERS_H

#include <stdint.h>
#include <stdlib.h>

// Number of taps used for FIR filters
#define NUM_FIR_TAPS 4

// Shift used for FIR averager coefficients (should be log base 2 NUM_FIR_TAPS)
#define FIR_AVERAGER_SHIFT 2

void buf_averager(const uint16_t* in_buf, uint16_t* out_buf, const uint16_t* edge_samples_buf, size_t num_taps, size_t num_copies, size_t shift);

#endif