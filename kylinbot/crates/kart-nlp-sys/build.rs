//! Build script for kart-nlp-sys.
//!
//! Generates Rust bindings from the SDK headers using bindgen.
//! Headers are expected to be installed at /usr/include/kylin-ai/

use std::env;
use std::path::PathBuf;

fn main() {
    // Skip bindgen in stub mode
    if env::var("CARGO_FEATURE_STUB").is_ok() {
        println!("cargo:warning=kart-nlp-sys: stub mode enabled, skipping bindgen");
        return;
    }

    let wrapper_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("wrapper.h");

    // Check if wrapper.h exists
    if !wrapper_path.exists() {
        println!("cargo:warning=kart-nlp-sys: wrapper.h not found, skipping bindgen");
        return;
    }

    println!("cargo:rerun-if-changed=wrapper.h");

    // Link to system library
    println!("cargo:rustc-link-lib=dylib=kysdk-genai-nlp");

    // Generate bindings using bindgen
    // Use system headers from /usr/include/kylin-ai/
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg("-I/usr/include/kylin-ai")  // For <genai/text/...> and <common/...>
        .clang_arg("-std=c11")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Allowlist SDK functions
        .allowlist_function("genai_text_.*")
        .allowlist_function("chat_message_.*")
        .allowlist_function("chat_model_config_.*")
        .allowlist_function("chat_result_.*")
        .allowlist_function("text_model_list_.*")
        // Allowlist SDK types
        .allowlist_type("GenAiTextSession")
        .allowlist_type("ChatMessage")
        .allowlist_type("ChatModelConfig")
        .allowlist_type("ChatResult")
        .allowlist_type("ChatResultCallback")
        .allowlist_type("GenAiTextInitCallback")
        .allowlist_type("PromptId")
        .allowlist_type("TextModelList")
        .allowlist_type("TextModelInfo")
        .allowlist_type("ChatResultSearchInfo")
        .allowlist_type("ChatResultSearchItem")
        .allowlist_type("ModelDeployType")
        // Treat opaque types as opaque (they are forward-declared in headers)
        .opaque_type("GenAiTextSession")
        .opaque_type("ChatMessage")
        .opaque_type("ChatModelConfig")
        .opaque_type("ChatResult")
        .opaque_type("TextModelList")
        .opaque_type("TextModelInfo")
        .opaque_type("ChatResultSearchInfo")
        .opaque_type("ChatResultSearchItem")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate kart-nlp bindings");

    // Write bindings to file
    let out_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("src/bindings.rs"))
        .expect("Could not write bindings");
}
