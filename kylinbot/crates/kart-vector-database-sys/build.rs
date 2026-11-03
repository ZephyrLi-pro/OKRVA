//! Build script for kart-vector-database-sys.
//!
//! Generates Rust bindings from the SDK's C ABI headers using bindgen.
//! Headers are expected at:
//!   - /usr/include/kysdk-vector-engine-client/   (vectordataengine.h, c/*.h)
//!   - /usr/include/kylin-ai/                     (common/macros.h)
//! installed via the `libkysdk-vector-engine-client-dev` debian package. Build
//! will fail without the headers unless the `stub` feature is enabled.

use std::env;
use std::path::PathBuf;

fn main() {
    // Compute the Debian multiarch system library path from the build target.
    // Must run before the stub-mode early return so the env var is always set.
    set_system_lib_path();

    // Skip bindgen entirely in stub mode so the crate still compiles on hosts
    // without the SDK headers (or `libclang`) installed.
    if env::var("CARGO_FEATURE_STUB").is_ok() {
        println!("cargo:warning=kart-vector-database-sys: stub mode enabled, skipping bindgen");
        return;
    }

    let wrapper_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("wrapper.h");

    if !wrapper_path.exists() {
        println!("cargo:warning=kart-vector-database-sys: wrapper.h not found, skipping bindgen");
        return;
    }

    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rustc-link-lib=dylib=kysdk-vector-engine-client");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        // vectordataengine.h lives here.
        .clang_arg("-I/usr/include/kysdk-vector-engine-client")
        // <common/macros.h> (the AISDK_EXTERN macro) lives here.
        .clang_arg("-I/usr/include/kylin-ai")
        .clang_arg("-std=c11")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Functions: every public symbol in vectordataengine.h + c/*.h is
        // prefixed `vector_data_engine_`.
        .allowlist_function("vector_data_engine_.*")
        // Opaque handle types declared via `typedef struct _Foo Foo;`.
        .allowlist_type("VectorDataEngine")
        .allowlist_type("CollectionSchema")
        .allowlist_type("FieldSchema")
        .allowlist_type("FunctionSchema")
        .allowlist_type("SearchArguments")
        .allowlist_type("AnnSearchRequest")
        .allowlist_type("RRFRanker")
        .allowlist_type("WeightedRanker")
        .allowlist_type("SearchResults")
        .allowlist_type("QueryArguments")
        .allowlist_type("QueryResults")
        .allowlist_type(".*FieldData")
        // Force opaque so bindgen does not try to lay out the private structs.
        .opaque_type("VectorDataEngine")
        .opaque_type("CollectionSchema")
        .opaque_type("FieldSchema")
        .opaque_type("FunctionSchema")
        .opaque_type("SearchArguments")
        .opaque_type("AnnSearchRequest")
        .opaque_type("RRFRanker")
        .opaque_type("WeightedRanker")
        .opaque_type("SearchResults")
        .opaque_type("QueryArguments")
        .opaque_type("QueryResults")
        .opaque_type(".*FieldData")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate kart-vector-database bindings");

    let out_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("src/bindings.rs"))
        .expect("Could not write bindings");
}

/// Derive the Debian multiarch lib directory from the Cargo build target triple
/// and expose it as a compile-time constant via `cargo:rustc-env`.
///
/// Rust triple:  `{arch}-{vendor}-{os}-{env}`  e.g. `x86_64-unknown-linux-gnu`
/// Debian triple: `{arch}-{os}-{env}`           e.g. `x86_64-linux-gnu`
///
/// Non-Linux targets get an empty string (no multiarch path exists).
fn set_system_lib_path() {
    let target = env::var("TARGET").unwrap_or_default();
    if !target.contains("linux") {
        println!("cargo:rustc-env=KART_VECTOR_DB_SYSTEM_LIB=");
        return;
    }
    let parts: Vec<&str> = target.split('-').collect();
    let debian_triple = if parts.len() >= 4 {
        format!("{}-{}-{}", parts[0], parts[2], parts[3])
    } else {
        target
    };
    println!(
        "cargo:rustc-env=KART_VECTOR_DB_SYSTEM_LIB=/usr/lib/{debian_triple}/libkysdk-vector-engine-client.so"
    );
}
