#ifndef _OPERATORS_SMV_TILING_COMMON_H_
#define _OPERATORS_SMV_TILING_COMMON_H_

#include "core/tensor.h"

namespace smaug {
namespace smv {

enum TilingDims {
    None,
    DimN,
    DimNC,
    DimNH,
    DimNCH,
    Invalid
};

struct TilingConfig {
  public:
    TilingConfig() {}

    int getTotalSize() const {
        return inputs.storageSize() + weights.storageSize() +
               outputs.storageSize();
    }

    TensorShape inputs;
    TensorShape weights;
    TensorShape outputs;
    TilingDims inputTilingDims;
    TilingDims weightTilingDims;
    TilingDims outputTilingDims;
};

std::ostream& operator<<(std::ostream& os, const TilingDims& dims);

bool needsNwiseTiling(TilingDims dim);

bool needsCwiseTiling(TilingDims dim);

bool needsHwiseTiling(TilingDims dim);

}  // namespace smv
}  // namespace smaug

#endif
