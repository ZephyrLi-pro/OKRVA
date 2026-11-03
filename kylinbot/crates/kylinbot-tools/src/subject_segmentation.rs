use async_trait::async_trait;
use serde_json::json;
use std::path::{Path, PathBuf};
use std::sync::Arc;

use kylinbot_api::tool::{Tool, ToolResult};
use kylinbot_config::policy::SecurityPolicy;

pub struct SubjectSegmentationTool {
    security: Arc<SecurityPolicy>,
    #[allow(dead_code)]
    workspace_dir: PathBuf,
}

impl SubjectSegmentationTool {
    pub fn new(security: Arc<SecurityPolicy>, workspace_dir: PathBuf) -> Self {
        Self {
            security,
            workspace_dir,
        }
    }
}

#[async_trait]
impl Tool for SubjectSegmentationTool {
    fn name(&self) -> &str {
        "subject_segmentation"
    }

    fn description(&self) -> &str {
        "Extract the foreground subject from an image using on-device segmentation. \
         Supports portrait and general modes. Saves the result as a transparent-background PNG."
    }

    fn parameters_schema(&self) -> serde_json::Value {
        json!({
            "type": "object",
            "required": ["image_path"],
            "properties": {
                "image_path": {
                    "type": "string",
                    "description": "Path to the image file to segment"
                },
                "mode": {
                    "type": "string",
                    "enum": ["portrait", "general"],
                    "description": "Segmentation mode: 'portrait' for human subjects, 'general' for any object. Default: 'general'"
                }
            }
        })
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        if let Err(error) = self
            .security
            .enforce_tool_operation(kylinbot_config::policy::ToolOperation::Act, "subject_segmentation")
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

        let mode_str = args
            .get("mode")
            .and_then(|v| v.as_str())
            .unwrap_or("general");

        #[cfg(feature = "kart-coreai-vision")]
        {
            use kart_coreai_vision_sys::SegmentationMode;

            let mode = match mode_str {
                "portrait" => SegmentationMode::Portrait,
                "general" => SegmentationMode::General,
                other => {
                    return Ok(ToolResult {
                        success: false,
                        output: String::new(),
                        error: Some(format!(
                            "Invalid mode '{other}': must be 'portrait' or 'general'"
                        )),
                    });
                }
            };

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

            match kart_coreai_vision_sys::segment_subject(&image_path, mode).await {
                Ok(result) => {
                    // Determine output path
                    let source = Path::new(&image_path);
                    let stem = source
                        .file_stem()
                        .and_then(|s| s.to_str())
                        .unwrap_or("output");

                    let images_dir = self.workspace_dir.join("images");
                    if let Err(e) = tokio::fs::create_dir_all(&images_dir).await {
                        return Ok(ToolResult {
                            success: false,
                            output: String::new(),
                            error: Some(format!("Failed to create images directory: {e}")),
                        });
                    }

                    let output_path = images_dir.join(format!("{stem}_segmented.png"));

                    if let Err(e) = tokio::fs::write(&output_path, &result.foreground_data).await {
                        return Ok(ToolResult {
                            success: false,
                            output: String::new(),
                            error: Some(format!("Failed to save segmented image: {e}")),
                        });
                    }

                    Ok(ToolResult {
                        success: true,
                        output: format!(
                            "Subject segmentation complete.\n\
                             File: {}\n\
                             Format: {}\n\
                             Dimensions: {}x{}\n\
                             Mode: {}",
                            output_path.display(),
                            result.format,
                            result.width,
                            result.height,
                            mode_str,
                        ),
                        error: None,
                    })
                }
                Err(e) => Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some(format!("Segmentation failed: {e}")),
                }),
            }
        }

        #[cfg(not(feature = "kart-coreai-vision"))]
        {
            let _ = mode_str;
            Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(
                    "Subject segmentation not available — enable kart-coreai-vision feature"
                        .into(),
                ),
            })
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kylinbot_config::policy::SecurityPolicy;

    fn make_tool() -> SubjectSegmentationTool {
        let security = Arc::new(SecurityPolicy::default());
        SubjectSegmentationTool::new(security, PathBuf::from("/tmp/kylinbot-test"))
    }

    #[test]
    fn name_is_subject_segmentation() {
        assert_eq!(make_tool().name(), "subject_segmentation");
    }

    #[test]
    fn schema_requires_image_path() {
        let schema = make_tool().parameters_schema();
        let required = schema["required"].as_array().unwrap();
        assert!(required.iter().any(|v| v.as_str() == Some("image_path")));
    }

    #[test]
    fn schema_has_optional_mode() {
        let schema = make_tool().parameters_schema();
        let mode = &schema["properties"]["mode"];
        let mode_enum = mode["enum"].as_array().unwrap();
        assert!(mode_enum.iter().any(|v| v.as_str() == Some("portrait")));
        assert!(mode_enum.iter().any(|v| v.as_str() == Some("general")));
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
