// wrapper.h — driven by bindgen
//
// Includes the public headers we want bindings for. The SDK ships its headers
// under /usr/include/kylin-ai/ via the `libkylin-coreai-embedding-dev` debian
// package; build.rs adds that directory to the include path.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <coreai/embedding/embedding.h>
#include <coreai/embedding/error.h>
#include <coreai/embedding/modelinfo.h>
