//! Safe Rust bindings for libkysdk-genai-vision SDK.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};
use std::path::PathBuf;
use std::sync::{Arc, OnceLock};

use libloading::{Library, Symbol};
use thiserror::Error;
use tokio::sync::mpsc;

// Re-export types from bindgen
#[cfg(not(feature = "stub"))]
#[allow(nonstandard_style, unused)]
mod bindings;

// ImageFormat enum for safe abstraction
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ImageFormat {
    Unknown = -1,
    Jpeg = 0,
    Png = 1,
    Webp = 2,
}

impl ImageFormat {
    pub fn as_str(&self) -> &'static str {
        match self {
            ImageFormat::Unknown => "unknown",
            ImageFormat::Jpeg => "jpeg",
            ImageFormat::Png => "png",
            ImageFormat::Webp => "webp",
        }
    }

    pub fn from_raw(val: i32) -> Self {
        match val {
            0 => ImageFormat::Jpeg,
            1 => ImageFormat::Png,
            2 => ImageFormat::Webp,
            _ => ImageFormat::Unknown,
        }
    }
}

impl From<i32> for ImageFormat {
    fn from(val: i32) -> Self {
        Self::from_raw(val)
    }
}

#[cfg(not(feature = "stub"))]
pub use bindings::{
    GenAiImageSession, ImageConfig, ImageSize, VisionImageResult,
    ImageResultCallback,
};

// Stub types when feature = "stub" (for compilation without SDK headers)
#[cfg(feature = "stub")]
pub mod stub_types {
    use std::os::raw::c_void;

    pub type GenAiImageSession = *mut std::ffi::c_void;
    pub type ImageConfig = *mut std::ffi::c_void;
    pub type VisionImageResult = *mut std::ffi::c_void;
    pub type ImageResultCallback =
        Option<unsafe extern "C" fn(*mut VisionImageResult, *mut c_void)>;
}

#[cfg(feature = "stub")]
pub use stub_types::{
    GenAiImageSession, ImageConfig, VisionImageResult,
    ImageResultCallback,
};

// ImageSize from C header - use stub version for stub build, bindings version otherwise
#[cfg(feature = "stub")]
#[derive(Debug, Clone, Copy)]
pub struct ImageSize {
    pub width: i32,
    pub height: i32,
}

// ── Error Types ─────────────────────────────────────────────────────────────

/// Errors returned by kart-vision operations.
#[derive(Debug, Error)]
pub enum KartVisionError {
    /// SDK library not found or failed to load.
    #[error("libkysdk-genai-vision SDK not found — set KYSDK_GENAI_VISION_LIB_PATH or install the SDK")]
    LibraryNotFound,

    /// Session creation failed.
    #[error("Session creation failed")]
    SessionCreateFailed,

    /// Session initialization failed.
    #[error("Session initialization failed (code {0})")]
    SessionInitFailed(i32),

    /// Invalid input (e.g., null bytes in string).
    #[error("Invalid input: {0}")]
    InvalidInput(String),

    /// SDK returned an error code.
    #[error("SDK error (code {code}): {message}")]
    GenerationFailed { code: i32, message: String },

    /// Callback wait timed out.
    #[error("Callback wait timed out")]
    Timeout,

    /// Function not found in library.
    #[error("Function not found in library: {0}")]
    FunctionNotFound(String),

    /// Input text too long.
    #[error("Input text too long")]
    InputTextTooLong,

    /// Invalid style.
    #[error("Invalid style: {0}")]
    InvalidStyle(String),

    /// Invalid size.
    #[error("Invalid size")]
    InvalidSize,

    /// Invalid number.
    #[error("Invalid number")]
    InvalidNumber,

    /// Image generation blocked.
    #[error("Image generation blocked")]
    GenerationBlocked,
}

pub type Result<T> = std::result::Result<T, KartVisionError>;

// ── Output Types ─────────────────────────────────────────────────────────────

/// Output from an image generation operation.
#[derive(Debug, Clone)]
pub struct ImageOutput {
    /// Raw image data.
    pub data: Vec<u8>,
    /// Image format.
    pub format: ImageFormat,
    /// Image width in pixels.
    pub width: u32,
    /// Image height in pixels.
    pub height: u32,
}

/// Image generation configuration.
#[derive(Debug, Clone, Default)]
pub struct ImageGenConfig {
    /// Style (e.g., "水彩", "油画").
    pub style: Option<String>,
    /// Image size.
    pub size: Option<ImageSize>,
    /// Number of images to generate.
    pub number: Option<i32>,
}

// ── Callback Context ─────────────────────────────────────────────────────────

/// Context passed to the C callback for image generation results.
struct ImageCallbackContext {
    /// TX channel for image results to async world.
    tx: mpsc::UnboundedSender<Result<ImageOutput>>,
}

// ── Library Loading ──────────────────────────────────────────────────────────

static KYSDK_GENAI_VISION_LIB: OnceLock<Option<Library>> = OnceLock::new();

fn lib() -> Option<&'static Library> {
    KYSDK_GENAI_VISION_LIB
        .get_or_init(|| {
            #[cfg(target_arch = "x86_64")]
            const SYSTEM_LIB: &str = "/usr/lib/x86_64-linux-gnu/libkysdk-genai-vision.so";
            #[cfg(target_arch = "aarch64")]
            const SYSTEM_LIB: &str = "/usr/lib/aarch64-linux-gnu/libkysdk-genai-vision.so";
            #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
            const SYSTEM_LIB: &str = "";

            let candidates: Vec<PathBuf> = vec![
                // 1. Explicit env-var override (full path)
                std::env::var("KYSDK_GENAI_VISION_LIB_PATH")
                    .ok()
                    .map(PathBuf::from)
                    .unwrap_or_default(),
                // 2. System library — multiarch path for this build target
                PathBuf::from(SYSTEM_LIB),
                // 3. Next to the running binary (deployment)
                std::env::current_exe()
                    .ok()
                    .and_then(|e| e.parent().map(|d| d.join("libkysdk-genai-vision.so")))
                    .unwrap_or_default(),
                // 4. Bare SONAME — defers to the dynamic linker
                PathBuf::from("libkysdk-genai-vision.so"),
            ];

            for path in &candidates {
                if path.as_os_str().is_empty() {
                    continue;
                }
                match unsafe { Library::new(path) } {
                    Ok(lib) => {
                        // Verify library exports expected image generation symbols
                        let valid = unsafe {
                            lib.get::<unsafe extern "C" fn() -> *mut GenAiImageSession>(
                                b"genai_image_create_session\0",
                            )
                            .is_ok()
                        };
                        if valid {
                            eprintln!("[kart-vision-sys] loaded library from {}", path.display());
                            return Some(lib);
                        } else {
                            eprintln!(
                                "[kart-vision-sys] {} loaded but image symbols not found — not a valid SDK",
                                path.display()
                            );
                        }
                    }
                    Err(e) => {
                        eprintln!("[kart-vision-sys] could not load {}: {}", path.display(), e);
                    }
                }
            }
            eprintln!(
                "[kart-vision-sys] no library found; set KYSDK_GENAI_VISION_LIB_PATH or install the SDK"
            );
            None
        })
        .as_ref()
}

// ── C Callback Bridge ───────────────────────────────────────────────────────

/// C-compatible callback wrapper for image generation results.
///
/// # Safety
/// This function is called from C code. The user_data must be a valid
/// pointer to an ImageCallbackContext.
unsafe extern "C" fn image_result_callback(
    result: *mut VisionImageResult,
    user_data: *mut c_void,
) {
    if user_data.is_null() {
        return;
    }

    let ctx = unsafe { &*(user_data as *const ImageCallbackContext) };

    if let Some(lib) = lib() {
        // Get error code
        let get_error_code: Symbol<unsafe extern "C" fn(*mut VisionImageResult) -> i32> =
            unsafe {
                match lib.get(b"image_result_get_error_code\0") {
                    Ok(f) => f,
                    Err(_) => return,
                }
            };

        let error_code = unsafe { get_error_code(result) };

        if error_code != 0 {
            // Get error message
            let get_error_msg: Symbol<
                unsafe extern "C" fn(*mut VisionImageResult) -> *const c_char,
            > = unsafe {
                match lib.get(b"image_result_get_error_message\0") {
                    Ok(f) => f,
                    Err(_) => return,
                }
            };

            let message = unsafe {
                let ptr = get_error_msg(result);
                if !ptr.is_null() {
                    CStr::from_ptr(ptr).to_str().ok().map(String::from).unwrap_or_default()
                } else {
                    String::from("Unknown error")
                }
            };

            let err = match error_code {
                100 => KartVisionError::InputTextTooLong,
                101 => KartVisionError::InvalidStyle(message),
                102 => KartVisionError::InvalidSize,
                103 => KartVisionError::InvalidNumber,
                104 => KartVisionError::GenerationBlocked,
                _ => KartVisionError::GenerationFailed { code: error_code, message },
            };

            let _ = ctx.tx.send(Err(err));
            return;
        }

        // Get image data
        let get_data: Symbol<
            unsafe extern "C" fn(*mut VisionImageResult, *mut i32) -> *const u8,
        > = unsafe {
            match lib.get(b"image_result_get_data\0") {
                Ok(f) => f,
                Err(_) => {
                    let _ = ctx.tx.send(Err(KartVisionError::FunctionNotFound("image_result_get_data".into())));
                    return;
                }
            }
        };

        // Get format
        let get_format: Symbol<unsafe extern "C" fn(*mut VisionImageResult) -> i32> =
            unsafe {
                match lib.get(b"image_result_get_format\0") {
                    Ok(f) => f,
                    Err(_) => {
                        let _ = ctx.tx.send(Err(KartVisionError::FunctionNotFound("image_result_get_format".into())));
                        return;
                    }
                }
            };

        // Get size
        let get_size: Symbol<unsafe extern "C" fn(*mut VisionImageResult) -> ImageSize> =
            unsafe {
                match lib.get(b"image_result_get_size\0") {
                    Ok(f) => f,
                    Err(_) => {
                        let _ = ctx.tx.send(Err(KartVisionError::FunctionNotFound("image_result_get_size".into())));
                        return;
                    }
                }
            };

        let mut data_length: i32 = 0;
        let data_ptr = unsafe { get_data(result, &mut data_length) };
        let format_raw = unsafe { get_format(result) };
        let size = unsafe { get_size(result) };

        let image_data = if !data_ptr.is_null() && data_length > 0 {
            unsafe { std::slice::from_raw_parts(data_ptr, data_length as usize).to_vec() }
        } else {
            Vec::new()
        };

        let output = ImageOutput {
            data: image_data,
            format: ImageFormat::from_raw(format_raw),
            width: size.width as u32,
            height: size.height as u32,
        };

        let _ = ctx.tx.send(Ok(output));
    }
}

// ── Session Handle (RAII) ────────────────────────────────────────────────────

/// Safe RAII handle over the image generation session.
///
/// Automatically destroys the session on `Drop`.
pub struct ImageGenerationSession {
    session: *mut GenAiImageSession,
    /// Rust-side Arc that owns the callback context allocation.
    callback_ctx: Option<Arc<ImageCallbackContext>>,
    /// Raw pointer derived from a cloned Arc and passed to C.
    callback_raw: *const ImageCallbackContext,
}

impl ImageGenerationSession {
    /// Create a new image generation session.
    pub fn new() -> Result<Self> {
        let lib = lib().ok_or(KartVisionError::LibraryNotFound)?;

        // Create session
        let session: *mut GenAiImageSession = unsafe {
            let f: Symbol<unsafe extern "C" fn() -> *mut GenAiImageSession> = lib
                .get(b"genai_image_create_session\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;
            f()
        };

        if session.is_null() {
            return Err(KartVisionError::SessionCreateFailed);
        }

        // Initialize session
        let ret: i32 = unsafe {
            let f: Symbol<unsafe extern "C" fn(*mut GenAiImageSession) -> i32> = lib
                .get(b"genai_image_init_session\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;
            f(session)
        };

        if ret != 0 {
            unsafe {
                let destroy: Symbol<unsafe extern "C" fn(*mut *mut GenAiImageSession)> =
                    lib.get(b"genai_image_destroy_session\0")
                        .map_err(|_| KartVisionError::LibraryNotFound)?;
                let mut s = session;
                destroy(&mut s);
            }
            return Err(KartVisionError::SessionInitFailed(ret));
        }

        // Enable the SDK's internal event loop so callbacks fire without a GLib main loop.
        unsafe {
            if let Ok(f) = lib.get::<unsafe extern "C" fn(*mut GenAiImageSession, bool)>(
                b"genai_image_enable_internal_event_loop\0",
            ) {
                f(session, true);
                tracing::debug!("Internal event loop enabled for ImageGenerationSession");
            } else {
                tracing::warn!("genai_image_enable_internal_event_loop not found — callbacks may not fire without GLib");
            }
        }

        tracing::debug!("ImageGenerationSession created successfully");

        Ok(Self {
            session,
            callback_ctx: None,
            callback_raw: std::ptr::null(),
        })
    }

    /// Set the image generation configuration.
    #[allow(unused_variables)]
    pub fn set_config(&mut self, config: &ImageGenConfig) -> Result<()> {
        let lib = lib().ok_or(KartVisionError::LibraryNotFound)?;

        unsafe {
            // Create config
            let create_config: Symbol<unsafe extern "C" fn() -> *mut ImageConfig> =
                lib.get(b"image_config_create\0")
                    .map_err(|_| KartVisionError::LibraryNotFound)?;
            let config_ptr = create_config();

            if config_ptr.is_null() {
                return Err(KartVisionError::InvalidInput("Failed to create config".into()));
            }

            // Set style if provided
            if let Some(ref style) = config.style {
                let style_cstr = CString::new(style.as_str())
                    .map_err(|e| KartVisionError::InvalidInput(e.to_string()))?;
                let set_style: Symbol<
                    unsafe extern "C" fn(*mut ImageConfig, *const c_char),
                > = lib
                    .get(b"image_config_set_style\0")
                    .map_err(|_| KartVisionError::LibraryNotFound)?;
                set_style(config_ptr, style_cstr.as_ptr());
            }

            // Set size if provided
            if let Some(size) = config.size {
                let set_size: Symbol<
                    unsafe extern "C" fn(*mut ImageConfig, ImageSize),
                > = lib
                    .get(b"image_config_set_size\0")
                    .map_err(|_| KartVisionError::LibraryNotFound)?;
                set_size(config_ptr, size);
            }

            // Set generation number if provided
            if let Some(number) = config.number {
                let set_number: Symbol<
                    unsafe extern "C" fn(*mut ImageConfig, i32),
                > = lib
                    .get(b"image_config_set_generation_number\0")
                    .map_err(|_| KartVisionError::LibraryNotFound)?;
                set_number(config_ptr, number);
            }

            // Apply config to session
            let set_config: Symbol<
                unsafe extern "C" fn(*mut GenAiImageSession, *mut ImageConfig),
            > = lib
                .get(b"genai_image_set_config\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;
            set_config(self.session, config_ptr);

            // Destroy config (session retains a copy internally)
            let destroy_config: Symbol<unsafe extern "C" fn(*mut *mut ImageConfig)> =
                lib.get(b"image_config_destroy\0")
                    .map_err(|_| KartVisionError::LibraryNotFound)?;
            let mut config_ptr_mut = config_ptr;
            destroy_config(&mut config_ptr_mut);
        }

        Ok(())
    }

    /// Generate an image asynchronously.
    pub async fn generate(&mut self, prompt: &str, config: Option<ImageGenConfig>) -> Result<ImageOutput> {
        let lib = lib().ok_or(KartVisionError::LibraryNotFound)?;

        // Set config if provided
        if let Some(ref cfg) = config {
            self.set_config(cfg)?;
        }

        let (tx, mut rx) = mpsc::unbounded_channel();

        // Build Arc for the context, give C a raw pointer from a clone
        let ctx = Arc::new(ImageCallbackContext { tx });
        let raw = Arc::into_raw(Arc::clone(&ctx));

        // Reclaim any previous raw ptr we gave to C
        if !self.callback_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.callback_raw)) }
        }

        // Register callback
        unsafe {
            let set_callback: Symbol<
                unsafe extern "C" fn(
                    *mut GenAiImageSession,
                    ImageResultCallback,
                    *mut c_void,
                ),
            > = lib
                .get(b"genai_image_result_set_callback\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;
            set_callback(
                self.session,
                Some(image_result_callback),
                raw as *mut c_void,
            );
        }

        // Start generation
        let prompt_cstr =
            CString::new(prompt).map_err(|e| KartVisionError::InvalidInput(e.to_string()))?;

        unsafe {
            let generate: Symbol<
                unsafe extern "C" fn(*mut GenAiImageSession, *const c_char),
            > = lib
                .get(b"genai_image_generate_image_async\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;
            generate(self.session, prompt_cstr.as_ptr());
        }

        // Store context
        self.callback_ctx = Some(ctx);
        self.callback_raw = raw;

        // Wait for result with timeout
        match tokio::time::timeout(std::time::Duration::from_secs(120), rx.recv()).await {
            Ok(Some(result)) => result,
            Ok(None) => Err(KartVisionError::Timeout),
            Err(_) => Err(KartVisionError::Timeout),
        }
    }

    /// Get supported image styles.
    pub fn supported_styles(&self) -> Result<Vec<String>> {
        let lib = lib().ok_or(KartVisionError::LibraryNotFound)?;

        unsafe {
            let get_styles: Symbol<
                unsafe extern "C" fn(*mut GenAiImageSession, *mut i32) -> *const *const c_char,
            > = lib
                .get(b"genai_image_get_supported_image_style\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;

            let mut number: i32 = 0;
            let styles_ptr = get_styles(self.session, &mut number);

            if styles_ptr.is_null() || number == 0 {
                return Ok(Vec::new());
            }

            let mut styles = Vec::new();
            let slice = std::slice::from_raw_parts(styles_ptr, number as usize);
            for &ptr in slice {
                if !ptr.is_null() {
                    if let Ok(s) = CStr::from_ptr(ptr).to_str() {
                        styles.push(s.to_string());
                    }
                }
            }

            Ok(styles)
        }
    }

    /// Get supported image sizes.
    pub fn supported_sizes(&self) -> Result<Vec<ImageSize>> {
        let lib = lib().ok_or(KartVisionError::LibraryNotFound)?;

        unsafe {
            let get_sizes: Symbol<
                unsafe extern "C" fn(*mut GenAiImageSession, *mut i32) -> *const ImageSize,
            > = lib
                .get(b"genai_image_get_supported_image_size\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;

            let mut number: i32 = 0;
            let sizes_ptr = get_sizes(self.session, &mut number);

            if sizes_ptr.is_null() || number == 0 {
                return Ok(Vec::new());
            }

            let slice = std::slice::from_raw_parts(sizes_ptr, number as usize);
            Ok(slice.to_vec())
        }
    }

    /// Get maximum supported generation number.
    pub fn max_generation_number(&self) -> Result<i32> {
        let lib = lib().ok_or(KartVisionError::LibraryNotFound)?;

        unsafe {
            let get_number: Symbol<
                unsafe extern "C" fn(*mut GenAiImageSession) -> i32,
            > = lib
                .get(b"genai_image_get_supported_image_number\0")
                .map_err(|_| KartVisionError::LibraryNotFound)?;

            Ok(get_number(self.session))
        }
    }
}

impl Drop for ImageGenerationSession {
    fn drop(&mut self) {
        if let Some(lib) = lib() {
            unsafe {
                if let Ok(f) =
                    lib.get::<unsafe extern "C" fn(*mut *mut GenAiImageSession)>(
                        b"genai_image_destroy_session\0",
                    )
                {
                    f(&mut self.session);
                }
            }
        }
        // Reclaim the Arc refcount we gave to C
        if !self.callback_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.callback_raw)) };
            self.callback_raw = std::ptr::null();
        }
    }
}

unsafe impl Send for ImageGenerationSession {}
unsafe impl Sync for ImageGenerationSession {}

/// Check if the SDK library is available.
pub fn is_available() -> bool {
    lib().is_some()
}

// ── Tests ────────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn is_available_does_not_panic() {
        let _ = is_available();
    }

    #[test]
    fn error_display() {
        let err = KartVisionError::LibraryNotFound;
        assert!(err.to_string().contains("not found"));

        let err = KartVisionError::SessionInitFailed(-1);
        assert!(err.to_string().contains("-1"));

        let err = KartVisionError::InvalidInput("null byte".into());
        assert!(err.to_string().contains("Invalid input"));
        assert!(err.to_string().contains("null byte"));
    }

    #[test]
    fn error_generation_failed() {
        let err = KartVisionError::GenerationFailed {
            code: 1001,
            message: "service unavailable".into(),
        };
        assert!(err.to_string().contains("SDK error"));
        assert!(err.to_string().contains("1001"));
        assert!(err.to_string().contains("service unavailable"));
    }

    #[test]
    fn error_function_not_found() {
        let err = KartVisionError::FunctionNotFound("genai_image_foo".into());
        assert!(err.to_string().contains("Function not found"));
        assert!(err.to_string().contains("genai_image_foo"));
    }

    #[test]
    fn error_timeout() {
        let err = KartVisionError::Timeout;
        assert!(err.to_string().contains("timed out"));
    }

    #[test]
    fn error_input_text_too_long() {
        let err = KartVisionError::InputTextTooLong;
        assert!(err.to_string().contains("too long"));
    }

    #[test]
    fn error_invalid_style() {
        let err = KartVisionError::InvalidStyle("unknown".into());
        assert!(err.to_string().contains("Invalid style"));
        assert!(err.to_string().contains("unknown"));
    }

    #[test]
    fn image_format_from_raw() {
        assert_eq!(ImageFormat::from_raw(0), ImageFormat::Jpeg);
        assert_eq!(ImageFormat::from_raw(1), ImageFormat::Png);
        assert_eq!(ImageFormat::from_raw(2), ImageFormat::Webp);
        assert_eq!(ImageFormat::from_raw(99), ImageFormat::Unknown);
    }

    #[test]
    fn image_format_as_str() {
        assert_eq!(ImageFormat::Jpeg.as_str(), "jpeg");
        assert_eq!(ImageFormat::Png.as_str(), "png");
        assert_eq!(ImageFormat::Webp.as_str(), "webp");
        assert_eq!(ImageFormat::Unknown.as_str(), "unknown");
    }

    #[test]
    fn session_new_returns_error_or_success() {
        let result = ImageGenerationSession::new();
        match &result {
            Ok(_) => tracing::debug!("ImageGenerationSession created OK"),
            Err(e) => tracing::debug!("ImageGenerationSession returned error: {}", e),
        }
        let _ = result;
    }

    #[test]
    fn lib_loads_on_first_call() {
        let lib_opt = lib();
        match lib_opt {
            Some(lib) => {
                let has_sym = unsafe {
                    lib.get::<unsafe extern "C" fn() -> *mut GenAiImageSession>(
                        b"genai_image_create_session\0",
                    )
                    .is_ok()
                };
                assert!(has_sym, "Library loaded but image symbol not found");
            }
            None => {
                tracing::debug!("lib() returned None — SDK not available");
            }
        }
    }
}