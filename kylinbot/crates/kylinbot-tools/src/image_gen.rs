use async_trait::async_trait;
use kylinbot_api::tool::{Tool, ToolResult};
use kylinbot_config::policy::SecurityPolicy;
use kylinbot_config::policy::ToolOperation;
use std::path::PathBuf;
use std::sync::Arc;

use kylinbot_config::schema::{FalImageGenConfig, ImageGenConfig, KartImageGenConfig};

/// Image generation request configuration.
#[derive(Debug, Clone, Default)]
pub struct ImageGenRequest {
    pub style: Option<String>,
    pub width: Option<u32>,
    pub height: Option<u32>,
}

/// Errors from image generation providers.
#[derive(Debug, thiserror::Error)]
pub enum ImageGenError {
    #[error("Provider {0} is not available")]
    ProviderUnavailable(String),
    #[error("Generation failed: {reason}")]
    GenerationFailed { provider: String, reason: String },
    #[error("Invalid input: {0}")]
    InvalidInput(String),
}

/// Generated image result.
#[derive(Debug)]
pub struct GeneratedImage {
    pub path: PathBuf,
    pub format: String,
    pub width: u32,
    pub height: u32,
}

/// Unified trait for image generation providers.
#[async_trait]
pub trait ImageGenProvider: Send + Sync {
    async fn generate(
        &self,
        prompt: &str,
        request: ImageGenRequest,
        workspace_dir: &PathBuf,
    ) -> Result<GeneratedImage, ImageGenError>;
    fn name(&self) -> &str;
}

/// fal.ai provider implementation.
mod fal_provider {
    use super::*;
    use async_trait::async_trait;
    use reqwest::Client;
    use serde_json::json;
    use tokio::time::Duration;

    pub struct FalImageGenProvider {
        config: FalImageGenConfig,
        client: Client,
    }

    impl FalImageGenProvider {
        pub fn new(config: FalImageGenConfig) -> Self {
            let client = Client::builder()
                .timeout(Duration::from_secs(120))
                .build()
                .unwrap_or_default();
            Self { config, client }
        }

        fn read_api_key(env_var: &str) -> Result<String, String> {
            std::env::var(env_var)
                .map(|v| v.trim().to_string())
                .ok()
                .filter(|v| !v.is_empty())
                .ok_or_else(|| format!("Missing API key: set the {env_var} environment variable"))
        }
    }

    #[async_trait]
    impl ImageGenProvider for FalImageGenProvider {
        async fn generate(
            &self,
            prompt: &str,
            _request: ImageGenRequest,
            workspace_dir: &PathBuf,
        ) -> Result<GeneratedImage, ImageGenError> {
            let api_key = Self::read_api_key(&self.config.api_key_env)
                .map_err(|e| ImageGenError::InvalidInput(e))?;

            let url = format!("https://fal.run/{}", self.config.default_model);

            let body = json!({
                "prompt": prompt,
                "image_size": "square_hd",
                "num_images": 1
            });

            let resp = self
                .client
                .post(&url)
                .header("Authorization", format!("Key {api_key}"))
                .header("Content-Type", "application/json")
                .json(&body)
                .send()
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: e.to_string(),
                })?;

            if !resp.status().is_success() {
                let status = resp.status();
                let body_text = resp.text().await.unwrap_or_default();
                return Err(ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("API error ({}): {}", status, body_text),
                });
            }

            let resp_json: serde_json::Value = resp
                .json()
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("Failed to parse response: {}", e),
                })?;

            let image_url = resp_json
                .pointer("/images/0/url")
                .and_then(|v| v.as_str())
                .ok_or_else(|| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: "No image URL in response".into(),
                })?;

            let img_resp = self
                .client
                .get(image_url)
                .send()
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("Failed to download image: {}", e),
                })?;

            if !img_resp.status().is_success() {
                return Err(ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("Failed to download image ({})", img_resp.status()),
                });
            }

            let bytes = img_resp
                .bytes()
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("Failed to read image bytes: {}", e),
                })?;

            let images_dir = workspace_dir.join("images");
            tokio::fs::create_dir_all(&images_dir)
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("Failed to create images directory: {}", e),
                })?;

            let output_path = images_dir.join(format!("{}.png", uuid::Uuid::new_v4()));

            tokio::fs::write(&output_path, &bytes)
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "fal".into(),
                    reason: format!("Failed to write image file: {}", e),
                })?;

            let (width, height) = resp_json
                .pointer("/images/0/width")
                .and_then(|w| w.as_u64())
                .zip(resp_json.pointer("/images/0/height").and_then(|h| h.as_u64()))
                .map(|(w, h)| (w as u32, h as u32))
                .unwrap_or((0, 0));

            Ok(GeneratedImage {
                path: output_path,
                format: "png".into(),
                width,
                height,
            })
        }

        fn name(&self) -> &str {
            "fal"
        }
    }
}

/// Kart Vision SDK provider implementation.
#[cfg(feature = "kart-vision")]
mod kart_provider {
    use super::*;
    use async_trait::async_trait;
    use kart_vision_sys::{ImageGenConfig as KartConfig, ImageSize};
    use std::path::PathBuf;

    pub struct KartImageGenProvider {
        config: KartImageGenConfig,
    }

    impl KartImageGenProvider {
        pub fn new(config: KartImageGenConfig) -> Self {
            Self { config }
        }
    }

    #[async_trait]
    impl ImageGenProvider for KartImageGenProvider {
        async fn generate(
            &self,
            prompt: &str,
            request: ImageGenRequest,
            workspace_dir: &PathBuf,
        ) -> Result<GeneratedImage, ImageGenError> {
            let mut session = kart_vision_sys::ImageGenerationSession::new()
                .map_err(|e| ImageGenError::ProviderUnavailable(format!("kart-vision-sys: {}", e)))?;

            let has_style = request.style.is_some() || self.config.style.is_some();
            let has_size = request.width.is_some()
                || request.height.is_some()
                || self.config.width.is_some()
                || self.config.height.is_some();

            let mut cfg = KartConfig::default();
            cfg.number = Some(1);

            // Style
            if request.style.is_some() {
                cfg.style = request.style;
            } else if let Some(ref style) = self.config.style {
                cfg.style = Some(style.clone());
            }

            // Size: always set a valid size — SDK has no usable default
            {
                let supported = session.supported_sizes().unwrap_or_default();
                if has_size {
                    let w = request.width.or(self.config.width).unwrap_or(1024) as i32;
                    let h = request.height.or(self.config.height).unwrap_or(1024) as i32;
                    if supported.is_empty() || supported.iter().any(|s| s.width == w && s.height == h) {
                        cfg.size = Some(ImageSize { width: w, height: h });
                    } else {
                        tracing::warn!(
                            requested_w = w, requested_h = h,
                            "Requested size not supported, falling back to first supported size"
                        );
                        if let Some(s) = supported.first() {
                            cfg.size = Some(ImageSize { width: s.width, height: s.height });
                        }
                    }
                } else if let Some(s) = supported.first() {
                    cfg.size = Some(ImageSize { width: s.width, height: s.height });
                }
            }

            let output = session
                .generate(prompt, Some(cfg))
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "kart".into(),
                    reason: e.to_string(),
                })?;

            let images_dir = workspace_dir.join("images");
            tokio::fs::create_dir_all(&images_dir)
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "kart".into(),
                    reason: format!("Failed to create images directory: {}", e),
                })?;

            let ext = output.format.as_str();
            let output_path = images_dir.join(format!("{}.{ext}", uuid::Uuid::new_v4()));

            tokio::fs::write(&output_path, &output.data)
                .await
                .map_err(|e| ImageGenError::GenerationFailed {
                    provider: "kart".into(),
                    reason: format!("Failed to write image file: {}", e),
                })?;

            Ok(GeneratedImage {
                path: output_path,
                format: ext.into(),
                width: output.width,
                height: output.height,
            })
        }

        fn name(&self) -> &str {
            "kart"
        }
    }
}

/// Stub provider when kart-vision feature is not enabled.
#[cfg(not(feature = "kart-vision"))]
mod kart_provider {
    use super::*;
    use async_trait::async_trait;

    pub struct KartImageGenProvider;

    impl KartImageGenProvider {
        pub fn new() -> Self {
            Self
        }
    }

    impl Default for KartImageGenProvider {
        fn default() -> Self {
            Self::new()
        }
    }

    #[async_trait]
    impl ImageGenProvider for KartImageGenProvider {
        async fn generate(
            &self,
            _prompt: &str,
            _request: ImageGenRequest,
            _workspace_dir: &PathBuf,
        ) -> Result<GeneratedImage, ImageGenError> {
            Err(ImageGenError::ProviderUnavailable(
                "kart provider not available (enable kart-vision feature)".into(),
            ))
        }

        fn name(&self) -> &str {
            "kart"
        }
    }
}

/// Unified image generation tool supporting multiple providers (fal.ai, kart-vision SDK).
pub struct ImageGenTool {
    security: Arc<SecurityPolicy>,
    workspace_dir: PathBuf,
    provider: Box<dyn ImageGenProvider>,
}

impl ImageGenTool {
    pub fn new(
        security: Arc<SecurityPolicy>,
        workspace_dir: PathBuf,
        config: &ImageGenConfig,
    ) -> anyhow::Result<Self> {
        let provider: Box<dyn ImageGenProvider> = match config.provider.as_str() {
            "fal" => {
                let fal_config = config.fal.clone().unwrap_or_else(|| FalImageGenConfig {
                    default_model: "fal-ai/flux/schnell".into(),
                    api_key_env: "FAL_API_KEY".into(),
                });
                Box::new(fal_provider::FalImageGenProvider::new(fal_config))
            }
            "kart" => {
                #[cfg(feature = "kart-vision")]
                {
                    let kart_config = config.kart.clone().unwrap_or_default();
                    Box::new(kart_provider::KartImageGenProvider::new(kart_config))
                }
                #[cfg(not(feature = "kart-vision"))]
                {
                    return Err(anyhow::anyhow!(
                        "kart provider not available (enable kart-vision feature)"
                    ));
                }
            }
            _ => {
                return Err(anyhow::anyhow!("Unknown image generation provider: {}", config.provider));
            }
        };

        Ok(Self {
            security,
            workspace_dir,
            provider,
        })
    }

    pub fn with_provider(
        security: Arc<SecurityPolicy>,
        workspace_dir: PathBuf,
        provider: Box<dyn ImageGenProvider>,
    ) -> Self {
        Self {
            security,
            workspace_dir,
            provider,
        }
    }
}

#[async_trait]
impl Tool for ImageGenTool {
    fn name(&self) -> &str {
        "image_gen"
    }

    fn description(&self) -> &str {
        "Generate an image from a text prompt. \
         Supports multiple providers (fal.ai, kart-vision SDK). \
         Saves the result to the workspace images directory and returns the file path."
    }

    fn parameters_schema(&self) -> serde_json::Value {
        serde_json::json!({
            "type": "object",
            "required": ["prompt"],
            "properties": {
                "prompt": {
                    "type": "string",
                    "description": "Text prompt describing the image to generate."
                },
                "filename": {
                    "type": "string",
                    "description": "Output filename without extension (default: 'generated_image'). Saved as PNG in workspace/images/."
                },
                "style": {
                    "type": "string",
                    "description": "Image style (kart provider only, e.g., '水彩', '油画')."
                },
                "width": {
                    "type": "integer",
                    "description": "Image width in pixels (kart provider only)."
                },
                "height": {
                    "type": "integer",
                    "description": "Image height in pixels (kart provider only)."
                }
            }
        })
    }

    async fn execute(&self, args: serde_json::Value) -> anyhow::Result<ToolResult> {
        if let Err(error) = self
            .security
            .enforce_tool_operation(ToolOperation::Act, "image_gen")
        {
            return Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(error),
            });
        }

        let prompt = match args.get("prompt").and_then(|v| v.as_str()) {
            Some(p) if !p.trim().is_empty() => p.trim().to_string(),
            _ => {
                return Ok(ToolResult {
                    success: false,
                    output: String::new(),
                    error: Some("Missing required parameter: 'prompt'".into()),
                });
            }
        };

        let request = ImageGenRequest {
            style: args.get("style").and_then(|v| v.as_str().map(String::from)),
            width: args.get("width").and_then(|v| v.as_u64().map(|n| n as u32)),
            height: args.get("height").and_then(|v| v.as_u64().map(|n| n as u32)),
        };

        match self.provider.generate(&prompt, request, &self.workspace_dir).await {
            Ok(image) => {
                let size_kb = std::fs::metadata(&image.path)
                    .map(|m| m.len() as usize / 1024)
                    .unwrap_or(0);

                Ok(ToolResult {
                    success: true,
                    output: format!(
                        "Image generated successfully.\n\
                         File: {}\n\
                         Size: {} KB\n\
                         Format: {}\n\
                         Dimensions: {}x{}\n\
                         Provider: {}\n\
                         Prompt: {}",
                        image.path.display(),
                        size_kb,
                        image.format,
                        image.width,
                        image.height,
                        self.provider.name(),
                        prompt,
                    ),
                    error: None,
                })
            }
            Err(e) => Ok(ToolResult {
                success: false,
                output: String::new(),
                error: Some(e.to_string()),
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use kylinbot_config::autonomy::AutonomyLevel;
    use kylinbot_config::policy::SecurityPolicy;
    use std::sync::Arc;

    fn test_security() -> Arc<SecurityPolicy> {
        Arc::new(SecurityPolicy {
            autonomy: AutonomyLevel::Full,
            workspace_dir: std::env::temp_dir(),
            ..SecurityPolicy::default()
        })
    }

    #[test]
    fn tool_name() {
        let config = ImageGenConfig {
            enabled: true,
            provider: "fal".into(),
            fal: None,
            kart: None,
        };
        let tool = ImageGenTool::new(test_security(), std::env::temp_dir(), &config);
        assert!(tool.is_ok());
        let tool = tool.unwrap();
        assert_eq!(tool.name(), "image_gen");
    }

    #[test]
    fn tool_description_is_nonempty() {
        let config = ImageGenConfig {
            enabled: true,
            provider: "fal".into(),
            fal: None,
            kart: None,
        };
        let tool = ImageGenTool::new(test_security(), std::env::temp_dir(), &config);
        assert!(tool.is_ok());
        let tool = tool.unwrap();
        assert!(!tool.description().is_empty());
        assert!(tool.description().contains("image"));
    }

    #[test]
    fn tool_schema_has_required_prompt() {
        let config = ImageGenConfig {
            enabled: true,
            provider: "fal".into(),
            fal: None,
            kart: None,
        };
        let tool = ImageGenTool::new(test_security(), std::env::temp_dir(), &config);
        assert!(tool.is_ok());
        let tool = tool.unwrap();
        let schema = tool.parameters_schema();
        assert_eq!(schema["required"], serde_json::json!(["prompt"]));
        assert!(schema["properties"]["prompt"].is_object());
    }

    #[test]
    fn tool_spec_roundtrip() {
        let config = ImageGenConfig {
            enabled: true,
            provider: "fal".into(),
            fal: None,
            kart: None,
        };
        let tool = ImageGenTool::new(test_security(), std::env::temp_dir(), &config);
        assert!(tool.is_ok());
        let tool = tool.unwrap();
        let spec = tool.spec();
        assert_eq!(spec.name, "image_gen");
        assert!(spec.parameters.is_object());
    }
}