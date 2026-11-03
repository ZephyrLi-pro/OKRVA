# Stub SDK configuration
# Usage: qmake CONFIG+=use_sdk_stubs
# Provides stub implementations of:
#   libkyai-config-dev        (aiconfig/modelconfig.h)
#   libkyai-assistant-dev     (kylin-ai/private/osassistant/osassistant.h)
#   libkysdk-coreai-speech-dev (kylin-ai/coreai/speech/*)
#   libkysdk-ai-common-dev    (common/error.h, kylin-ai/common/*)
#   libkysdk-genai-nlp-dev    (kylin-ai/genai/text/*, genai/text/*)
#   libkysdk-advanced-ai-dev  (kylin-ai/advai/knowledgebase/*)

message("** Building with SDK stubs - for testing/development only **")

INCLUDEPATH += $$PWD/include

SOURCES += \
    $$PWD/src/speech_stub.cpp \
    $$PWD/src/genai_stub.cpp \
    $$PWD/src/kb_stub.cpp
