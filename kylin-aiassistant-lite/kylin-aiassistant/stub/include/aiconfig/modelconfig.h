#pragma once

#include <string>

/**
 * AI model configuration types.
 * Stub for libkyai-config-dev
 */
namespace kyai {
namespace config {
namespace model {

enum class AiCapability {
    Speech = 0,
    Vision,
    NLP,
    ImageGeneration
};

enum class DeployType {
    PublicCloud = 0,
    PrivateCloud,
    Ondevice,
    OnDevice = Ondevice
};

class ModelConfig {
public:
    ModelConfig() = default;
    ~ModelConfig() = default;

    std::string getCurrentModelName(AiCapability /*cap*/, DeployType /*type*/) {
        return "";
    }

    int getModelSchedulingPriority(DeployType /*type*/) {
        return 0;
    }
};

} // namespace model
} // namespace config
} // namespace kyai
