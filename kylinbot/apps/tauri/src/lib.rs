//! KylinBot Desktop — Tauri application library.

#[cfg(target_os = "windows")]
compile_error!("kylinbot-desktop is not supported on Windows; supported platforms: Linux, macOS");

pub mod commands;
pub mod gateway_client;
pub mod health;
#[cfg(unix)]
pub mod proxy;
pub mod state;
pub mod transport;
pub mod tray;

use gateway_client::GatewayClient;
use state::shared_state;
use tauri::{Manager, RunEvent};

/// Attempt to auto-pair with the gateway so the WebView has a valid token
/// before the React frontend mounts. Runs on localhost so the admin endpoints
/// are accessible without auth.
async fn auto_pair(state: &state::SharedState) -> Option<String> {
    let transport = {
        let s = state.read().await;
        s.transport.clone()
    };

    let client = GatewayClient::new(&transport, None);

    // Check if gateway is reachable and requires pairing.
    if !client.requires_pairing().await.unwrap_or(false) {
        return None; // Pairing disabled — no token needed.
    }

    // Check if we already have a valid token in state.
    {
        let s = state.read().await;
        if let Some(ref token) = s.token {
            let authed = GatewayClient::new(&transport, Some(token));
            if authed.validate_token().await.unwrap_or(false) {
                return Some(token.clone()); // Existing token is valid.
            }
        }
    }

    // No valid token — auto-pair by requesting a new code and exchanging it.
    let client = GatewayClient::new(&transport, None);
    match client.auto_pair().await {
        Ok(token) => {
            let mut s = state.write().await;
            s.token = Some(token.clone());
            Some(token)
        }
        Err(_) => None, // Gateway may not be ready yet; health poller will retry.
    }
}

/// Inject a bearer token into the WebView's localStorage so the React app
/// skips the pairing dialog. Uses Tauri's WebviewWindow scripting API.
fn inject_token_into_webview<R: tauri::Runtime>(window: &tauri::WebviewWindow<R>, token: &str) {
    let escaped = token.replace('\\', "\\\\").replace('\'', "\\'");
    let script = format!("localStorage.setItem('kylinbot_token', '{escaped}')");
    let _ = window.eval(&script);
}

/// Set the macOS dock icon programmatically so it shows even in dev builds
/// (which don't have a proper .app bundle).
#[cfg(target_os = "macos")]
fn set_dock_icon() {
    use objc2::{AnyThread, MainThreadMarker};
    use objc2_app_kit::NSApplication;
    use objc2_app_kit::NSImage;
    use objc2_foundation::NSData;

    let icon_bytes = include_bytes!("../icons/128x128.png");
    let mtm = unsafe { MainThreadMarker::new_unchecked() };
    let data = NSData::with_bytes(icon_bytes);
    if let Some(image) = NSImage::initWithData(NSImage::alloc(), &data) {
        let app = NSApplication::sharedApplication(mtm);
        unsafe { app.setApplicationIconImage(Some(&image)) };
    }
}

/// Register the `kylinbot://localhost/` custom URI scheme protocol on Unix platforms.
/// This proxies WebView HTTP requests to the gateway via Unix domain socket.
#[cfg(unix)]
fn register_kylinbot_protocol(builder: tauri::Builder<tauri::Wry>) -> tauri::Builder<tauri::Wry> {
    builder.register_asynchronous_uri_scheme_protocol("kylinbot", move |ctx, request, responder| {
        // Read transport from the shared state via the app handle.
        let app_handle = ctx.app_handle().clone();
        let state: tauri::State<'_, state::SharedState> = app_handle.state::<state::SharedState>();
        let transport = {
            let s = state.blocking_read();
            s.transport.clone()
        };

        // WebSocket upgrade requests cannot be carried through the WKURLSchemeHandler /
        // WebKitGTK custom URI scheme — they're HTTP-only. Short-circuit with an
        // informative 400 so callers don't see a silent failure or 1006 disconnect.
        if request
            .headers()
            .get(http::header::UPGRADE)
            .and_then(|v| v.to_str().ok())
            .is_some_and(|v| v.eq_ignore_ascii_case("websocket"))
        {
            responder.respond(
                http::Response::builder()
                    .status(http::StatusCode::BAD_REQUEST)
                    .body(
                        b"WebSocket upgrades over kylinbot:// are not supported. \
                          Use the ws_open Tauri command (web/src/lib/uds-ws.ts)."
                            .to_vec(),
                    )
                    .unwrap(),
            );
            return;
        }

        let client = transport::build_client(&transport);

        // Rewrite the URI: kylinbot://localhost/path → /path
        let uri = request.uri().clone();
        let path_and_query = uri
            .path_and_query()
            .map(|pq| pq.as_str().to_string())
            .unwrap_or("/".to_string());

        let mut req_builder = http::Request::builder()
            .method(request.method().clone())
            .uri(path_and_query);

        for (name, value) in request.headers().iter() {
            if name != http::header::HOST {
                req_builder = req_builder.header(name, value);
            }
        }

        let body = bytes::Bytes::copy_from_slice(request.body());
        let http_req = req_builder.body(body).unwrap();

        // Spawn async task to handle the request
        tauri::async_runtime::spawn(async move {
            match client.request(http_req).await {
                Ok(resp) => {
                    let status = resp.status();
                    let mut builder = http::Response::builder().status(status);
                    for (name, value) in resp.headers().iter() {
                        if let Ok(h_name) = http::HeaderName::from_bytes(name.as_str().as_bytes()) {
                            builder = builder.header(h_name, value);
                        }
                    }
                    responder.respond(builder.body(resp.into_body().to_vec()).unwrap());
                }
                Err(e) => {
                    tracing::warn!("kylinbot protocol proxy error: {e}");
                    responder.respond(
                        http::Response::builder()
                            .status(http::StatusCode::BAD_GATEWAY)
                            .body(format!("gateway unreachable: {e}").into_bytes())
                            .unwrap(),
                    );
                }
            }
        });
    })
}

/// No-op on non-Unix (Windows is blocked by compile_error, but this satisfies the type system).
#[cfg(not(unix))]
fn register_kylinbot_protocol(builder: tauri::Builder<tauri::Wry>) -> tauri::Builder<tauri::Wry> {
    builder
}

/// Configure and run the Tauri application.
pub fn run() {
    let shared = shared_state();

    let builder = tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_store::Builder::default().build())
        .plugin(tauri_plugin_single_instance::init(|app, _args, _cwd| {
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.show();
                let _ = window.set_focus();
            }
        }))
        .manage(shared.clone());

    #[cfg(unix)]
    let builder = builder.manage(std::sync::Arc::new(commands::ws::WsRegistry::new()));

    let builder = builder.invoke_handler(tauri::generate_handler![
        commands::gateway::get_status,
        commands::gateway::get_health,
        commands::channels::list_channels,
        commands::pairing::initiate_pairing,
        commands::pairing::get_devices,
        commands::agent::send_message,
        #[cfg(unix)]
        commands::ws::ws_open,
        #[cfg(unix)]
        commands::ws::ws_send,
        #[cfg(unix)]
        commands::ws::ws_close,
    ]);

    let builder = register_kylinbot_protocol(builder);

    builder
        .setup(move |app| {
            #[cfg(target_os = "macos")]
            set_dock_icon();

            let _ = tray::setup_tray(app);

            // Override default transport with persisted preference, if any.
            // Must happen BEFORE the proxy spawns so the proxy sees the right transport.
            if let Some(persisted) = state::load_persisted_transport(app.handle()) {
                tauri::async_runtime::block_on(async {
                    shared.write().await.transport = persisted;
                });
            }

            // Start the localhost TCP→UDS proxy before the WebView starts
            // loading `frontendDist` (= http://127.0.0.1:<PROXY_PORT>/_app/).
            // We block_on the bind so the port is open before the WebView's
            // first request; the accept loop runs as a background task.
            #[cfg(unix)]
            {
                let transport_for_proxy =
                    tauri::async_runtime::block_on(async { shared.read().await.transport.clone() });
                let listener = match tauri::async_runtime::block_on(proxy::bind()) {
                    Ok(l) => l,
                    Err(e) => {
                        tracing::error!(
                            "{}",
                            proxy::explain_bind_error(
                                &e,
                                match &transport_for_proxy {
                                    state::DesktopTransport::Uds { socket_path } =>
                                        Some(socket_path),
                                    _ => None,
                                }
                            )
                        );
                        return Err(Box::new(e) as Box<dyn std::error::Error>);
                    }
                };
                tracing::info!(
                    "Tauri↔gateway proxy listening on http://127.0.0.1:{}",
                    proxy::PROXY_PORT
                );
                tauri::async_runtime::spawn(proxy::run(listener, transport_for_proxy));
            }

            let app_handle = app.handle().clone();
            let pair_state = shared.clone();
            tauri::async_runtime::spawn(async move {
                if let Some(token) = auto_pair(&pair_state).await
                    && let Some(window) = app_handle.get_webview_window("main")
                {
                    inject_token_into_webview(&window, &token);
                }
            });

            health::spawn_health_poller(app.handle().clone(), shared.clone());

            Ok(())
        })
        .build(tauri::generate_context!())
        .expect("error while building tauri application")
        .run(|_app, event| {
            if let RunEvent::ExitRequested { api, .. } = event {
                api.prevent_exit();
            }
        });
}
