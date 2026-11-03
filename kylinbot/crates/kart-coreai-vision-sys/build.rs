use std::env;
use std::path::PathBuf;

fn main() {
    if env::var("CARGO_FEATURE_STUB").is_ok() {
        println!("cargo:warning=kart-coreai-vision-sys: stub mode enabled, skipping bindgen");
        return;
    }

    let wrapper_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()).join("wrapper.h");

    if !wrapper_path.exists() {
        println!("cargo:warning=kart-coreai-vision-sys: wrapper.h not found, skipping bindgen");
        return;
    }

    println!("cargo:rerun-if-changed=wrapper.h");

    // NOTE: We intentionally do NOT emit `cargo:rustc-link-lib=dylib=kysdk-coreai-vision`.
    // The crate uses `libloading` for runtime dynamic loading with graceful fallback
    // when the SDK is not installed.

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .clang_arg("-I/usr/include/kylin-ai")
        .clang_arg("-std=c11")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // OCR functions
        .allowlist_function("text_recognition_.*")
        .allowlist_function("text_line_.*")
        // Subject segmentation functions
        .allowlist_function("subject_segmentation_.*")
        // Types
        .allowlist_type("TextRecognitionSession")
        .allowlist_type("TextRecognitionResult")
        .allowlist_type("TextLine")
        .allowlist_type("TextRecognitionResultCallback")
        .allowlist_type("TextRecognitionModelConfig")
        .allowlist_type("SubjectSegmentationSession")
        .allowlist_type("SubjectSegmentationResult")
        .allowlist_type("SubjectSegmentationResultCallback")
        .allowlist_type("SubjectSegmentationInitCallback")
        .allowlist_type("SubjectSegmentationConfig")
        .allowlist_type("SubjectSegmentationMode")
        .allowlist_type("SubjectSegmentationPointGroup")
        .allowlist_type("PixelPoint")
        .allowlist_type("SegmentationMaskData")
        .allowlist_type("BoundingBox")
        // Opaque types
        .opaque_type("TextRecognitionSession")
        .opaque_type("TextRecognitionResult")
        .opaque_type("TextLine")
        .opaque_type("TextRecognitionModelConfig")
        .opaque_type("SubjectSegmentationSession")
        .opaque_type("SubjectSegmentationResult")
        .opaque_type("SubjectSegmentationConfig")
        .size_t_is_usize(true)
        .generate()
        .expect("Unable to generate kart-coreai-vision bindings");

    let out_path = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("src/bindings.rs"))
        .expect("Could not write bindings");
}
