#include "catch.hpp"
#include "core/backend.h"
#include "core/tensor.h"
#include "core/smaug_test.h"
#include "operators/smv/smv_test_common.h"
#include "operators/smv/smv_batch_norm_op.h"
#include "operators/smv/smv_batch_norm_tiling.h"

using namespace smaug;

Tensor* concatWeightTensors(SmvBatchNormOp* bnOp) {
    auto mean = bnOp->getInput(SmvBatchNormOp::Mean);
    auto variance = bnOp->getInput(SmvBatchNormOp::Variance);
    auto gamma = bnOp->getInput(SmvBatchNormOp::Gamma);
    auto beta = bnOp->getInput(SmvBatchNormOp::Beta);
    // Concatenate the four weight tensors into one.
    return concatTensors(
            { mean, variance, gamma, beta }, 0, bnOp->getWorkspace());
}

TEST_CASE_METHOD(SmaugTest, "Post-conv bn tiling", "[smvtiling]") {
    using namespace smaug::smv;
    using namespace smaug::smv::bn;
    auto bnOp = new SmvBatchNormOp("bn", workspace());
    std::vector<int> inputsHalos(4, 0);
    std::vector<int> weightsHalos(2, 0);
    std::vector<int> outputsHalos(4, 0);

    SECTION("No tiling needed") {
        TensorShape inputShape(
                { 1, 16, 32, 32 }, DataLayout::NCHW, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("inputs", inputShape);
        workspace()->addTensor(inputs);
        bnOp->setInput(inputs, 0);
        bnOp->createAllTensors();
        allocateAllTensors<float16>(bnOp);
        auto weights = concatWeightTensors(bnOp);
        auto outputs = bnOp->getOutput(0);
        TilingConfig config = TilingOptimizer::computeBasicTileShapes(
                inputs, weights, outputs);
        REQUIRE(config.inputs == inputShape);
        REQUIRE(config.weights.dims() == std::vector<int>{ 4, 16 });
        REQUIRE(config.outputs == inputShape);
    }

    SECTION("DimNC tiling") {
        TensorShape inputShape(
                { 1, 128, 16, 16 }, DataLayout::NCHW, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("inputs", inputShape);
        workspace()->addTensor(inputs);
        bnOp->setInput(inputs, 0);
        bnOp->createAllTensors();
        allocateAllTensors<float16>(bnOp);
        auto weights = concatWeightTensors(bnOp);
        auto outputs = bnOp->getOutput(0);
        TilingConfig config = TilingOptimizer::computeBasicTileShapes(
                inputs, weights, outputs);
        REQUIRE(config.inputs.dims() == std::vector<int>{ 1, 64, 16, 16 });
        REQUIRE(config.weights == weights->getShape());
        REQUIRE(config.outputs.dims() == std::vector<int>{ 1, 64, 16, 16 });

        SECTION("Generated tiles have correct shape and data") {
            fillTensorWithFixedData(inputs);
            TiledTensor inputsTiles = generateTiledTensor(
                    inputs, config.inputs, inputsHalos, workspace());
            REQUIRE(inputsTiles.size() == 2);
            for (auto i = inputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(inputsTiles[i]->getShape().dims() ==
                        config.inputs.dims());
                verifyTensorWithFixedData(inputsTiles[i], 0);
            }

            fillTensorWithFixedData(weights);
            TiledTensor weightsTiles = generateTiledTensor(
                    weights, config.weights, weightsHalos, workspace());
            REQUIRE(weightsTiles.size() == 1);
            REQUIRE(weightsTiles[0]->getShape().dims() ==
                    config.weights.dims());
            verifyTensorWithFixedData(weightsTiles[0], 0);

            fillTensorWithFixedData(outputs);
            TiledTensor outputsTiles = generateTiledTensor(
                    outputs, config.outputs, outputsHalos, workspace());
            REQUIRE(outputsTiles.size() == 2);
            for (auto i = outputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(outputsTiles[i]->getShape().dims() ==
                        config.outputs.dims());
                verifyTensorWithFixedData(outputsTiles[i], 0);
            }
        }
    }

    SECTION("DimNW tiling") {
        TensorShape inputShape(
                { 1, 32, 64, 64 }, DataLayout::NCHW, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("inputs", inputShape);
        workspace()->addTensor(inputs);
        bnOp->setInput(inputs, 0);
        bnOp->createAllTensors();
        allocateAllTensors<float16>(bnOp);
        auto weights = concatWeightTensors(bnOp);
        auto outputs = bnOp->getOutput(0);
        TilingConfig config = TilingOptimizer::computeBasicTileShapes(
                inputs, weights, outputs);
        REQUIRE(config.inputs.dims() == std::vector<int>{ 1, 32, 64, 8 });
        REQUIRE(config.weights == weights->getShape());
        REQUIRE(config.outputs.dims() == std::vector<int>{ 1, 32, 64, 8 });

        SECTION("Generated tiles have correct shape and data") {
            fillTensorWithFixedData(inputs);
            TiledTensor inputsTiles = generateTiledTensor(
                    inputs, config.inputs, inputsHalos, workspace());
            REQUIRE(inputsTiles.size() == 8);
            for (auto i = inputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(inputsTiles[i]->getShape().dims() ==
                        config.inputs.dims());
                verifyTensorWithFixedData(inputsTiles[i], 8 * i);
            }

            fillTensorWithFixedData(weights);
            TiledTensor weightsTiles = generateTiledTensor(
                    weights, config.weights, weightsHalos, workspace());
            REQUIRE(weightsTiles.size() == 1);
            REQUIRE(weightsTiles[0]->getShape().dims() ==
                    config.weights.dims());
            verifyTensorWithFixedData(weightsTiles[0], 0);

            fillTensorWithFixedData(outputs);
            TiledTensor outputsTiles = generateTiledTensor(
                    outputs, config.outputs, outputsHalos, workspace());
            REQUIRE(outputsTiles.size() == 8);
            for (auto i = outputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(outputsTiles[i]->getShape().dims() ==
                        config.outputs.dims());
                verifyTensorWithFixedData(outputsTiles[i], 8 * i);
            }
        }
    }

    SECTION("DimNCW tiling") {
        TensorShape inputShape(
                { 1, 64, 128, 128 }, DataLayout::NCHW, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("inputs", inputShape);
        workspace()->addTensor(inputs);
        bnOp->setInput(inputs, 0);
        bnOp->createAllTensors();
        allocateAllTensors<float16>(bnOp);
        auto weights = concatWeightTensors(bnOp);
        auto outputs = bnOp->getOutput(0);
        TilingConfig config = TilingOptimizer::computeBasicTileShapes(
                inputs, weights, outputs);
        REQUIRE(config.inputs.dims() == std::vector<int>{ 1, 8, 128, 16 });
        REQUIRE(config.weights == weights->getShape());
        REQUIRE(config.outputs.dims() == std::vector<int>{ 1, 8, 128, 16 });

        SECTION("Generated tiles have correct shape and data") {
            fillTensorWithFixedData(inputs);
            TiledTensor inputsTiles = generateTiledTensor(
                    inputs, config.inputs, inputsHalos, workspace());
            REQUIRE(inputsTiles.size() == 64);
            for (auto i = inputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(inputsTiles[i]->getShape().dims() ==
                        config.inputs.dims());
                verifyTensorWithFixedData(inputsTiles[i], (i % 8) * 16);
            }

            fillTensorWithFixedData(weights);
            TiledTensor weightsTiles = generateTiledTensor(
                    weights, config.weights, weightsHalos, workspace());
            REQUIRE(weightsTiles.size() == 1);
            REQUIRE(weightsTiles[0]->getShape().dims() ==
                    config.weights.dims());
            verifyTensorWithFixedData(weightsTiles[0], 0);

            fillTensorWithFixedData(outputs);
            TiledTensor outputsTiles = generateTiledTensor(
                    outputs, config.outputs, outputsHalos, workspace());
            REQUIRE(outputsTiles.size() == 64);
            for (auto i = outputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(outputsTiles[i]->getShape().dims() ==
                        config.outputs.dims());
                verifyTensorWithFixedData(outputsTiles[i], (i % 8) * 16);
            }
        }
    }
}

TEST_CASE_METHOD(SmaugTest, "Post-fc bn tiling", "[smvtiling]") {
    using namespace smaug::smv;
    using namespace smaug::smv::bn;
    auto bnOp = new SmvBatchNormOp("bn", workspace());
    std::vector<int> halos(2, 0);

    SECTION("No tiling needed") {
        TensorShape inputShape(
                { 1, 1024 }, DataLayout::NC, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("inputs", inputShape);
        workspace()->addTensor(inputs);
        bnOp->setInput(inputs, 0);
        bnOp->createAllTensors();
        allocateAllTensors<float16>(bnOp);
        auto weights = concatWeightTensors(bnOp);
        auto outputs = bnOp->getOutput(0);
        TilingConfig config = TilingOptimizer::computeBasicTileShapes(
                inputs, weights, outputs);
        REQUIRE(config.inputs == inputShape);
        REQUIRE(config.weights.dims() == std::vector<int>{ 4, 1024 });
        REQUIRE(config.outputs == inputShape);
    }

    SECTION("DimNC tiling") {
        TensorShape inputShape(
                { 1, 32768 }, DataLayout::NC, SmvBackend::Alignment);
        Tensor* inputs = new Tensor("inputs", inputShape);
        workspace()->addTensor(inputs);
        bnOp->setInput(inputs, 0);
        bnOp->createAllTensors();
        allocateAllTensors<float16>(bnOp);
        auto weights = concatWeightTensors(bnOp);
        auto outputs = bnOp->getOutput(0);
        TilingConfig config = TilingOptimizer::computeBasicTileShapes(
                inputs, weights, outputs);
        REQUIRE(config.inputs.dims() == std::vector<int>{ 1, 4096 });
        REQUIRE(config.weights.dims() == std::vector<int>{ 4, 4096 });
        REQUIRE(config.outputs.dims() == std::vector<int>{ 1, 4096 });

        SECTION("Generated tiles have correct shape and data") {
            fillTensorWithFixedData(inputs);
            TiledTensor inputsTiles = generateTiledTensor(
                    inputs, config.inputs, halos, workspace());
            REQUIRE(inputsTiles.size() == 8);
            for (auto i = inputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(inputsTiles[i]->getShape().dims() ==
                        config.inputs.dims());
                verifyTensorWithFixedData(inputsTiles[i], 4096 * i);
            }

            fillTensorWithFixedData(weights);
            TiledTensor weightsTiles = generateTiledTensor(
                    weights, config.weights, halos, workspace());
            REQUIRE(weightsTiles.size() == 8);
            for (auto i = weightsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(weightsTiles[i]->getShape().dims() ==
                        config.weights.dims());
                verifyTensorWithFixedData(weightsTiles[i], 4096 * i);
            }

            fillTensorWithFixedData(outputs);
            TiledTensor outputsTiles = generateTiledTensor(
                    outputs, config.outputs, halos, workspace());
            REQUIRE(outputsTiles.size() == 8);
            for (auto i = outputsTiles.startIndex(); !i.end(); ++i) {
                REQUIRE(outputsTiles[i]->getShape().dims() ==
                        config.outputs.dims());
                verifyTensorWithFixedData(outputsTiles[i], 4096 * i);
            }
        }
    }
}
