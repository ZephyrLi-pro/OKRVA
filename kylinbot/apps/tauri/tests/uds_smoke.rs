//! Smoke test: spin up the gateway on a UDS socket and verify the Tauri
//! `UdsClient` can reach `/health`.

#[cfg(unix)]
mod uds {
    use std::path::PathBuf;
    use std::time::Duration;

    use kylinbot_config::schema::{Config, ModelProviderConfig, Transport};
    use kylinbot_gateway::{GatewayBindOptions, run_gateway};
    use tempfile::TempDir;

    fn uds_test_config(temp_dir: &std::path::Path) -> Config {
        let workspace_dir = temp_dir.join("workspace");
        std::fs::create_dir_all(&workspace_dir).expect("workspace dir");

        let socket_path = temp_dir.join("run").join("gateway.sock");
        let mut config = Config {
            workspace_dir: workspace_dir.clone(),
            config_path: temp_dir.join("config.toml"),
            ..Config::default()
        };
        config.gateway.transport = Transport::Uds;
        config.gateway.socket_path = Some(socket_path);
        config.gateway.require_pairing = false;
        config.gateway.session_persistence = false;
        config.providers.fallback = Some("openrouter".into());
        config.providers.models.insert(
            "openrouter".into(),
            ModelProviderConfig {
                model: Some("test-model".into()),
                ..ModelProviderConfig::default()
            },
        );
        config
    }

    async fn wait_for_socket(socket_path: &std::path::Path) {
        let deadline = tokio::time::Instant::now() + Duration::from_secs(10);
        while tokio::time::Instant::now() < deadline {
            if socket_path.exists() {
                return;
            }
            tokio::time::sleep(Duration::from_millis(50)).await;
        }
        panic!(
            "gateway socket was not created at {}",
            socket_path.display()
        );
    }

    async fn shutdown_gateway(socket_path: &std::path::Path) {
        use tokio::io::{AsyncReadExt, AsyncWriteExt};
        use tokio::net::UnixStream;

        let mut stream = UnixStream::connect(socket_path)
            .await
            .expect("connect for shutdown");
        let request = "POST /admin/shutdown HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        stream
            .write_all(request.as_bytes())
            .await
            .expect("write shutdown");
        let mut response = String::new();
        let _ = tokio::time::timeout(Duration::from_secs(5), stream.read_to_string(&mut response))
            .await;
    }

    #[tokio::test]
    async fn uds_client_health_via_gateway() {
        let temp = TempDir::new().expect("temp dir");
        let config = uds_test_config(temp.path());
        let socket_path = config.gateway.socket_path.clone().expect("socket path");

        let parent = socket_path.parent().expect("socket parent");
        std::fs::create_dir_all(parent).expect("socket parent dir");

        let bind = GatewayBindOptions::from_config(&config);
        let gateway = tokio::spawn(async move { run_gateway(bind, config, None).await });

        wait_for_socket(&socket_path).await;

        // Use the Tauri-side UdsClient to call /health.
        let transport = kylinbot_desktop::state::DesktopTransport::Uds {
            socket_path: socket_path.clone(),
        };
        let client = kylinbot_desktop::gateway_client::GatewayClient::new(&transport, None);

        let healthy = client.get_health().await.expect("health call");
        assert!(healthy, "gateway /health should return success");

        shutdown_gateway(&socket_path).await;

        let _ = tokio::time::timeout(Duration::from_secs(10), gateway)
            .await
            .expect("gateway shutdown timeout");
    }

    #[tokio::test]
    async fn uds_client_requires_pairing_is_false() {
        let temp = TempDir::new().expect("temp dir");
        let config = uds_test_config(temp.path());
        let socket_path = config.gateway.socket_path.clone().expect("socket path");

        let parent = socket_path.parent().expect("socket parent");
        std::fs::create_dir_all(parent).expect("socket parent dir");

        let bind = GatewayBindOptions::from_config(&config);
        let gateway = tokio::spawn(async move { run_gateway(bind, config, None).await });

        wait_for_socket(&socket_path).await;

        let transport = kylinbot_desktop::state::DesktopTransport::Uds {
            socket_path: socket_path.clone(),
        };
        let client = kylinbot_desktop::gateway_client::GatewayClient::new(&transport, None);

        let requires = client
            .requires_pairing()
            .await
            .expect("requires_pairing call");
        assert!(
            !requires,
            "gateway should not require pairing with require_pairing=false"
        );

        shutdown_gateway(&socket_path).await;

        let _ = tokio::time::timeout(Duration::from_secs(10), gateway)
            .await
            .expect("gateway shutdown timeout");
    }

    #[tokio::test]
    async fn uds_client_connect_failure_is_handled() {
        let transport = kylinbot_desktop::state::DesktopTransport::Uds {
            socket_path: PathBuf::from("/tmp/nonexistent_uds_smoke_test.sock"),
        };
        let client = kylinbot_desktop::gateway_client::GatewayClient::new(&transport, None);
        let healthy = client
            .get_health()
            .await
            .expect("health call should not panic");
        assert!(!healthy, "health should be false for missing socket");
    }
}
