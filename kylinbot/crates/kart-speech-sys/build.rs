//! Build script for kart-speech-sys.
//!
//! Generates Rust bindings from the SDK headers using bindgen.
//! Headers are expected to be installed at /usr/include/kylin-ai/

use std::env;
use std::path::PathBuf;

fn main() {
    // Skip bindgen in stub mode
    if env::var("CARGO_FEATURE_STUB").is_ok() {
        println!("cargo:warning=kart-speech-sys: stub mode enabled, skipping bindgen");
        return;
    }

    let wrapper_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("wrapper.h");

    // Check if wrapper.h exists
    if !wrapper_path.exists() {
        println!("cargo:warning=kart-speech-sys: wrapper.h not found, skipping bindgen");
        return;
    }

    println!("cargo:rerun-if-changed=wrapper.h");

    // Link to system library
    println!("cargo:rustc-link-lib=dylib=kysdk-coreai-speech");

    // Generate bindings using bindgen
    // Use system headers from /usr/include/kylin-ai/
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg("-I/usr/include/kylin-ai")  // For <coreai/speech/...> and <common/...>
        .clang_arg("-std=c11")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Allowlist SDK functions for synthesizer
        .allowlist_function("speech_synthesizer_.*")
        // Allowlist SDK functions for recognizer
        .allowlist_function("speech_recognizer_.*")
        // Allowlist SDK functions for config
        .allowlist_function("speech_model_config_.*")
        .allowlist_function("audio_config_.*")
        // Allowlist SDK functions for result
        .allowlist_function("speech_synthesis_result_.*")
        .allowlist_function("speech_recognition_result_.*")
        // Allowlist SDK types
        .allowlist_type("SpeechSynthesizerSession")
        .allowlist_type("SpeechRecognitionSession")
        .allowlist_type("SpeechModelConfig")
        .allowlist_type("AudioConfig")
        .allowlist_type("SpeechSynthesisResult")
        .allowlist_type("SpeechRecognitionResult")
        .allowlist_type("SpeechSynthesisResultCallback")
        .allowlist_type("SpeechRecognitionResultCallback")
        .allowlist_type("AudioDataStream")
        .allowlist_type("AudioDataStreamCallback")
        .allowlist_type("SpeechSynthesisReason")
        .allowlist_type("SpeechRecognitionReason")
        .allowlist_type("ModelDeployType")
        // Treat opaque types as opaque
        .opaque_type("SpeechSynthesizerSession")
        .opaque_type("SpeechRecognitionSession")
        .opaque_type("SpeechModelConfig")
        .opaque_type("AudioConfig")
        .opaque_type("SpeechSynthesisResult")
        .opaque_type("SpeechRecognitionResult")
        .opaque_type("AudioDataStream")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate kart-speech bindings");

    // Write bindings to file
    let out_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("src/bindings.rs"))
        .expect("Could not write bindings");
}