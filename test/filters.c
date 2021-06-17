#include "filters.h"

// Buffered moving averager:
// 	- Averages over num_taps * num_copies number of samples (plus edge samples)
//	- Edge samples are those that go just before in_buf (num_taps - 1 in total), for ping-pong buffering
// 	- Passes linearly over combined buffer [edge_samples_buf:in_buf] in groups of num_taps
// 	- Each pass computes one output sample using num_taps number of input samples
//	- shift is the right shift amount of the output (division by log base 2 num_taps)
// 	- Assumes num_taps is a power of two (output get truncated otherwise, causing a loss of gain)
//	- Assumes in_buf is at least num_taps * num_copies long
void buf_averager(const uint16_t* in_buf, uint16_t* out_buf, const uint16_t* edge_samples_buf, size_t num_taps, size_t num_copies, size_t shift)
{
	const size_t edge_samples = num_taps - 1;
	const size_t in_buf_size = num_taps * num_copies;
	
	// Calculate each output sample
	// 	n is the index of the oldest sample in the current filter pass
	// 	Note: starts at n = -edge_samples, where n = 0 is the start of in_buf
	for (signed n = -edge_samples; n < (signed) in_buf_size - (signed) edge_samples; n++)
	{
		uint16_t out = 0;
		
		// Sum the num_taps number of samples starting from in_buf[i]
		// 	offset is number of unit delays current sample is ahead of n
		for (size_t offset = 0; offset < num_taps; offset++)
		{
			if (n + (signed) offset < 0)
			{
				out += edge_samples_buf[n + (signed) offset + (signed) edge_samples];
			}
			else
			{
				out += in_buf[n + offset];
			}
		}
		
		// Divide value by coefficient (by shifting)
		out_buf[n + edge_samples] = out >> shift;
	}
}
