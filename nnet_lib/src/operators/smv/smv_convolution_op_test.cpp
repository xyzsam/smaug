#include "catch.hpp"
#include "core/backend.h"
#include "core/tensor.h"
#include "core/smaug_test.h"
#include "operators/smv/smv_test_common.h"
#include "operators/smv/smv_convolution_op.h"
#include "operators/smv/smv_convolution_tiling.h"

using namespace smaug;

Tensor* getReferenceOutput(SmvConvolutionOp* convOp, Workspace* workspace) {
    auto input = convOp->getInput(0);
    auto kernels = convOp->getInput(1);
    auto input32 = convertFp16ToFp32Tensor(input, workspace);
    auto kernels32 = convertFp16ToFp32Tensor(kernels, workspace);

    // A reference convolution operator is used to get the 'correct' output.
    auto refConvOp = new ConvolutionOp<ReferenceBackend>("ref_conv", workspace);
    refConvOp->setPadding(convOp->getPadding());
    refConvOp->setWeightDims(convOp->getWeightRows(), convOp->getWeightCols(),
                             convOp->getNumOfmaps());
    refConvOp->setStride(convOp->getRowStride(), convOp->getColStride());
    refConvOp->setInput(input32, 0);
    refConvOp->setInput(kernels32, 1);
    refConvOp->createAllTensors();
    refConvOp->getOutput(0)->allocateStorage<float>();
    refConvOp->run();
    return convertFp32ToFp16Tensor(refConvOp->getOutput(0), workspace);
}

TEST_CASE_METHOD(SmaugTest, "SMV Tiled Convolution", "[smvconv]") {
    auto convOp = new SmvConvolutionOp("conv", workspace());
    // Outputs should be the same size as inputs.
    convOp->setStride(1, 1);
    convOp->setPadding(SamePadding);

    SECTION("No tiling required") {
        TensorShape inputShape(
                { 1, 8, 8, 8 }, DataLayout::NHWC, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("input", inputShape);
        inputs->allocateStorage<float16>();
        workspace()->addTensor(inputs);
        convOp->setInput(inputs, 0);
        convOp->setWeightDims(3, 3, 8);
        SECTION("Same padding") {
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
        SECTION("Valid padding") {
            convOp->setPadding(ValidPadding);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
    }

    SECTION("DimN tiled convolution") {
        SECTION("Every weight tile contains 8 kernels") {
            TensorShape inputShape(
                    { 1, 8, 8, 192 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(3, 3, 128);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
        SECTION("Every weight tile contains more than 8 kernels") {
            TensorShape inputShape(
                    { 1, 8, 8, 32 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            SECTION("Every weight tile contains multiples of 8 kernels") {
                // The weight tiles will contain 56, 56 and 16 kernels
                // respectively.
                convOp->setWeightDims(3, 3, 128);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
            SECTION("Weight tile contains non-multiples of 8 kernels") {
                // The weight tiles will contain 50 kernels.
                convOp->setWeightDims(3, 3, 50);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
        }
    }

    SECTION("DimNH tiled convolution") {
        SECTION("Inputs DimNH tiled, No need to tile the weights") {
            TensorShape inputShape(
                    { 1, 32, 32, 32 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(3, 3, 8);
            SECTION("Same padding") {
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
            SECTION("Valid padding") {
                convOp->setPadding(ValidPadding);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
        }
        SECTION("Inputs DimNH tiled, weights DimN tiled") {
            TensorShape inputShape(
                    { 1, 32, 32, 32 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            SECTION("5x5 kernel size") {
                convOp->setWeightDims(5, 5, 128);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
            SECTION("2x2 kernel size") {
                convOp->setWeightDims(2, 2, 256);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
        }
        // The difference between this and the previous one is the tiling in the
        // weights due to the input channels.
        SECTION("Inputs DimNH tiled, weights DimNC tiled") {
            TensorShape inputShape({ 1, 64, 16, 256 }, DataLayout::NHWC,
                                   SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(4, 4, 128);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
    }

    SECTION("DimNC tiled convolution") {
        SECTION("Input tile and weight tile have the same channel dimension.") {
            SECTION("Both have 1 channelwise tile.") {
                TensorShape inputShape({ 1, 16, 8, 64 }, DataLayout::NHWC,
                                       SmvBackend::Alignment);
                Tensor* inputs = new Tensor("inputs", inputShape);
                workspace()->addTensor(inputs);
                convOp->setInput(inputs, 0);
                convOp->setWeightDims(5, 5, 8);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
            SECTION("Both have 4 channelwise tiles.") {
                TensorShape inputShape({ 1, 16, 16, 256 }, DataLayout::NHWC,
                                       SmvBackend::Alignment);
                Tensor* inputs = new Tensor("inputs", inputShape);
                workspace()->addTensor(inputs);
                convOp->setInput(inputs, 0);
                convOp->setWeightDims(5, 5, 8);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
        }
        SECTION("Inputs are not tiled channelwise, weights have 2 channelwise "
                "tiles") {
            TensorShape inputShape(
                    { 1, 8, 8, 256 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(3, 3, 8);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
        SECTION("Inputs are not tiled channelwise, weights have 3 channelwise "
                "tiles") {
            TensorShape inputShape(
                    { 1, 4, 4, 512 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(3, 3, 8);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
        SECTION("Inputs and weights don't need tiling, outputs need DimNC "
                "tiling") {
            TensorShape inputShape(
                    { 1, 32, 32, 8 }, DataLayout::NHWC, SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            SECTION("16 output tiles") {
                convOp->setWeightDims(1, 1, 256);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
            SECTION("8 output tiles") {
                convOp->setWeightDims(2, 2, 128);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
            SECTION("4 output tiles") {
                convOp->setWeightDims(3, 3, 64);
                createAndFillTensorsWithData<float16>(
                        convOp, fillTensorWithData);
                convOp->run();
                auto outputs = convOp->getOutput(0);
                auto refOutputs = getReferenceOutput(convOp, workspace());
                verifyOutputs<float16>(outputs, refOutputs);
            }
        }
    }

    SECTION("DimNCH tiled convolution") {
        SECTION("Inputs DimNCH tiling: 3 tiles rowwise, 6 tiles channelwise") {
            TensorShape inputShape({ 1, 32, 32, 192 }, DataLayout::NHWC,
                                   SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(4, 4, 32);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
        SECTION("Inputs DimNCH tiling: 9 tiles rowwise, 6 tiles channelwise") {
            TensorShape inputShape({ 1, 64, 64, 192 }, DataLayout::NHWC,
                                   SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(2, 2, 32);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
        SECTION("Inputs DimNCH tiling: 43 tiles rowwise, 6 tiles channelwise") {
            TensorShape inputShape({ 1, 128, 128, 192 }, DataLayout::NHWC,
                                   SmvBackend::Alignment);
            Tensor* inputs = new Tensor("inputs", inputShape);
            workspace()->addTensor(inputs);
            convOp->setInput(inputs, 0);
            convOp->setWeightDims(2, 2, 32);
            createAndFillTensorsWithData<float16>(convOp, fillTensorWithData);
            convOp->run();
            auto outputs = convOp->getOutput(0);
            auto refOutputs = getReferenceOutput(convOp, workspace());
            verifyOutputs<float16>(outputs, refOutputs);
        }
    }
}
