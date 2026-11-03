//! Build script for kart-embedding-sys.
//!
//! Generates Rust bindings from the SDK headers using bindgen.
//! Headers are expected to be installed at /usr/include/kylin-ai/ via the
//! `libkylin-coreai-embedding-dev` debian package; build will fail without it
//! unless the `stub` feature is enabled.

use std::env;
use std::path::PathBuf;

fn main() {
    // Compute the Debian multiarch system library path from the build target.
    // Must run before the stub-mode early return so the env var is always set.
    set_system_lib_path();

    // Skip bindgen entirely in stub mode so the crate still compiles on
    // hosts without the SDK headers (or `libclang`) installed.
    if env::var("CARGO_FEATURE_STUB").is_ok() {
        println!("cargo:warning=kart-embedding-sys: stub mode enabled, skipping bindgen");
        return;
    }

    let wrapper_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("wrapper.h");

    if !wrapper_path.exists() {
        println!("cargo:warning=kart-embedding-sys: wrapper.h not found, skipping bindgen");
        return;
    }

    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rustc-link-lib=dylib=kysdk-coreai-embedding");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg("-I/usr/include/kylin-ai")
        .clang_arg("-std=c11")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Text-embedding surface only — image embedding is intentionally
        // excluded from PR A. Use explicit allowlist (not regex wildcards) to
        // avoid pulling in `text_embedding_by_image_model*`,
        // `text_embedding_async`, or deprecated `text_embedding_get_model_info`.
        .allowlist_function("text_embedding_create_session")
        .allowlist_function("text_embedding_destroy_session")
        .allowlist_function("text_embedding_init_session")
        .allowlist_function("text_embedding_init_model")
        .allowlist_function("text_embedding_get_model_list")
        // The core sync embed call — its bare name (no underscore suffix) is
        // intentionally included; the regex `text_embedding_.*` would miss it.
        .allowlist_function("text_embedding$")
        .allowlist_function("embedding_result_.*")
        .allowlist_function("embedding_model_list_.*")
        .allowlist_function("embedding_model_info_.*")
        // Types
        .allowlist_type("TextEmbeddingSession")
        .allowlist_type("EmbeddingResult")
        .allowlist_type("EmbeddingModelList")
        .allowlist_type("_EmbeddingModelInfo")
        .allowlist_type("EmbeddingModelInfo")
        .allowlist_type("TextEmbeddingResultCallback")
        .allowlist_type("CoreAiEmbeddingErrorCode")
        // Opaque forward-declared structs
        .opaque_type("TextEmbeddingSession")
        .opaque_type("EmbeddingResult")
        .opaque_type("EmbeddingModelList")
        .opaque_type("EmbeddingModelInfo")
        .opaque_type("_EmbeddingModelInfo")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate kart-embedding bindings");

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
        println!("cargo:rustc-env=KART_EMBEDDING_SYSTEM_LIB=");
        return;
    }
    let parts: Vec<&str> = target.split('-').collect();
    let debian_triple = if parts.len() >= 4 {
        format!("{}-{}-{}", parts[0], parts[2], parts[3])
    } else {
        target
    };
    println!(
        "cargo:rustc-env=KART_EMBEDDING_SYSTEM_LIB=/usr/lib/{debian_triple}/libkysdk-coreai-embedding.so"
    );
}
