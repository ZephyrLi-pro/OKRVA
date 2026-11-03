// wrapper.h — driven by bindgen
//
// Includes the public C ABI header for the Kylin vector-engine-client SDK.
// Headers are installed at /usr/include/kysdk-vector-engine-client/ via the
// `libkysdk-vector-engine-client-dev` debian package; build.rs adds that
// directory plus /usr/include/kylin-ai (for <common/macros.h>) to the include
// path.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <vectordataengine.h>
