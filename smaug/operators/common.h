#ifndef _OPERATORS_COMMON_H_
#define _OPERATORS_COMMON_H_

#include <stdint.h>

#include "gem5/sampling_interface.h"

#ifdef DMA_MODE
#ifdef __cplusplus
extern "C" {
#endif
#include "gem5/dma_interface.h"
#ifdef __cplusplus
}
#endif
#include "gem5/aladdin_sys_connection.h"
#include "gem5/aladdin_sys_constants.h"
#include "gem5/systolic_array_connection.h"
#endif

#ifdef __cplusplus
// Functions for invoking kernels and mapping arrays.
//
// If gem5 simulation is not used, the pure software version of the accelerate
// kernels will be invoked.
//
// These functions should be called from C++ files and not be included in C
// files.

#include <string>
#include <utility>
#include <memory>
#include "smaug/core/globals.h"
#include "tracer/trace_logger_aladdin.h"

namespace smaug {

// Return the trace name.
std::string getTraceName(int accelIdx);

template <typename Kernel, typename... Args>
void invokeKernel(int accelIdx,
                  unsigned reqCode,
                  const Kernel& kernel,
                  Args&&... args) {
    if (runningInSimulation) {
        invokeAcceleratorAndBlock(reqCode);
    } else {
#ifdef TRACE_MODE
        llvmtracer_set_trace_name(getTraceName(accelIdx).c_str());
#endif
        kernel(std::forward<Args>(args)...);
    }
}

// The difference between this and the one above is the names of generated
// traces at TRACE mode. This one generates trace to default first accelerator
// (with "_acc0" suffix).
template <typename Kernel, typename... Args>
void invokeKernel(unsigned reqCode, const Kernel& kernel, Args&&... args) {
    invokeKernel(0, reqCode, kernel, std::forward<Args>(args)...);
}

template <typename Kernel, typename... Args>
std::unique_ptr<volatile int> invokeKernelNoBlock(int accelIdx,
                                                  unsigned reqCode,
                                                  const Kernel& kernel,
                                                  Args&&... args) {
    if (runningInSimulation) {
        return std::unique_ptr<volatile int>(
                invokeAcceleratorAndReturn(reqCode));
    } else {
#ifdef TRACE_MODE
        llvmtracer_set_trace_name(getTraceName(accelIdx).c_str());
#endif
        kernel(std::forward<Args>(args)...);
        return nullptr;
    }
}

void mapArrayToAccel(unsigned reqCode,
                     const char* arrayName,
                     void* baseAddr,
                     size_t size);

void setArrayMemTypeIfSimulating(unsigned reqCode,
                                 const char* arrayName,
                                 MemoryType memType);

}  // namespace smaug
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t next_multiple(size_t request, size_t align);

#ifdef __cplusplus
}
#endif

typedef enum _activation_type {
    NO_ACTIVATION,
    RELU,
    RELU_THRESHOLD,
    LRELU,
    ELU,
    SELU,
    TANH,
    HARD_TANH,
    SIGMOID,
    SOFTMAX
} activation_type;

typedef struct _activation_param_t {
    // LReLU
    float slope;
    // ELU/SELU
    float alpha;
    float lambda;
    // Hard Tanh
    float min;
    float max;
} activation_param_t;

#ifdef __cplusplus
struct ActivationInfo {
   public:
    ActivationInfo() : function(activation_type::NO_ACTIVATION) {}
    ActivationInfo(activation_type _function) : function(_function) {
        // Use default parameters if not specified.
        switch (_function) {
            case activation_type::LRELU:
                params.slope = 0.2;
                break;
            case activation_type::ELU:
                params.alpha = 0.1;
                break;
            case activation_type::SELU:
                params.alpha = 1.6733;
                params.lambda = 1.0507;
                break;
            case activation_type::HARD_TANH:
                params.min = -1;
                params.max = 1;
                break;
            default:
                break;
        }
    }
    ActivationInfo(activation_type _function, activation_param_t _params)
            : function(_function), params(_params) {}
    activation_type function;
    activation_param_t params;
};
#endif

typedef enum _SamplingLevel {
    NoSampling = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    VeryHigh = 4
} SamplingLevel;

typedef struct _SamplingInfo {
    SamplingLevel level;
    int num_sample_iterations;
} SamplingInfo;

// Scalar types.
typedef float fp_t;
typedef int sfx_t;
typedef unsigned ufx_t;
typedef uint16_t fp16_t;
typedef uint16_t float16;

#ifndef VECTOR_SIZE
#define VECTOR_SIZE 8
#endif

#define CACHELINE_SIZE 32
#define LOG_PAGE_SIZE 12

// 16 packed 32-bit floating point values.
typedef fp16_t v16fp_t
        __attribute__((__vector_size__(VECTOR_SIZE * 2 * sizeof(fp_t))));
// 8 packed 32-bit floating point values.
typedef fp_t v8fp_t
        __attribute__((__vector_size__(VECTOR_SIZE * sizeof(fp_t))));
// 4 packed 32-bit floating point values.
typedef fp_t v4fp_t
        __attribute__((__vector_size__(VECTOR_SIZE / 2 * sizeof(fp_t))));

// 16 packed 16-bit floating point values.
typedef fp16_t v16ph_t
        __attribute__((__vector_size__(VECTOR_SIZE * 2 * sizeof(fp16_t))));
// 8 packed 16-bit floating point values.
typedef fp16_t v8ph_t
        __attribute__((__vector_size__(VECTOR_SIZE * sizeof(fp16_t))));
// 4 packed 16-bit floating point values.
typedef fp16_t v4ph_t
        __attribute__((__vector_size__(VECTOR_SIZE / 2 * sizeof(fp16_t))));

// 8 packed 32-bit integer values.
typedef sfx_t v8sfx_t
        __attribute__((__vector_size__(VECTOR_SIZE * sizeof(sfx_t))));
typedef sfx_t v4sfx_t
        __attribute__((__vector_size__(VECTOR_SIZE / 2 * sizeof(sfx_t))));

// Use these convenience macros to cast a raw pointer into a multidimensional
// variable-length array, which lets us use [] notation inside of the ugly
// sub2ind syntax!
//
// Usage:
//   If we have an array like array[5][4]:
//      ARRAY_2D(TYPE, output_name, array, 4);
//
//   If we have an array like array[5][4][3]:
//      ARRAY_3D(TYPE, output_name, array, 4, 3);
//
//   If we have an array like array[5][4][3][2]
//      ARRAY_4D(TYPE, output_name, array, 4, 3, 2);
//
//   And so on...

#if defined(__clang__)

#define TO_TYPE(output_array_name, input_array_name)                           \
    output_array_name##_t output_array_name =                                  \
            (output_array_name##_t)(input_array_name)

#define ARRAY_1D(TYPE, output_array_name, input_array_name)                    \
    TYPE* output_array_name = (TYPE*)input_array_name

#define ARRAY_2D(TYPE, output_array_name, input_array_name, DIM_1)             \
    typedef TYPE(*output_array_name##_t)[DIM_1];                               \
    TO_TYPE(output_array_name, input_array_name)

#define ARRAY_3D(TYPE, output_array_name, input_array_name, DIM_1, DIM_2)      \
    typedef TYPE(*output_array_name##_t)[DIM_1][DIM_2];                        \
    TO_TYPE(output_array_name, input_array_name)

#define ARRAY_4D(                                                              \
        TYPE, output_array_name, input_array_name, DIM_1, DIM_2, DIM_3)        \
    typedef TYPE(*output_array_name##_t)[DIM_1][DIM_2][DIM_3];                 \
    TO_TYPE(output_array_name, input_array_name)

#define ARRAY_5D(                                                              \
        TYPE, output_array_name, input_array_name, DIM_1, DIM_2, DIM_3, DIM_4) \
    typedef TYPE(*output_array_name##_t)[DIM_1][DIM_2][DIM_3][DIM_4];          \
    TO_TYPE(output_array_name, input_array_name)

#define VEC_ARRAY_1D(TYPE, output_array_name, input_array_name)                \
    TYPE* output_array_name = (TYPE*)input_array_name

#define VEC_ARRAY_2D(TYPE, output_array_name, input_array_name, cols)          \
    typedef TYPE(*output_array_name##_t)[(cols) / VECTOR_SIZE];                \
    TO_TYPE(output_array_name, input_array_name)

#define VEC_ARRAY_3D(TYPE, output_array_name, input_array_name, rows, cols)    \
    typedef TYPE(*output_array_name##_t)[(rows)][(cols) / VECTOR_SIZE];        \
    TO_TYPE(output_array_name, input_array_name)

#define VEC_ARRAY_4D(                                                          \
        TYPE, output_array_name, input_array_name, height, rows, cols)         \
    typedef TYPE(                                                              \
            *output_array_name##_t)[(height)][(rows)][(cols) / VECTOR_SIZE];   \
    TO_TYPE(output_array_name, input_array_name)

#elif defined(__GNUC__)

#define ARRAY_1D(TYPE, output_array_name, input_array_name)                    \
    TYPE* output_array_name = (TYPE*)input_array_name

#define ARRAY_2D(TYPE, output_array_name, input_array_name, DIM_1)             \
    TYPE(*output_array_name)[DIM_1] = (TYPE(*)[DIM_1])input_array_name

#define ARRAY_3D(TYPE, output_array_name, input_array_name, DIM_1, DIM_2)      \
    TYPE(*output_array_name)[DIM_1][DIM_2] =                                   \
        (TYPE(*)[DIM_1][DIM_2])input_array_name

#define ARRAY_4D(                                                              \
    TYPE, output_array_name, input_array_name, DIM_1, DIM_2, DIM_3)            \
        TYPE(*output_array_name)[DIM_1][DIM_2][DIM_3] =                        \
            (TYPE(*)[DIM_1][DIM_2][DIM_3])input_array_name

#define ARRAY_5D(                                                              \
    TYPE, output_array_name, input_array_name, DIM_1, DIM_2, DIM_3, DIM_4)     \
        TYPE(*output_array_name)[DIM_1][DIM_2][DIM_3][DIM_4] =                 \
            (TYPE(*)[DIM_1][DIM_2][DIM_3][DIM_4])input_array_name

#define VEC_ARRAY_1D(TYPE, output_array_name, input_array_name)                \
    TYPE* output_array_name = (TYPE*)(input_array_name)

#define VEC_ARRAY_2D(TYPE, output_array_name, input_array_name, cols)          \
    TYPE(*output_array_name)                                                   \
    [(cols) / (VECTOR_SIZE)] =                                                 \
            (TYPE(*)[(cols) / (VECTOR_SIZE)]) input_array_name

#define VEC_ARRAY_3D(TYPE, output_array_name, input_array_name, rows, cols)    \
    TYPE(*output_array_name)                                                   \
    [(rows)][(cols) / (VECTOR_SIZE)] =                                         \
            (TYPE(*)[(rows)][(cols) / (VECTOR_SIZE)]) input_array_name

#define VEC_ARRAY_4D(                                                          \
        TYPE, output_array_name, input_array_name, height, rows, cols)         \
    TYPE(*output_array_name)                                                   \
    [(height)][(rows)][(cols) / (VECTOR_SIZE)] =                               \
            (TYPE(*)[(height)][(rows)][(cols) / (VECTOR_SIZE)])                \
                    input_array_name

#endif

// Apply a mask to a 256-bit packed single precision FP vector.
//
// The mask is a vector of either 0s or -1s (all 1s). Entries that are have a
// mask of 0 are zeroed out.
//
// LLVM is smart enough to turn this into a SELECT instruction, rather than a
// bitwise mask!
//
// Args:
//    input: a v8fp_t vector
//    mask: a v8sfx_t vector of either 0s or -1s.
#define VEC256_MASK(input, mask) ((v8fp_t)((v8sfx_t)input & mask))

// Same as above, but for 128-bit vectors.
#define VEC128_MASK(input, mask) ((v4fp_t)((v4sfx_t)input & mask))

// Macros for computing the maximum of a group of elements.
//
// Why macros and not functions (or a loop)? A loop takes O(n) cycles to
// compute the maximum, when it could be done in O(log n) time with a tree
// based implementation. But Aladdin regards function calls as a hard
// dependency that it does not optimize across, so we would not get the
// parallelism we expect from the tree. Thus, these must be macros.
//
// I've only implemented a few of these. These are only meant for the pooling
// layers, and we shouldn't need more than a 3x3 pooling layer anyways.
#define max2(A, B) (((A) > (B)) ? (A) : (B))
#define max3(e0, e1, e2) max2(max2(e0, e1), e2)
#define max4(e0, e1, e2, e3) max2(max2(e0, e1), max2(e2, e3))
#define max8(e0, e1, e2, e3, e4, e5, e6, e7)                                   \
    max2(max4(e0, e1, e2, e3), max4(e4, e5, e6, e7))
#define max9(e0, e1, e2, e3, e4, e5, e6, e7, e8)                               \
    max2(max8(e0, e1, e2, e3, e4, e5, e6, e7), e8)
#define min2(A, B) (((A) < (B)) ? (A) : (B))

#define FRAC_CEIL(A, B) ((A) / (B) + ((A) % (B) != 0))

// Compiler-specific features.
//
// ALWAYS_INLINE:
// We have to disable all function inlining at the global level for Aladdin +
// LLVM-Tracer to work, but sometimes we do want to force inline functions
// (otherwise we run into all the issues of function call barriers in Aladdin).
// Add ALWAYS_INLINE before the function declaration to force inlining on this
// function.  Don't do this except when we're tracing though; usually it is not
// necessary and it generates a lot of compiler warnings.
//
// ASSERT:
// Disable asserts within instrumented when tracing.
//
// ASSUME_ALIGNED:
// Tell the compiler to assume a pointer is aligned on some byte boundary. This
// is not supported in clang 3.4.
#ifdef TRACE_MODE
#define ALWAYS_INLINE __attribute__((__always_inline__))
#define ASSERT(x)
#define ASSUME_ALIGNED(ptr, alignment) (ptr)
#else
#define ALWAYS_INLINE
#define ASSERT(x) assert(x)
#define ASSUME_ALIGNED(ptr, args...) __builtin_assume_aligned((ptr), args)
#endif

#define MAYBE_UNUSED __attribute__((__unused__))

#endif