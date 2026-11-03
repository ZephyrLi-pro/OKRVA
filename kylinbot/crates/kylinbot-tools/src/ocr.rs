use async_trait::async_trait;
use serde_json::json;
use std::path::Path;
use std::sync::Arc;

use kylinbot_api::tool::{Tool, ToolResult};
use kylinbot_config::policy::SecurityPolicy;

pub struct OcrTool {
    security: Arc<SecurityPolicy>,
}

impl OcrTool {
    pub fn new(security: Arc<SecurityPolicy>) -> Self {
        Self { security }
    }
}

#[async_trait]
impl Tool for OcrTool {
    fn name(&self) -> &str {
        "ocr"
    }

    fn description(&self) -> &str {
        "Recognize text from an image file using on-device OCR. \
         Returns the full recognized text and each line with bounding box coordinates."
    }

    fn parameters_schema(&self) -> serde_json::Value {
        json!({
            "type": "object",
            "required": ["image_path"],
            "properties": {
                "image_path": {
                    "type": "string",
                    "description": "Path to the image file to recognize text from"
                }
            }
        })
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        if let Err(error) = self
            .security
            .enforce_tool_operation(kylinbot_config::policy::ToolOperation::Read, "ocr")
        {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(error),
            });
        }

        let image_path = match args.get("image_path").and_then(|v| v.as_str()) {
            Some(p) if !p.trim().is_empty() => p.trim().to_string(),
            _ => {
                return Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some("Missing required parameter: 'image_path'".into()),
                });
            }
        };

        if !Path::new(&image_path).exists() {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(format!("Image file not found: {image_path}")),
            });
        }

        #[cfg(feature = "kart-coreai-vision")]
        {
            if !kart_coreai_vision_sys::is_available() {
                return Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some(
                        "CoreAI Vision SDK not available — install libkysdk-coreai-vision"
                            .into(),
                    ),
                });
            }

            match kart_coreai_vision_sys::recognize_text(&image_path).await {
                Ok(result) => {
                    let lines_json: Vec<serde_json::Value> = result
                        .lines
                        .iter()
                        .map(|line| {
                            json!({
                                "text": line.text,
                                "corner_points": line.corner_points.iter()
                                    .map(|(x, y)| json!({"x": x, "y": y}))
                                    .collect::<Vec<_>>()
                            })
                        })
                        .collect();

                    let output = json!({
                        "text": result.text,
                        "lines": lines_json,
                        "line_count": result.lines.len(),
                    });

                    Ok(ToolResult {
                        success: true,
                        output: serde_json::to_string_pretty(&output)
                            .unwrap_or_else(|_| result.text),
                        error: None,
                    })
                }
                Err(e) => Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some(format!("OCR failed: {e}")),
                }),
            }
        }

        #[cfg(not(feature = "kart-coreai-vision"))]
        {
            Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(
                    "OCR not available — enable kart-coreai-vision feature".into(),
                ),
            })
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kylinbot_config::policy::SecurityPolicy;

    fn make_tool() -> OcrTool {
        let security = Arc::new(SecurityPolicy::default());
        OcrTool::new(security)
    }

    #[test]
    fn name_is_ocr() {
        assert_eq!(make_tool().name(), "ocr");
    }

    #[test]
    fn schema_requires_image_path() {
        let schema = make_tool().parameters_schema();
        let required = schema["required"].as_array().unwrap();
        assert!(required.iter().any(|v| v.as_str() == Some("image_path")));
    }

    #[tokio::test]
    async fn missing_image_path_returns_error() {
        let tool = make_tool();
        let result = tool.execute(serde_json::json!({})).await.unwrap();
        assert!(!result.success);
        assert!(result.error.unwrap().contains("image_path"));
    }

    #[tokio::test]
    async fn nonexistent_file_returns_error() {
        let tool = make_tool();
        let result = tool
            .execute(serde_json::json!({"image_path": "/tmp/nonexistent_image_file_12345.jpg"}))
            .await
            .unwrap();
        assert!(!result.success);
        assert!(result.error.unwrap().contains("not found"));
    }
}
