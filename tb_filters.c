/*
 * Testbench for filters
 * 
 * Command format: tb_filters <input samples filename> <number of samples>
 * 
 * Input samples file:
 *  - Binary file of 16-bit samples in linear order
 *  - Number of samples expected is specified as argument 2
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "filters.h"

#define OUTPUT_BASE_FILENAME "out"
#define NUM_TESTS_POSSIBLE 1
#define MIN_SAMPLES NUM_FIR_TAPS

int main(int argc, char *argv[])
{
    const int num_tests = argc - 2;
    int num_samples;

    FILE *input_samples_file;
    FILE *output_samples_file;

    uint16_t *in_buf;    // Length of num_samples
    uint16_t *out_buf;   // Length of num_samples

    // Validate arguments
    if (argc < 2)
    {
        fprintf(stderr, "Error: Too few arguments");
        return 0;
    }

    if (isdigit(argv[2][0]))
    {
        num_samples = atoi(argv[2]);
    }
    else
    {
        fprintf(stderr, "Error: Invalid number of samples");
        return 0;
    }

    if (num_samples < MIN_SAMPLES)
    {
        fprintf(stderr, "Error: Invalid number of samples");
        return 0;
    }

    // Load file samples into buffer
    if (!(input_samples_file = fopen(argv[1], "rb")))
    {
        fprintf(stderr, "Error: Could not open input samples file");
        return 0;
    }

    in_buf = malloc(sizeof(uint16_t) * num_samples);
    out_buf = malloc(sizeof(uint16_t) * num_samples);

    size_t len = fread(in_buf, sizeof(uint16_t), num_samples, input_samples_file);

    if (len < num_samples)
    {
        fprintf(stderr, "Error: Error reading samples file");
        return 0;
    }

    fclose(input_samples_file);

    // Run tests
    for (int test_index = 0; test_index < NUM_TESTS_POSSIBLE; test_index++)
    {
        // Set up buffers
        uint16_t *edge_samples_buf = calloc(NUM_FIR_TAPS - 1, sizeof(uint16_t));

        // Run appropriate test
        switch (test_index) {
            case 0:     buf_averager(in_buf, out_buf, edge_samples_buf, NUM_FIR_TAPS, num_samples / NUM_FIR_TAPS, FIR_AVERAGER_SHIFT);
            default:    break;
        }

        // Open output file (format: "<OUTPUT_BASE_FILENAME><test_index>.bin")
        char output_filename[1024] = "";
        char str_test_index[8];

        itoa(test_index, str_test_index, 10);

        strcat(output_filename, OUTPUT_BASE_FILENAME);
        strcat(output_filename, str_test_index);
        strcat(output_filename, ".bin");

        if (!(output_samples_file = fopen(output_filename, "wb")))
        {
            fprintf(stderr, "Error: Could not open output samples file");
            return 0;
        }

        // Output samples to file
        fwrite(out_buf, sizeof(uint16_t), num_samples, output_samples_file);

        fclose(output_samples_file);
    }
}