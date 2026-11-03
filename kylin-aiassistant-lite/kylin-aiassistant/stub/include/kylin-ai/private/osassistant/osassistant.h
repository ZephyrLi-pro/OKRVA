#pragma once

#include <functional>
#include <list>
#include <string>
#include <vector>

/**
 * OsAssistant stub for libkyai-assistant-dev
 * Provides no-op implementations for building without the real SDK.
 */
namespace kyai {
namespace assistant {

enum class ErrorCode {
    Ok = 0,
    NetError,
    ServerError,
    UnAuthorized,
    RequestTooMany,
    RequestFailed,
    InputTextLengthInvaild,
    TaskTimeout,
    GenerateImageFailed,
    GenerateImageBlocked,
    ParamError,
    InputTextInvaild,
    FileNotFound,
    IntentServerError,
    NlpServerError,
    VisionServerError,
    InitNotCompleted,
    InputTextBlocked,
    OutputTextBlocked,
    UnknownError,
    SystemOperationFailed,
    IntentionRecognitionFailed,
    NlpModelNotFound,
    VisionModelNotFound,
    FileExceedSizeLimit,
    FileParseFailed,
    CreateScheduleFailed,
    CreateToDoListFailed,
    FreeAllocatedQuotaExceeded,
    LocalNlpModelNotLoaded,
    SslCertificateError,
    ExtractImageContentsFailed,
    IntentNlpModelNotFound,
};

struct Error {
    ErrorCode code = ErrorCode::Ok;
    std::string message;

    Error() = default;
    explicit Error(ErrorCode c) : code(c) {}
};

enum class PromptContentAccessType {
    ReadOnly = 1,
    ReadWrite,
    NoAccess
};

struct Prompt {
    int id = 0;
    std::string name;
    std::string content;
    PromptContentAccessType type = PromptContentAccessType::ReadWrite;
    bool active = true;
};

struct Message {
    std::string role;
    std::string content;
    std::string user;
    std::string assistant;
};

class Features {
public:
    enum class Feature {
        DocumentQA = 0,
        ImageProcessing,
        WebSearch,
        DeepThinking,
    };

    Features(int = 0) {}
    Features(const Features &) = default;
    Features &operator=(const Features &) = default;

    bool isSupported(Feature) const { return false; }
    bool containsFeature(Feature) const { return false; }
};

class OsAssistant {
public:
    OsAssistant() = default;
    ~OsAssistant() = default;

    Error init() { return Error{ErrorCode::Ok}; }

    Error initWithChatHistory(std::vector<Message> /*messages*/) {
        return Error{ErrorCode::Ok};
    }

    void setChatAsyncCallback(std::function<void(const std::string)> /*cb*/) {}

    void chatAsync(const std::string & /*json*/) {}

    void stopChat() {}

    void clearContext() {}

    std::vector<Prompt> prompts() { return {}; }

    Features supportedFeatures() { return Features(0); }

    void setPromptId(int /*actorId*/) {}

    bool setPromptOrder(std::list<int> /*ids*/) { return true; }

    int createPrompt(std::string /*name*/, std::string /*content*/) { return -1; }

    bool updatePrompt(int /*actorId*/, std::string /*name*/, std::string /*content*/) {
        return false;
    }

    bool deletePrompt(int /*actorId*/) { return false; }

    std::string getConfiguredNlpModelInfos() { return ""; }

    void setCurrentNlpModel(const std::string & /*modelName*/) {}
};

} // namespace assistant
} // namespace kyai
