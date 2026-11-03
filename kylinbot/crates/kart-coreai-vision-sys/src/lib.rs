//! Safe Rust bindings for libkysdk-coreai-vision SDK (OCR + subject segmentation).

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_void};
use std::path::PathBuf;
use std::sync::{Arc, OnceLock};

use libloading::{Library, Symbol};
use thiserror::Error;
use tokio::sync::{mpsc, Mutex, OnceCell};

// ── Bindings ────────────────────────────────────────────────────────────────

#[cfg(not(feature = "stub"))]
#[allow(nonstandard_style, unused)]
mod bindings;

#[cfg(not(feature = "stub"))]
pub use bindings::{
    PixelPoint, SegmentationMaskData, SubjectSegmentationConfig,
    SubjectSegmentationInitCallback, SubjectSegmentationMode,
    SubjectSegmentationPointGroup, SubjectSegmentationResult,
    SubjectSegmentationResultCallback, SubjectSegmentationSession,
    TextLine, TextRecognitionResult, TextRecognitionResultCallback,
    TextRecognitionSession,
};

#[cfg(feature = "stub")]
pub mod stub_types {
    use std::os::raw::c_void;

    pub type TextRecognitionSession = c_void;
    pub type TextRecognitionResult = c_void;
    pub type TextLine = c_void;
    pub type TextRecognitionResultCallback =
        Option<unsafe extern "C" fn(*mut TextRecognitionResult, *mut c_void)>;
    pub type SubjectSegmentationSession = c_void;
    pub type SubjectSegmentationResult = c_void;
    pub type SubjectSegmentationConfig = c_void;
    pub type SubjectSegmentationResultCallback =
        Option<unsafe extern "C" fn(*mut SubjectSegmentationResult, *mut c_void)>;
    pub type SubjectSegmentationInitCallback =
        Option<unsafe extern "C" fn(i32, *mut c_void)>;

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct PixelPoint {
        pub x: i32,
        pub y: i32,
    }

    #[repr(C)]
    #[derive(Debug, Clone, Copy)]
    pub struct SegmentationMaskData {
        pub mask_data: *mut u8,
        pub mask_length: usize,
        pub mask_width: i32,
        pub mask_height: i32,
    }

    pub const SUBJECT_SEGMENTATION_MODE_PORTRAIT: u32 = 0;
    pub const SUBJECT_SEGMENTATION_MODE_GENERAL: u32 = 1;
    pub type SubjectSegmentationMode = u32;

    #[repr(C)]
    pub struct SubjectSegmentationPointGroup {
        pub points: *mut PixelPoint,
        pub point_count: i32,
    }
}

#[cfg(feature = "stub")]
pub use stub_types::*;

// ── Error Types ─────────────────────────────────────────────────────────────

#[derive(Debug, Error)]
pub enum KartCoreVisionError {
    #[error("libkysdk-coreai-vision SDK not found — set KYSDK_COREAI_VISION_LIB_PATH or install the SDK")]
    LibraryNotFound,

    #[error("Session initialization failed (code {0})")]
    SessionInitFailed(i32),

    #[error("Callback wait timed out")]
    Timeout,

    #[error("OCR failed (code {code}): {message}")]
    OcrFailed { code: i32, message: String },

    #[error("Segmentation failed (code {code}): {message}")]
    SegmentationFailed { code: i32, message: String },

    #[error("Image not found: {0}")]
    ImageNotFound(String),
}

pub type Result<T> = std::result::Result<T, KartCoreVisionError>;

// ── Output Types ────────────────────────────────────────────────────────────

#[derive(Debug, Clone)]
pub struct TextLineResult {
    pub text: String,
    pub corner_points: Vec<(i32, i32)>,
}

#[derive(Debug, Clone)]
pub struct OcrResult {
    pub text: String,
    pub lines: Vec<TextLineResult>,
}

#[derive(Debug, Clone)]
pub struct SegmentationResult {
    pub foreground_data: Vec<u8>,
    pub format: String,
    pub width: u32,
    pub height: u32,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SegmentationMode {
    Portrait,
    General,
}

impl Default for SegmentationMode {
    fn default() -> Self {
        Self::General
    }
}

// ── Callback Contexts ───────────────────────────────────────────────────────

struct OcrCallbackContext {
    tx: mpsc::UnboundedSender<Result<OcrResult>>,
}

struct SegmentationCallbackContext {
    tx: mpsc::UnboundedSender<Result<SegmentationResult>>,
}

struct InitCallbackContext {
    tx: mpsc::UnboundedSender<Result<()>>,
}

// ── Library Loading ─────────────────────────────────────────────────────────

static KYSDK_COREAI_VISION_LIB: OnceLock<Option<Library>> = OnceLock::new();

fn lib() -> Option<&'static Library> {
    KYSDK_COREAI_VISION_LIB
        .get_or_init(|| {
            #[cfg(target_arch = "x86_64")]
            const SYSTEM_LIB: &str = "/usr/lib/x86_64-linux-gnu/libkysdk-coreai-vision.so";
            #[cfg(target_arch = "aarch64")]
            const SYSTEM_LIB: &str = "/usr/lib/aarch64-linux-gnu/libkysdk-coreai-vision.so";
            #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
            const SYSTEM_LIB: &str = "";

            let candidates: Vec<PathBuf> = vec![
                std::env::var("KYSDK_COREAI_VISION_LIB_PATH")
                    .ok()
                    .map(PathBuf::from)
                    .unwrap_or_default(),
                PathBuf::from(SYSTEM_LIB),
                std::env::current_exe()
                    .ok()
                    .and_then(|e| e.parent().map(|d| d.join("libkysdk-coreai-vision.so")))
                    .unwrap_or_default(),
                PathBuf::from("libkysdk-coreai-vision.so"),
            ];

            for path in &candidates {
                if path.as_os_str().is_empty() {
                    continue;
                }
                match unsafe { Library::new(path) } {
                    Ok(lib) => {
                        let valid = unsafe {
                            lib.get::<unsafe extern "C" fn() -> *mut TextRecognitionSession>(
                                b"text_recognition_create_session\0",
                            )
                            .is_ok()
                        };
                        if valid {
                            eprintln!(
                                "[kart-coreai-vision-sys] loaded library from {}",
                                path.display()
                            );
                            return Some(lib);
                        } else {
                            eprintln!(
                                "[kart-coreai-vision-sys] {} loaded but symbols not found",
                                path.display()
                            );
                        }
                    }
                    Err(e) => {
                        eprintln!(
                            "[kart-coreai-vision-sys] could not load {}: {}",
                            path.display(),
                            e
                        );
                    }
                }
            }
            eprintln!(
                "[kart-coreai-vision-sys] no library found; set KYSDK_COREAI_VISION_LIB_PATH or install the SDK"
            );
            None
        })
        .as_ref()
}

// ── OCR Callback Bridge ─────────────────────────────────────────────────────

unsafe extern "C" fn ocr_result_callback(
    result: *mut TextRecognitionResult,
    user_data: *mut c_void,
) {
    if user_data.is_null() {
        return;
    }

    if result.is_null() {
        let ctx = unsafe { &*(user_data as *const OcrCallbackContext) };
        let _ = ctx.tx.send(Err(KartCoreVisionError::OcrFailed { code: -1, message: "null result".into() }));
        return;
    }

    let ctx = unsafe { &*(user_data as *const OcrCallbackContext) };

    let Some(lib) = lib() else {
        return;
    };

    let get_error_code: Symbol<unsafe extern "C" fn(*mut TextRecognitionResult) -> i32> = unsafe {
        match lib.get(b"text_recognition_result_get_error_code\0") {
            Ok(f) => f,
            Err(_) => return,
        }
    };

    let error_code = unsafe { get_error_code(result) };

    if error_code != 0 {
        let get_error_msg: Symbol<
            unsafe extern "C" fn(*mut TextRecognitionResult) -> *const c_char,
        > = unsafe {
            match lib.get(b"text_recognition_result_get_error_message\0") {
                Ok(f) => f,
                Err(_) => return,
            }
        };

        let message = unsafe {
            let ptr = get_error_msg(result);
            if !ptr.is_null() {
                CStr::from_ptr(ptr)
                    .to_str()
                    .ok()
                    .map(String::from)
                    .unwrap_or_default()
            } else {
                String::from("Unknown error")
            }
        };

        let _ = ctx.tx.send(Err(KartCoreVisionError::OcrFailed {
            code: error_code,
            message,
        }));
        return;
    }

    // Extract full text
    let get_value: Symbol<unsafe extern "C" fn(*mut TextRecognitionResult) -> *const c_char> =
        unsafe {
            match lib.get(b"text_recognition_result_get_value\0") {
                Ok(f) => f,
                Err(_) => return,
            }
        };

    let full_text = unsafe {
        let ptr = get_value(result);
        if !ptr.is_null() {
            CStr::from_ptr(ptr)
                .to_str()
                .ok()
                .map(String::from)
                .unwrap_or_default()
        } else {
            String::new()
        }
    };

    // Extract text lines
    let get_text_lines: Symbol<
        unsafe extern "C" fn(*mut TextRecognitionResult, *mut i32) -> *mut *mut TextLine,
    > = unsafe {
        match lib.get(b"text_recognition_result_get_text_lines\0") {
            Ok(f) => f,
            Err(_) => {
                let _ = ctx.tx.send(Ok(OcrResult {
                    text: full_text,
                    lines: Vec::new(),
                }));
                return;
            }
        }
    };

    let get_line_value: Symbol<unsafe extern "C" fn(*mut TextLine) -> *const c_char> = unsafe {
        match lib.get(b"text_line_get_value\0") {
            Ok(f) => f,
            Err(_) => {
                let _ = ctx.tx.send(Ok(OcrResult {
                    text: full_text,
                    lines: Vec::new(),
                }));
                return;
            }
        }
    };

    let get_corner_points: Symbol<
        unsafe extern "C" fn(*mut TextLine, *mut i32) -> *mut PixelPoint,
    > = unsafe {
        match lib.get(b"text_line_get_corner_points\0") {
            Ok(f) => f,
            Err(_) => {
                let _ = ctx.tx.send(Ok(OcrResult {
                    text: full_text,
                    lines: Vec::new(),
                }));
                return;
            }
        }
    };

    let mut line_count: i32 = 0;
    let lines_ptr = unsafe { get_text_lines(result, &mut line_count) };

    let mut lines = Vec::new();
    if !lines_ptr.is_null() && line_count > 0 {
        let line_slice = unsafe { std::slice::from_raw_parts(lines_ptr, line_count as usize) };
        for &line_ptr in line_slice {
            if line_ptr.is_null() {
                continue;
            }

            let line_text = unsafe {
                let ptr = get_line_value(line_ptr);
                if !ptr.is_null() {
                    CStr::from_ptr(ptr)
                        .to_str()
                        .ok()
                        .map(String::from)
                        .unwrap_or_default()
                } else {
                    String::new()
                }
            };

            let mut point_count: i32 = 0;
            let points_ptr = unsafe { get_corner_points(line_ptr, &mut point_count) };

            let mut corner_points = Vec::new();
            if !points_ptr.is_null() && point_count > 0 {
                let point_slice =
                    unsafe { std::slice::from_raw_parts(points_ptr, point_count as usize) };
                for p in point_slice {
                    corner_points.push((p.x, p.y));
                }
            }

            lines.push(TextLineResult {
                text: line_text,
                corner_points,
            });
        }
    }

    let _ = ctx.tx.send(Ok(OcrResult {
        text: full_text,
        lines,
    }));
}

// ── Subject Segmentation Callback Bridges ───────────────────────────────────

unsafe extern "C" fn segmentation_init_callback(error_code: i32, user_data: *mut c_void) {
    if user_data.is_null() {
        return;
    }
    let ctx = unsafe { &*(user_data as *const InitCallbackContext) };
    if error_code == 0 {
        let _ = ctx.tx.send(Ok(()));
    } else {
        let _ = ctx
            .tx
            .send(Err(KartCoreVisionError::SessionInitFailed(error_code)));
    }
}

unsafe extern "C" fn segmentation_result_callback(
    result: *mut SubjectSegmentationResult,
    user_data: *mut c_void,
) {
    if user_data.is_null() {
        return;
    }

    if result.is_null() {
        let ctx = unsafe { &*(user_data as *const SegmentationCallbackContext) };
        let _ = ctx.tx.send(Err(KartCoreVisionError::SegmentationFailed { code: -1, message: "null result".into() }));
        return;
    }

    let ctx = unsafe { &*(user_data as *const SegmentationCallbackContext) };

    let Some(lib) = lib() else {
        return;
    };

    let get_error_code: Symbol<
        unsafe extern "C" fn(*const SubjectSegmentationResult) -> i32,
    > = unsafe {
        match lib.get(b"subject_segmentation_result_get_error_code\0") {
            Ok(f) => f,
            Err(_) => return,
        }
    };

    let error_code = unsafe { get_error_code(result) };

    if error_code != 0 {
        let get_error_msg: Symbol<
            unsafe extern "C" fn(*const SubjectSegmentationResult) -> *const c_char,
        > = unsafe {
            match lib.get(b"subject_segmentation_result_get_error_message\0") {
                Ok(f) => f,
                Err(_) => return,
            }
        };

        let message = unsafe {
            let ptr = get_error_msg(result);
            if !ptr.is_null() {
                CStr::from_ptr(ptr)
                    .to_str()
                    .ok()
                    .map(String::from)
                    .unwrap_or_default()
            } else {
                String::from("Unknown error")
            }
        };

        let _ = ctx.tx.send(Err(KartCoreVisionError::SegmentationFailed {
            code: error_code,
            message,
        }));
        return;
    }

    // Get foreground image data
    let get_fg_data: Symbol<
        unsafe extern "C" fn(
            *const SubjectSegmentationResult,
            *mut i32,
            *mut i32,
            *mut usize,
        ) -> *const u8,
    > = unsafe {
        match lib.get(b"subject_segmentation_result_get_foreground_image_data\0") {
            Ok(f) => f,
            Err(_) => return,
        }
    };

    let get_fg_format: Symbol<
        unsafe extern "C" fn(*const SubjectSegmentationResult) -> *const c_char,
    > = unsafe {
        match lib.get(b"subject_segmentation_result_get_foreground_image_format\0") {
            Ok(f) => f,
            Err(_) => return,
        }
    };

    let mut width: i32 = 0;
    let mut height: i32 = 0;
    let mut data_length: usize = 0;

    let data_ptr = unsafe { get_fg_data(result, &mut width, &mut height, &mut data_length) };

    let foreground_data = if !data_ptr.is_null() && data_length > 0 {
        unsafe { std::slice::from_raw_parts(data_ptr, data_length).to_vec() }
    } else {
        Vec::new()
    };

    let format = unsafe {
        let ptr = get_fg_format(result);
        if !ptr.is_null() {
            CStr::from_ptr(ptr)
                .to_str()
                .ok()
                .map(String::from)
                .unwrap_or_else(|| "png".into())
        } else {
            "png".into()
        }
    };

    let _ = ctx.tx.send(Ok(SegmentationResult {
        foreground_data,
        format,
        width: width as u32,
        height: height as u32,
    }));
}

// ── TextRecognitionSession (RAII) ───────────────────────────────────────────

pub struct TextRecognitionSessionHandle {
    session: *mut TextRecognitionSession,
    callback_ctx: Option<Arc<OcrCallbackContext>>,
    callback_raw: *const OcrCallbackContext,
}

impl TextRecognitionSessionHandle {
    pub fn new() -> Result<Self> {
        let lib = lib().ok_or(KartCoreVisionError::LibraryNotFound)?;

        let session: *mut TextRecognitionSession = unsafe {
            let f: Symbol<unsafe extern "C" fn() -> *mut TextRecognitionSession> = lib
                .get(b"text_recognition_create_session\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            f()
        };

        if session.is_null() {
            return Err(KartCoreVisionError::SessionInitFailed(0));
        }

        // Synchronous init
        let ret: i32 = unsafe {
            let f: Symbol<unsafe extern "C" fn(*mut TextRecognitionSession) -> i32> = lib
                .get(b"text_recognition_init_session\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            f(session)
        };

        if ret != 0 {
            unsafe {
                if let Ok(destroy) =
                    lib.get::<unsafe extern "C" fn(*mut *mut TextRecognitionSession)>(
                        b"text_recognition_destroy_session\0",
                    )
                {
                    let mut s = session;
                    destroy(&mut s);
                }
            }
            return Err(KartCoreVisionError::SessionInitFailed(ret));
        }

        // Do NOT enable the internal event loop.  All kart C SDKs share
        // g_main_context_default(); the NLP SDK already pumps that context,
        // which dispatches our DBus callbacks.  A second loop thread segfaults.
        tracing::debug!("TextRecognitionSession created (no internal event loop)");

        Ok(Self {
            session,
            callback_ctx: None,
            callback_raw: std::ptr::null(),
        })
    }

    pub async fn recognize_from_file(&mut self, image_path: &str) -> Result<OcrResult> {
        let lib = lib().ok_or(KartCoreVisionError::LibraryNotFound)?;

        let (tx, mut rx) = mpsc::unbounded_channel();
        let ctx = Arc::new(OcrCallbackContext { tx });
        let raw = Arc::into_raw(Arc::clone(&ctx));

        // Register callback BEFORE dropping old one - order matters!
        unsafe {
            let set_callback: Symbol<
                unsafe extern "C" fn(
                    *mut TextRecognitionSession,
                    TextRecognitionResultCallback,
                    *mut c_void,
                ),
            > = lib
                .get(b"text_recognition_result_set_callback\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            set_callback(self.session, Some(ocr_result_callback), raw as *mut c_void);
        }

        // NOW it's safe to drop the old Arc - new callback is registered
        if !self.callback_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.callback_raw)) }
        }

        // Start recognition
        let path_cstr = CString::new(image_path)
            .map_err(|_| KartCoreVisionError::ImageNotFound(image_path.to_string()))?;

        unsafe {
            let recognize: Symbol<
                unsafe extern "C" fn(*mut TextRecognitionSession, *const c_char),
            > = lib
                .get(b"text_recognition_recognize_text_from_image_file_async\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            recognize(self.session, path_cstr.as_ptr());
        }

        self.callback_ctx = Some(ctx);
        self.callback_raw = raw;

        match tokio::time::timeout(std::time::Duration::from_secs(60), rx.recv()).await {
            Ok(Some(result)) => result,
            Ok(None) => Err(KartCoreVisionError::Timeout),
            Err(_) => Err(KartCoreVisionError::Timeout),
        }
    }

}

impl Drop for TextRecognitionSessionHandle {
    fn drop(&mut self) {
        // Internal event loop is never started, so nothing to stop.
        // destroy_session is skipped: the SDK's own destructor is a no-op
        // (// TODO 不析构，待查) and calling it corrupts the shared GLib context.
        if !self.callback_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.callback_raw)) };
            self.callback_raw = std::ptr::null();
        }
    }
}

unsafe impl Send for TextRecognitionSessionHandle {}
unsafe impl Sync for TextRecognitionSessionHandle {}

// ── SubjectSegmentationSession (RAII) ───────────────────────────────────────

pub struct SubjectSegmentationSessionHandle {
    session: *mut SubjectSegmentationSession,
    _init_ctx: Option<Arc<InitCallbackContext>>,
    init_raw: *const InitCallbackContext,
    result_ctx: Option<Arc<SegmentationCallbackContext>>,
    result_raw: *const SegmentationCallbackContext,
}

impl SubjectSegmentationSessionHandle {
    pub async fn new() -> Result<Self> {
        let (handle, init_rx) = Self::create_and_start_init()?;

        // Wait for init with 30s timeout — handle is Send so this is safe
        match tokio::time::timeout(std::time::Duration::from_secs(30), init_rx).await {
            Ok(Ok(Ok(()))) => {
                tracing::debug!("SubjectSegmentationSession initialized successfully");
            }
            Ok(Ok(Err(e))) => {
                drop(handle);
                return Err(e);
            }
            Ok(Err(_)) | Err(_) => {
                drop(handle);
                return Err(KartCoreVisionError::Timeout);
            }
        }

        Ok(handle)
    }

    fn create_and_start_init() -> Result<(Self, tokio::sync::oneshot::Receiver<Result<()>>)> {
        let lib = lib().ok_or(KartCoreVisionError::LibraryNotFound)?;

        let session: *mut SubjectSegmentationSession = unsafe {
            let f: Symbol<unsafe extern "C" fn() -> *mut SubjectSegmentationSession> = lib
                .get(b"subject_segmentation_create_session\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            f()
        };

        if session.is_null() {
            return Err(KartCoreVisionError::SessionInitFailed(0));
        }

        // Do NOT enable the internal event loop — see TextRecognitionSessionHandle::new().
        tracing::debug!("SubjectSegmentationSession created (no internal event loop)");

        // Async init with callback bridge — use oneshot for single init result
        let (init_tx, mut init_rx) = mpsc::unbounded_channel::<Result<()>>();
        let init_ctx = Arc::new(InitCallbackContext { tx: init_tx });
        let init_raw = Arc::into_raw(Arc::clone(&init_ctx));

        unsafe {
            let init_async: Symbol<
                unsafe extern "C" fn(
                    *mut SubjectSegmentationSession,
                    SubjectSegmentationInitCallback,
                    *mut c_void,
                ),
            > = lib
                .get(b"subject_segmentation_init_session_async\0")
                .map_err(|_| {
                    if let Ok(destroy) = lib
                        .get::<unsafe extern "C" fn(*mut *mut SubjectSegmentationSession)>(
                            b"subject_segmentation_destroy_session\0",
                        )
                    {
                        let mut s = session;
                        destroy(&mut s);
                    }
                    drop(Arc::from_raw(init_raw));
                    KartCoreVisionError::LibraryNotFound
                })?;
            init_async(
                session,
                Some(segmentation_init_callback),
                init_raw as *mut c_void,
            );
        }

        // Wrap mpsc receiver into oneshot for Send-safe await
        let (otx, orx) = tokio::sync::oneshot::channel();
        tokio::spawn(async move {
            let result = init_rx.recv().await.unwrap_or(Err(KartCoreVisionError::Timeout));
            let _ = otx.send(result);
        });

        let handle = Self {
            session,
            _init_ctx: Some(init_ctx),
            init_raw,
            result_ctx: None,
            result_raw: std::ptr::null(),
        };

        Ok((handle, orx))
    }

    pub async fn segment_from_file(
        &mut self,
        image_path: &str,
        mode: SegmentationMode,
    ) -> Result<SegmentationResult> {
        let lib = lib().ok_or(KartCoreVisionError::LibraryNotFound)?;

        // Create and set config with mode
        unsafe {
            let create_config: Symbol<unsafe extern "C" fn() -> *mut SubjectSegmentationConfig> =
                lib.get(b"subject_segmentation_config_create\0")
                    .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            let config_ptr = create_config();

            if !config_ptr.is_null() {
                let set_mode: Symbol<
                    unsafe extern "C" fn(*mut SubjectSegmentationConfig, SubjectSegmentationMode),
                > = lib
                    .get(b"subject_segmentation_config_set_mode\0")
                    .map_err(|_| KartCoreVisionError::LibraryNotFound)?;

                let mode_val: SubjectSegmentationMode = match mode {
                    SegmentationMode::Portrait => 0,
                    SegmentationMode::General => 1,
                };
                set_mode(config_ptr, mode_val);

                let set_config: Symbol<
                    unsafe extern "C" fn(
                        *mut SubjectSegmentationSession,
                        *mut SubjectSegmentationConfig,
                    ),
                > = lib
                    .get(b"subject_segmentation_set_config\0")
                    .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
                set_config(self.session, config_ptr);

                let destroy_config: Symbol<
                    unsafe extern "C" fn(*mut *mut SubjectSegmentationConfig),
                > = lib
                    .get(b"subject_segmentation_config_destroy\0")
                    .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
                let mut config_ptr_mut = config_ptr;
                destroy_config(&mut config_ptr_mut);
            }
        }

        // Set up result callback
        let (tx, mut rx) = mpsc::unbounded_channel();
        let ctx = Arc::new(SegmentationCallbackContext { tx });
        let raw = Arc::into_raw(Arc::clone(&ctx));

        // Register callback BEFORE dropping old one - order matters!
        unsafe {
            let set_callback: Symbol<
                unsafe extern "C" fn(
                    *mut SubjectSegmentationSession,
                    SubjectSegmentationResultCallback,
                    *mut c_void,
                ),
            > = lib
                .get(b"subject_segmentation_result_set_callback\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            set_callback(
                self.session,
                Some(segmentation_result_callback),
                raw as *mut c_void,
            );
        }

        // NOW it's safe to drop the old Arc - new callback is registered
        if !self.result_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.result_raw)) }
        }

        // Start segmentation
        let path_cstr = CString::new(image_path)
            .map_err(|_| KartCoreVisionError::ImageNotFound(image_path.to_string()))?;

        unsafe {
            let run: Symbol<
                unsafe extern "C" fn(
                    *mut SubjectSegmentationSession,
                    *const c_char,
                    *const c_char,
                ),
            > = lib
                .get(b"subject_segmentation_run_from_image_file_async\0")
                .map_err(|_| KartCoreVisionError::LibraryNotFound)?;
            run(self.session, std::ptr::null(), path_cstr.as_ptr());
        }

        self.result_ctx = Some(ctx);
        self.result_raw = raw;

        match tokio::time::timeout(std::time::Duration::from_secs(120), rx.recv()).await {
            Ok(Some(result)) => result,
            Ok(None) => Err(KartCoreVisionError::Timeout),
            Err(_) => Err(KartCoreVisionError::Timeout),
        }
    }
}

impl Drop for SubjectSegmentationSessionHandle {
    fn drop(&mut self) {
        // See TextRecognitionSessionHandle::Drop.
        if !self.init_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.init_raw)) };
        }
        if !self.result_raw.is_null() {
            unsafe { drop(Arc::from_raw(self.result_raw)) };
            self.result_raw = std::ptr::null();
        }
    }
}

unsafe impl Send for SubjectSegmentationSessionHandle {}
unsafe impl Sync for SubjectSegmentationSessionHandle {}

// ── Singleton API ─────────────────────────────────────────────────────────
//
// Each tool type gets one session, created on first use, never destroyed.
// All kart C SDKs share the process-wide GLib default context; repeatedly
// creating/destroying sessions races with the NLP SDK's event loop and
// segfaults.  A long-lived singleton avoids create/destroy entirely.

static OCR_SESSION: OnceCell<Mutex<TextRecognitionSessionHandle>> = OnceCell::const_new();
static SEG_SESSION: OnceCell<Mutex<SubjectSegmentationSessionHandle>> = OnceCell::const_new();

/// Run OCR on an image file.
pub async fn recognize_text(image_path: &str) -> Result<OcrResult> {
    let session_mutex = OCR_SESSION
        .get_or_try_init(|| async {
            let session = TextRecognitionSessionHandle::new()?;
            Ok::<_, KartCoreVisionError>(Mutex::new(session))
        })
        .await?;

    let mut session = session_mutex.lock().await;
    session.recognize_from_file(image_path).await
}

/// Run subject segmentation on an image file.
pub async fn segment_subject(
    image_path: &str,
    mode: SegmentationMode,
) -> Result<SegmentationResult> {
    let session_mutex = SEG_SESSION
        .get_or_try_init(|| async {
            let session = SubjectSegmentationSessionHandle::new().await?;
            Ok::<_, KartCoreVisionError>(Mutex::new(session))
        })
        .await?;

    let mut session = session_mutex.lock().await;
    session.segment_from_file(image_path, mode).await
}

// ── Public Utility ──────────────────────────────────────────────────────────

pub fn is_available() -> bool {
    lib().is_some()
}

// ── Tests ───────────────────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn is_available_does_not_panic() {
        let _ = is_available();
    }

    #[test]
    fn error_display() {
        let err = KartCoreVisionError::LibraryNotFound;
        assert!(err.to_string().contains("not found"));

        let err = KartCoreVisionError::SessionInitFailed(-1);
        assert!(err.to_string().contains("-1"));

        let err = KartCoreVisionError::Timeout;
        assert!(err.to_string().contains("timed out"));

        let err = KartCoreVisionError::OcrFailed {
            code: 100,
            message: "image error".into(),
        };
        assert!(err.to_string().contains("100"));
        assert!(err.to_string().contains("image error"));

        let err = KartCoreVisionError::SegmentationFailed {
            code: 200,
            message: "model error".into(),
        };
        assert!(err.to_string().contains("200"));
        assert!(err.to_string().contains("model error"));

        let err = KartCoreVisionError::ImageNotFound("/tmp/test.jpg".into());
        assert!(err.to_string().contains("/tmp/test.jpg"));
    }

    #[test]
    fn segmentation_mode_default_is_general() {
        assert_eq!(SegmentationMode::default(), SegmentationMode::General);
    }

    #[test]
    fn session_new_returns_error_or_success() {
        let result = TextRecognitionSessionHandle::new();
        match &result {
            Ok(_) => tracing::debug!("TextRecognitionSession created OK"),
            Err(e) => tracing::debug!("TextRecognitionSession returned error: {}", e),
        }
        let _ = result;
    }

    #[test]
    fn lib_loads_on_first_call() {
        let lib_opt = lib();
        match lib_opt {
            Some(lib) => {
                let has_sym = unsafe {
                    lib.get::<unsafe extern "C" fn() -> *mut TextRecognitionSession>(
                        b"text_recognition_create_session\0",
                    )
                    .is_ok()
                };
                assert!(has_sym, "Library loaded but OCR symbol not found");
            }
            None => {
                tracing::debug!("lib() returned None — SDK not available");
            }
        }
    }

    // ── Session lifecycle tests (require SDK) ──────────────────────────────

    #[test]
    fn ocr_session_create_drop() {
        if !is_available() {
            return;
        }
        let session = TextRecognitionSessionHandle::new().unwrap();
        drop(session);
    }

    #[tokio::test]
    async fn segmentation_session_create_drop() {
        if !is_available() {
            return;
        }
        let session = SubjectSegmentationSessionHandle::new().await.unwrap();
        drop(session);
    }

    #[test]
    fn sequential_ocr_sessions() {
        if !is_available() {
            return;
        }
        for _ in 0..3 {
            let session = TextRecognitionSessionHandle::new().unwrap();
            drop(session);
        }
    }

    #[tokio::test]
    async fn sequential_segmentation_sessions() {
        if !is_available() {
            return;
        }
        for _ in 0..3 {
            let session = SubjectSegmentationSessionHandle::new().await.unwrap();
            drop(session);
        }
    }

    #[tokio::test]
    async fn interleaved_ocr_and_segmentation() {
        if !is_available() {
            return;
        }
        for _ in 0..2 {
            let ocr = TextRecognitionSessionHandle::new().unwrap();
            drop(ocr);

            let seg = SubjectSegmentationSessionHandle::new().await.unwrap();
            drop(seg);
        }
    }

    #[tokio::test]
    async fn concurrent_ocr_and_segmentation() {
        if !is_available() {
            return;
        }
        let ((), ()) = tokio::join!(
            async {
                let session = TextRecognitionSessionHandle::new().unwrap();
                drop(session);
            },
            async {
                let session = SubjectSegmentationSessionHandle::new().await.unwrap();
                drop(session);
            }
        );
    }

    /// Prove that OCR works WITHOUT enabling vision's internal event loop,
    /// as long as an external GLib event loop pumps g_main_context_default().
    /// This simulates the production scenario where the NLP SDK already runs
    /// such a loop.
    #[tokio::test]
    async fn ocr_without_internal_event_loop() {
        if !is_available() {
            return;
        }

        // 1. Start a GLib event loop on the default context (simulates NLP SDK)
        let stop = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let stop2 = Arc::clone(&stop);
        let glib_thread = std::thread::spawn(move || unsafe {
            let glib = match libloading::Library::new("libglib-2.0.so") {
                Ok(l) => l,
                Err(_) => return,
            };
            let ctx_default: Symbol<unsafe extern "C" fn() -> *mut c_void> =
                glib.get(b"g_main_context_default\0").unwrap();
            let ctx_iteration: Symbol<unsafe extern "C" fn(*mut c_void, i32) -> i32> =
                glib.get(b"g_main_context_iteration\0").unwrap();
            let ctx = ctx_default();
            while !stop2.load(std::sync::atomic::Ordering::Relaxed) {
                ctx_iteration(ctx, 0); // may_block = FALSE
                std::thread::sleep(std::time::Duration::from_millis(5));
            }
        });

        // Give the loop a moment to start
        tokio::time::sleep(std::time::Duration::from_millis(50)).await;

        // 2. Create OCR session WITHOUT enabling internal event loop
        let vlib = lib().unwrap();
        let session: *mut TextRecognitionSession = unsafe {
            let f: Symbol<unsafe extern "C" fn() -> *mut TextRecognitionSession> = vlib
                .get(b"text_recognition_create_session\0")
                .unwrap();
            f()
        };
        assert!(!session.is_null(), "create_session returned null");

        let ret: i32 = unsafe {
            let f: Symbol<unsafe extern "C" fn(*mut TextRecognitionSession) -> i32> = vlib
                .get(b"text_recognition_init_session\0")
                .unwrap();
            f(session)
        };
        assert_eq!(ret, 0, "init_session failed with code {ret}");

        // Deliberately NOT calling enable_internal_event_loop

        // 3. Set callback and run OCR
        let (tx, mut rx) = mpsc::unbounded_channel();
        let ctx = Arc::new(OcrCallbackContext { tx });
        let raw = Arc::into_raw(Arc::clone(&ctx));

        unsafe {
            let set_cb: Symbol<
                unsafe extern "C" fn(
                    *mut TextRecognitionSession,
                    TextRecognitionResultCallback,
                    *mut c_void,
                ),
            > = vlib
                .get(b"text_recognition_result_set_callback\0")
                .unwrap();
            set_cb(session, Some(ocr_result_callback), raw as *mut c_void);
        }

        let test_image = "/home/kylin/.kylinbot/workspace/lark_images/om_x100b6e609b1a58a8b2cdf9ac724fc8f_825c8fb2-cf46-48d3-8bbc-aed6382f9a81.jpg";
        if !std::path::Path::new(test_image).exists() {
            // cleanup and skip
            stop.store(true, std::sync::atomic::Ordering::Relaxed);
            let _ = glib_thread.join();
            unsafe { drop(Arc::from_raw(raw)) };
            drop(ctx);
            return;
        }

        let path_cstr = CString::new(test_image).unwrap();
        unsafe {
            let recognize: Symbol<
                unsafe extern "C" fn(*mut TextRecognitionSession, *const c_char),
            > = vlib
                .get(b"text_recognition_recognize_text_from_image_file_async\0")
                .unwrap();
            recognize(session, path_cstr.as_ptr());
        }

        // 4. Wait for callback — dispatched by our external GLib loop
        let result = tokio::time::timeout(std::time::Duration::from_secs(30), rx.recv()).await;

        // 5. Cleanup
        stop.store(true, std::sync::atomic::Ordering::Relaxed);
        let _ = glib_thread.join();
        unsafe { drop(Arc::from_raw(raw)) };
        drop(ctx);

        match result {
            Ok(Some(Ok(ocr))) => {
                eprintln!("OCR without internal event loop succeeded: {} chars", ocr.text.len());
            }
            Ok(Some(Err(e))) => {
                eprintln!("OCR returned error (still proves callback was dispatched): {e}");
            }
            Ok(None) => panic!("channel closed without result"),
            Err(_) => panic!("timeout — callback was NOT dispatched by external event loop"),
        }
    }

    /// Same as above but for subject segmentation: external GLib loop dispatches
    /// the init callback and the result callback.
    #[tokio::test]
    async fn segmentation_without_internal_event_loop() {
        if !is_available() {
            return;
        }

        // 1. Start external GLib event loop
        let stop = Arc::new(std::sync::atomic::AtomicBool::new(false));
        let stop2 = Arc::clone(&stop);
        let glib_thread = std::thread::spawn(move || unsafe {
            let glib = match libloading::Library::new("libglib-2.0.so") {
                Ok(l) => l,
                Err(_) => return,
            };
            let ctx_default: Symbol<unsafe extern "C" fn() -> *mut c_void> =
                glib.get(b"g_main_context_default\0").unwrap();
            let ctx_iteration: Symbol<unsafe extern "C" fn(*mut c_void, i32) -> i32> =
                glib.get(b"g_main_context_iteration\0").unwrap();
            let ctx = ctx_default();
            while !stop2.load(std::sync::atomic::Ordering::Relaxed) {
                ctx_iteration(ctx, 0);
                std::thread::sleep(std::time::Duration::from_millis(5));
            }
        });

        tokio::time::sleep(std::time::Duration::from_millis(50)).await;

        // 2. Create segmentation session WITHOUT internal event loop
        let vlib = lib().unwrap();
        let session: *mut SubjectSegmentationSession = unsafe {
            let f: Symbol<unsafe extern "C" fn() -> *mut SubjectSegmentationSession> = vlib
                .get(b"subject_segmentation_create_session\0")
                .unwrap();
            f()
        };
        assert!(!session.is_null(), "create_session returned null");

        // Deliberately NOT calling enable_internal_event_loop

        // 3. Async init — callback must be dispatched by external loop
        let (init_tx, mut init_rx) = mpsc::unbounded_channel::<Result<()>>();
        let init_ctx = Arc::new(InitCallbackContext { tx: init_tx });
        let init_raw = Arc::into_raw(Arc::clone(&init_ctx));

        unsafe {
            let init_async: Symbol<
                unsafe extern "C" fn(
                    *mut SubjectSegmentationSession,
                    SubjectSegmentationInitCallback,
                    *mut c_void,
                ),
            > = vlib
                .get(b"subject_segmentation_init_session_async\0")
                .unwrap();
            init_async(
                session,
                Some(segmentation_init_callback),
                init_raw as *mut c_void,
            );
        }

        let init_result =
            tokio::time::timeout(std::time::Duration::from_secs(30), init_rx.recv()).await;
        match init_result {
            Ok(Some(Ok(()))) => {
                eprintln!("Segmentation init callback dispatched by external loop");
            }
            Ok(Some(Err(e))) => {
                eprintln!("Segmentation init error (callback was dispatched): {e}");
                stop.store(true, std::sync::atomic::Ordering::Relaxed);
                let _ = glib_thread.join();
                unsafe { drop(Arc::from_raw(init_raw)) };
                drop(init_ctx);
                return;
            }
            _ => {
                stop.store(true, std::sync::atomic::Ordering::Relaxed);
                let _ = glib_thread.join();
                unsafe { drop(Arc::from_raw(init_raw)) };
                drop(init_ctx);
                panic!("init timeout — callback was NOT dispatched by external event loop");
            }
        }

        // 4. Run segmentation on a real image
        let test_image = "/home/kylin/.kylinbot/workspace/lark_images/om_x100b6e609b1a58a8b2cdf9ac724fc8f_825c8fb2-cf46-48d3-8bbc-aed6382f9a81.jpg";
        if !std::path::Path::new(test_image).exists() {
            stop.store(true, std::sync::atomic::Ordering::Relaxed);
            let _ = glib_thread.join();
            unsafe { drop(Arc::from_raw(init_raw)) };
            drop(init_ctx);
            return;
        }

        // Set config (general mode)
        unsafe {
            let create_config: Symbol<unsafe extern "C" fn() -> *mut SubjectSegmentationConfig> =
                vlib.get(b"subject_segmentation_config_create\0").unwrap();
            let config_ptr = create_config();
            if !config_ptr.is_null() {
                let set_mode: Symbol<
                    unsafe extern "C" fn(*mut SubjectSegmentationConfig, SubjectSegmentationMode),
                > = vlib
                    .get(b"subject_segmentation_config_set_mode\0")
                    .unwrap();
                set_mode(config_ptr, 1); // GENERAL
                let set_config: Symbol<
                    unsafe extern "C" fn(
                        *mut SubjectSegmentationSession,
                        *mut SubjectSegmentationConfig,
                    ),
                > = vlib.get(b"subject_segmentation_set_config\0").unwrap();
                set_config(session, config_ptr);
                let destroy_config: Symbol<
                    unsafe extern "C" fn(*mut *mut SubjectSegmentationConfig),
                > = vlib
                    .get(b"subject_segmentation_config_destroy\0")
                    .unwrap();
                let mut cp = config_ptr;
                destroy_config(&mut cp);
            }
        }

        // Set result callback
        let (res_tx, mut res_rx) = mpsc::unbounded_channel();
        let res_ctx = Arc::new(SegmentationCallbackContext { tx: res_tx });
        let res_raw = Arc::into_raw(Arc::clone(&res_ctx));

        unsafe {
            let set_cb: Symbol<
                unsafe extern "C" fn(
                    *mut SubjectSegmentationSession,
                    SubjectSegmentationResultCallback,
                    *mut c_void,
                ),
            > = vlib
                .get(b"subject_segmentation_result_set_callback\0")
                .unwrap();
            set_cb(
                session,
                Some(segmentation_result_callback),
                res_raw as *mut c_void,
            );
        }

        let path_cstr = CString::new(test_image).unwrap();
        unsafe {
            let run: Symbol<
                unsafe extern "C" fn(
                    *mut SubjectSegmentationSession,
                    *const c_char,
                    *const c_char,
                ),
            > = vlib
                .get(b"subject_segmentation_run_from_image_file_async\0")
                .unwrap();
            run(session, std::ptr::null(), path_cstr.as_ptr());
        }

        let seg_result =
            tokio::time::timeout(std::time::Duration::from_secs(120), res_rx.recv()).await;

        // 5. Cleanup
        stop.store(true, std::sync::atomic::Ordering::Relaxed);
        let _ = glib_thread.join();
        unsafe { drop(Arc::from_raw(init_raw)) };
        drop(init_ctx);
        unsafe { drop(Arc::from_raw(res_raw)) };
        drop(res_ctx);

        match seg_result {
            Ok(Some(Ok(seg))) => {
                eprintln!(
                    "Segmentation without internal event loop succeeded: {}x{}, {} bytes",
                    seg.width, seg.height, seg.foreground_data.len()
                );
            }
            Ok(Some(Err(e))) => {
                eprintln!("Segmentation error (callback was dispatched): {e}");
            }
            Ok(None) => panic!("channel closed without result"),
            Err(_) => panic!("timeout — result callback was NOT dispatched by external event loop"),
        }
    }
}
