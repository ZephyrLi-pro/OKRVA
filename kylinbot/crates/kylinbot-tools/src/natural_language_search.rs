#[cfg(target_os = "linux")]
use std::ffi::{CStr, CString};
#[cfg(target_os = "linux")]
use std::os::raw::{c_char, c_int, c_uchar};
#[cfg(target_os = "linux")]
use std::sync::Arc;

use anyhow::Result;
#[cfg(target_os = "linux")]
use anyhow::{Context, anyhow, bail};
use async_trait::async_trait;
use kylinbot_api::tool::{Tool, ToolResult};
#[cfg(target_os = "linux")]
use libloading::Library;
use serde::{Deserialize, Serialize};
use serde_json::json;
#[cfg(target_os = "linux")]
use tokio::task;

const TOOL_NAME: &str = "natural_language_search";
const DEFAULT_LIMIT: u32 = 20;
const MAX_LIMIT: u32 = 100;

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct NaturalLanguageSearchHit {
    pub filepath: String,
    pub similarity: f64,
}

#[async_trait]
trait NaturalLanguageSearchClient {
    async fn search(&self, text: &str, limit: u32) -> Result<Vec<NaturalLanguageSearchHit>>;
}

pub struct NaturalLanguageSearchTool {
    client: Arc<dyn NaturalLanguageSearchClient + Send + Sync>,
    default_limit: u32,
    max_limit: u32,
}

impl NaturalLanguageSearchTool {
    pub fn new() -> Result<Self> {
        Ok(Self::with_client(DataManagementFfiClient::new()?))
    }

    fn with_client<C>(client: C) -> Self
    where
        C: NaturalLanguageSearchClient + Send + Sync + 'static,
    {
        Self {
            client: Arc::new(client),
            default_limit: DEFAULT_LIMIT,
            max_limit: MAX_LIMIT,
        }
    }
}

#[async_trait]
impl Tool for NaturalLanguageSearchTool {
    fn name(&self) -> &str {
        TOOL_NAME
    }

    fn description(&self) -> &str {
        "Search files indexed by the local data-management service across the user's indexed local files, not only the current workspace. Use this when the user asks to find documents, PDFs, Word files, images, or files by topic, filename meaning, time, format, size, or path conditions. Pass the user's natural-language search request through in the text argument without extracting or rewriting time, file format, file size, or path constraints; the data-management service performs that entity extraction and filtering itself. Do not translate Chinese queries, normalize relative dates, infer file extensions, or replace the user's wording with extracted keywords unless the user only provided a topic keyword such as \"AIPC\". Returned files should be treated as already satisfying those constraints even if the output only contains filepath and similarity. Do not use shell or filesystem tools to re-check size/format/path unless the user explicitly asks for filesystem verification."
    }

    fn parameters_schema(&self) -> serde_json::Value {
        json!({
            "type": "object",
            "properties": {
                "text": {
                    "type": "string",
                    "description": "Natural language file search query."
                },
                "limit": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": self.max_limit,
                    "default": self.default_limit,
                    "description": "Maximum number of search results."
                }
            },
            "required": ["text"]
        })
    }

    async fn execute(&self, args: serde_json::Value) -> Result<ToolResult> {
        let Some(text) = args.get("text").and_then(|value| value.as_str()) else {
            return Ok(error_result("Missing non-empty 'text' parameter"));
        };
        let text = text.trim();
        if text.is_empty() {
            return Ok(error_result("Missing non-empty 'text' parameter"));
        }

        let limit = match parse_limit(&args, self.default_limit, self.max_limit) {
            Ok(limit) => limit,
            Err(error) => return Ok(error_result(error)),
        };

        match self.client.search(text, limit).await {
            Ok(hits) => Ok(ToolResult {
                success: true,
                output: serde_json::to_string(&hits)?,
                error: None,
            }),
            Err(error) => Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(format!("{TOOL_NAME} failed: {error}")),
            }),
        }
    }
}

fn parse_limit(
    args: &serde_json::Value,
    default_limit: u32,
    max_limit: u32,
) -> std::result::Result<u32, String> {
    let Some(value) = args.get("limit") else {
        return Ok(default_limit);
    };
    let Some(limit) = value.as_u64() else {
        return Err("'limit' must be an integer".to_string());
    };
    if limit == 0 {
        return Err("'limit' must be at least 1".to_string());
    }
    if limit > u64::from(max_limit) {
        return Err(format!("'limit' must be at most {max_limit}"));
    }
    Ok(limit as u32)
}

fn error_result(error: impl Into<String>) -> ToolResult {
    ToolResult {
        success: false,
        output: String::new(),
        error: Some(error.into()),
    }
}

#[derive(Clone)]
struct DataManagementFfiClient {
    #[cfg(target_os = "linux")]
    inner: Arc<DataManagementFfi>,
}

impl DataManagementFfiClient {
    fn new() -> Result<Self> {
        #[cfg(target_os = "linux")]
        {
            return Ok(Self {
                inner: Arc::new(DataManagementFfi::load()?),
            });
        }

        #[cfg(not(target_os = "linux"))]
        {
            anyhow::bail!("data-management client is only supported on Linux")
        }
    }
}

#[async_trait]
impl NaturalLanguageSearchClient for DataManagementFfiClient {
    async fn search(&self, text: &str, limit: u32) -> Result<Vec<NaturalLanguageSearchHit>> {
        #[cfg(target_os = "linux")]
        {
            let ffi = self.inner.clone();
            let text = text.to_string();
            return task::spawn_blocking(move || ffi.search(&text, limit))
                .await
                .context("natural language search worker task failed")?;
        }

        #[cfg(not(target_os = "linux"))]
        {
            let _ = (text, limit);
            anyhow::bail!("data-management client is only supported on Linux")
        }
    }
}

#[cfg(target_os = "linux")]
#[repr(C)]
#[derive(Copy, Clone)]
struct DataManagementStatus {
    success: c_uchar,
    error_code: c_int,
    error_msg: [c_char; 500],
}

#[cfg(target_os = "linux")]
type DataManagementSession = std::ffi::c_void;
#[cfg(target_os = "linux")]
type CreateSessionFn = unsafe extern "C" fn() -> *mut DataManagementSession;
#[cfg(target_os = "linux")]
type DestroySessionFn = unsafe extern "C" fn(*mut DataManagementSession);
#[cfg(target_os = "linux")]
type DestroyResultFn = unsafe extern "C" fn(*mut c_char);
#[cfg(target_os = "linux")]
type NaturalLanguageSearchFn = unsafe extern "C" fn(
    *mut DataManagementSession,
    *const c_char,
    *mut *mut c_char,
) -> DataManagementStatus;

#[cfg(target_os = "linux")]
struct DataManagementFfi {
    _library: Library,
    create_session: CreateSessionFn,
    destroy_session: DestroySessionFn,
    destroy_result: DestroyResultFn,
    natural_language_search: NaturalLanguageSearchFn,
}

#[cfg(target_os = "linux")]
impl DataManagementFfi {
    fn load() -> Result<Self> {
        let library = load_data_management_library()?;
        let create_session =
            unsafe { load_symbol::<CreateSessionFn>(&library, b"data_management_create_session")? };
        let destroy_session = unsafe {
            load_symbol::<DestroySessionFn>(&library, b"data_management_destroy_session")?
        };
        let destroy_result =
            unsafe { load_symbol::<DestroyResultFn>(&library, b"data_management_destroy_result")? };
        let natural_language_search = unsafe {
            load_symbol::<NaturalLanguageSearchFn>(
                &library,
                b"data_management_natural_language_search",
            )?
        };

        Ok(Self {
            _library: library,
            create_session,
            destroy_session,
            destroy_result,
            natural_language_search,
        })
    }

    fn search(&self, text: &str, limit: u32) -> Result<Vec<NaturalLanguageSearchHit>> {
        let session = unsafe { (self.create_session)() };
        if session.is_null() {
            bail!("failed to create data-management session");
        }
        let _guard = SessionGuard {
            session,
            destroy_session: self.destroy_session,
        };

        let request = json!({
            "text": text,
            "limit": limit,
        })
        .to_string();
        let request = CString::new(request).context("request contains an interior NUL byte")?;
        let mut result: *mut c_char = std::ptr::null_mut();
        let status =
            unsafe { (self.natural_language_search)(session, request.as_ptr(), &mut result) };
        let _result_guard = ResultGuard {
            result,
            destroy_result: self.destroy_result,
        };

        if status.success == 0 {
            bail!(
                "data-management error {}: {}",
                status.error_code,
                status_message(&status)
            );
        }
        if result.is_null() {
            bail!("data-management returned a null result");
        }

        let result_text = unsafe { CStr::from_ptr(result) }
            .to_str()
            .context("data-management returned non-UTF-8 result")?;
        let hits: Vec<NaturalLanguageSearchHit> = serde_json::from_str(result_text)
            .context("failed to parse data-management search result")?;
        Ok(hits)
    }
}

#[cfg(target_os = "linux")]
fn load_data_management_library() -> Result<Library> {
    if let Ok(path) = std::env::var("KYAI_DATA_MANAGEMENT_CLIENT_LIB") {
        let library = unsafe { Library::new(&path) }
            .with_context(|| format!("failed to load KYAI_DATA_MANAGEMENT_CLIENT_LIB={path}"))?;
        return Ok(library);
    }

    let mut candidates = Vec::new();
    if let Some(triplet) = linux_gnu_triplet() {
        candidates.push(format!(
            "/usr/lib/{triplet}/libkyai-data-management-client.so.1"
        ));
        candidates.push(format!(
            "/usr/lib/{triplet}/libkyai-data-management-client.so"
        ));
    }
    candidates.push("libkyai-data-management-client.so.1".to_string());
    candidates.push("libkyai-data-management-client.so".to_string());

    let mut errors = Vec::new();
    for candidate in &candidates {
        match unsafe { Library::new(candidate) } {
            Ok(library) => return Ok(library),
            Err(error) => errors.push(format!("{candidate}: {error}")),
        }
    }
    Err(anyhow!(
        "failed to load libkyai-data-management-client ({})",
        errors.join("; ")
    ))
}

#[cfg(target_os = "linux")]
fn linux_gnu_triplet() -> Option<&'static str> {
    match std::env::consts::ARCH {
        "x86_64" => Some("x86_64-linux-gnu"),
        "aarch64" => Some("aarch64-linux-gnu"),
        "loongarch64" => Some("loongarch64-linux-gnu"),
        _ => None,
    }
}

#[cfg(target_os = "linux")]
unsafe fn load_symbol<T: Copy>(library: &Library, name: &[u8]) -> Result<T> {
    let symbol = unsafe { library.get::<T>(name) }
        .with_context(|| format!("failed to load symbol {}", String::from_utf8_lossy(name)))?;
    Ok(*symbol)
}

#[cfg(target_os = "linux")]
fn status_message(status: &DataManagementStatus) -> String {
    let nul = status
        .error_msg
        .iter()
        .position(|&ch| ch == 0)
        .unwrap_or(status.error_msg.len());
    let bytes: Vec<u8> = status.error_msg[..nul].iter().map(|&ch| ch as u8).collect();
    String::from_utf8_lossy(&bytes).into_owned()
}

#[cfg(target_os = "linux")]
struct SessionGuard {
    session: *mut DataManagementSession,
    destroy_session: DestroySessionFn,
}

#[cfg(target_os = "linux")]
impl Drop for SessionGuard {
    fn drop(&mut self) {
        unsafe {
            (self.destroy_session)(self.session);
        }
    }
}

#[cfg(target_os = "linux")]
struct ResultGuard {
    result: *mut c_char,
    destroy_result: DestroyResultFn,
}

#[cfg(target_os = "linux")]
impl Drop for ResultGuard {
    fn drop(&mut self) {
        if !self.result.is_null() {
            unsafe {
                (self.destroy_result)(self.result);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Clone)]
    struct MockClient {
        result: std::result::Result<Vec<NaturalLanguageSearchHit>, String>,
    }

    #[async_trait]
    impl NaturalLanguageSearchClient for MockClient {
        async fn search(&self, _text: &str, _limit: u32) -> Result<Vec<NaturalLanguageSearchHit>> {
            self.result.clone().map_err(anyhow::Error::msg)
        }
    }

    #[tokio::test]
    async fn execute_returns_hits_as_json_array() {
        let tool = NaturalLanguageSearchTool::with_client(MockClient {
            result: Ok(vec![NaturalLanguageSearchHit {
                filepath: "/home/kylin/Documents/example.docx".to_string(),
                similarity: 2.0,
            }]),
        });

        let result = tool
            .execute(json!({"text": "word文档", "limit": 10}))
            .await
            .unwrap();

        assert!(result.success);
        assert!(result.error.is_none());
        assert_eq!(
            serde_json::from_str::<serde_json::Value>(&result.output).unwrap(),
            json!([{"filepath": "/home/kylin/Documents/example.docx", "similarity": 2.0}])
        );
    }

    #[tokio::test]
    async fn execute_rejects_empty_text() {
        let tool = NaturalLanguageSearchTool::with_client(MockClient { result: Ok(vec![]) });

        let result = tool.execute(json!({"text": "  "})).await.unwrap();

        assert!(!result.success);
        assert_eq!(
            result.error.as_deref(),
            Some("Missing non-empty 'text' parameter")
        );
    }

    #[tokio::test]
    async fn execute_rejects_limit_over_max() {
        let tool = NaturalLanguageSearchTool::with_client(MockClient { result: Ok(vec![]) });

        let result = tool
            .execute(json!({"text": "合同", "limit": 101}))
            .await
            .unwrap();

        assert!(!result.success);
        assert_eq!(result.error.as_deref(), Some("'limit' must be at most 100"));
    }

    #[test]
    fn schema_matches_tool_contract() {
        let tool = NaturalLanguageSearchTool::with_client(MockClient { result: Ok(vec![]) });
        let schema = tool.parameters_schema();

        assert_eq!(schema["required"], json!(["text"]));
        assert_eq!(schema["properties"]["limit"]["default"], json!(20));
        assert_eq!(schema["properties"]["limit"]["maximum"], json!(100));
    }
}
