//! Build script for kart-vision-sys.
//!
//! Generates Rust bindings from the SDK headers using bindgen.
//! Headers are expected to be installed at /usr/include/kylin-ai/

use std::env;
use std::path::PathBuf;

fn main() {
    // Skip bindgen in stub mode
    if env::var("CARGO_FEATURE_STUB").is_ok() {
        println!("cargo:warning=kart-vision-sys: stub mode enabled, skipping bindgen");
        return;
    }

    let wrapper_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("wrapper.h");

    // Check if wrapper.h exists
    if !wrapper_path.exists() {
        println!("cargo:warning=kart-vision-sys: wrapper.h not found, skipping bindgen");
        return;
    }

    println!("cargo:rerun-if-changed=wrapper.h");

    // NOTE: We intentionally do NOT emit `cargo:rustc-link-lib=dylib=kysdk-genai-vision`.
    // The crate uses `libloading` for runtime dynamic loading with graceful fallback
    // when the SDK is not installed. A compile-time link would fail the build on
    // systems without the SDK, defeating the purpose of the runtime probe.

    // Generate bindings using bindgen
    // Use system headers from /usr/include/kylin-ai/
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg("-I/usr/include/kylin-ai")
        .clang_arg("-std=c11")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Allowlist image session functions
        .allowlist_function("genai_image_.*")
        // Allowlist image config functions
        .allowlist_function("image_config_.*")
        // Allowlist image result functions
        .allowlist_function("image_result_.*")
        // Allowlist types
        .allowlist_type("GenAiImageSession")
        .allowlist_type("ImageConfig")
        .allowlist_type("VisionImageResult")
        .allowlist_type("ImageSize")
        .allowlist_type("ImageFormat")
        .allowlist_function("VisionImageResultCallback")
        // Opaque types
        .opaque_type("GenAiImageSession")
        .opaque_type("ImageConfig")
        .opaque_type("VisionImageResult")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate kart-vision bindings");

    // Write bindings to file
    let out_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("src/bindings.rs"))
        .expect("Could not write bindings");
}