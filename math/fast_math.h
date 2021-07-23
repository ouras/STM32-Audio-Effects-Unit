#ifndef FAST_MATH_H
#define FAST_MATH_H

#include "bam.h"

int16_t bam_sin(bam_t x);   // Returns sin(x in BAM) as int16_t QA
int16_t q_tanh(int16_t x);  // Returns tanh(x in QA) as int16_t QA
//int16_t q_exp(int16_t x);   // Returns exp(x in QA) as int16_t QA

#endif