use kylinbot_config::schema::{CloudflareTunnelConfig, Config, GatewayTlsConfig, Transport};

#[test]
fn uds_tls_rejected() {
    let mut config = Config::default();
    config.gateway.transport = Transport::Uds;
    config.gateway.tls = Some(GatewayTlsConfig {
        enabled: true,
        cert_path: "cert.pem".into(),
        key_path: "key.pem".into(),
        client_auth: None,
    });

    let err = config.validate().expect_err("UDS plus TLS must fail");
    assert_eq!(
        err.to_string(),
        "TLS cannot be combined with transport = \"uds\"; either set transport = \"tcp\" or remove the [gateway.tls] block"
    );
}

#[test]
fn uds_tunnel_rejected() {
    let mut config = Config::default();
    config.gateway.transport = Transport::Uds;
    config.tunnel.provider = "cloudflare".into();
    config.tunnel.cloudflare = Some(CloudflareTunnelConfig {
        token: "test-token".into(),
    });

    let err = config.validate().expect_err("UDS plus tunnel must fail");
    assert_eq!(
        err.to_string(),
        "tunnel.provider requires gateway.transport = \"tcp\"; set gateway.transport = \"tcp\" to use tunnels"
    );
}
