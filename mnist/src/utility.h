#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <stdbool.h>

float randfloat();
void clear_matrix(float* input, int size);
void copy_matrix(float* input, float* output, int size);
int arg_max(float* input, int size, int increment);
int arg_min(float* input, int size, int increment);
void get_weights_dims_layer(layer_t* layers,
                            int l,
                            int* num_rows,
                            int* num_cols);
int get_num_weights_layer(layer_t* layers, int l);
int get_total_num_weights(layer_t* layers, int num_layers);
bool is_dummy_layer(layer_t* layers, int l);

#ifdef BITWIDTH_REDUCTION
// Don't add this function unless we want to model bit width quantization
// effects. In particular, do not enable this if we are building a trace.  We
// don't want to add in the cost of dynamically doing this operation - we
// assume the data is already in the specified reduced precision and all
// functional units are designed to handle that bitwidth. So just make this
// function go away.
float conv_float2fixed(float input);
#else
#define conv_float2fixed(X) X
#endif

#endif
