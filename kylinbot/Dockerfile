# syntax=docker/dockerfile:1.7

# ── Stage 0: Frontend build ─────────────────────────────────────
FROM node:22-alpine AS web-builder
WORKDIR /web
COPY web/package.json web/package-lock.json* ./
RUN npm ci --ignore-scripts 2>/dev/null || npm install --ignore-scripts
COPY web/ .
RUN npm run build

# ── Stage 1: Build ────────────────────────────────────────────
FROM rust:1.94-slim@sha256:da9dab7a6b8dd428e71718402e97207bb3e54167d37b5708616050b1e8f60ed6 AS builder

WORKDIR /app
ARG KYLINBOT_CARGO_FEATURES="channel-lark,whatsapp-web"

# Install build dependencies
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

# 1. Copy manifests to cache dependencies
COPY Cargo.toml Cargo.lock ./
# Copy every workspace-member manifest in one glob — adding or removing a crate
# no longer requires editing this file.  --parents preserves the
# crates/<name>/Cargo.toml directory structure.
COPY --parents crates/*/Cargo.toml ./
# apps/tauri: .dockerignore whitelists only Cargo.toml; src and build.rs are stubbed below.
COPY apps/tauri/Cargo.toml apps/tauri/Cargo.toml
# Create dummy targets for all workspace members so manifest parsing succeeds.
RUN mkdir -p src benches apps/tauri/src \
    && echo "fn main() {}" > src/main.rs \
    && echo "" > src/lib.rs \
    && echo "fn main() {}" > benches/agent_benchmarks.rs \
    && echo "fn main() {}" > apps/tauri/src/main.rs \
    && echo "fn main() {}" > apps/tauri/build.rs \
    && for d in crates/*/; do mkdir -p "${d}src" && printf '' > "${d}src/lib.rs"; done
RUN --mount=type=cache,id=kylinbot-cargo-registry,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,id=kylinbot-cargo-git,target=/usr/local/cargo/git,sharing=locked \
    --mount=type=cache,id=kylinbot-target,target=/app/target,sharing=locked \
    if [ -n "$KYLINBOT_CARGO_FEATURES" ]; then \
      cargo build --release --locked --features "$KYLINBOT_CARGO_FEATURES"; \
    else \
      cargo build --release --locked; \
    fi
RUN rm -rf src benches

# 2. Copy only build-relevant source paths (avoid cache-busting on docs/tests/scripts)
COPY src/ src/
COPY benches/ benches/
COPY *.rs .
RUN touch src/main.rs
RUN --mount=type=cache,id=kylinbot-cargo-registry,target=/usr/local/cargo/registry,sharing=locked \
    --mount=type=cache,id=kylinbot-cargo-git,target=/usr/local/cargo/git,sharing=locked \
    --mount=type=cache,id=kylinbot-target,target=/app/target,sharing=locked \
    rm -rf target/release/.fingerprint/kylinbot-* \
           target/release/deps/kylinbot-* \
           target/release/incremental/kylinbot-* && \
    if [ -n "$KYLINBOT_CARGO_FEATURES" ]; then \
      cargo build --release --locked --features "$KYLINBOT_CARGO_FEATURES"; \
    else \
      cargo build --release --locked; \
    fi && \
    cp target/release/kylinbot /app/kylinbot && \
    strip /app/kylinbot
RUN size=$(stat -c%s /app/kylinbot) && \
    if [ "$size" -lt 1000000 ]; then echo "ERROR: binary too small (${size} bytes), likely dummy build artifact" && exit 1; fi

# Prepare runtime directory structure and default config inline (no extra stage)
RUN mkdir -p /kylinbot-data/.kylinbot /kylinbot-data/workspace && \
    printf '%s\n' \
        'workspace_dir = "/kylinbot-data/workspace"' \
        'config_path = "/kylinbot-data/.kylinbot/config.toml"' \
        'api_key = ""' \
        'default_provider = "openrouter"' \
        'default_model = "anthropic/claude-sonnet-4-20250514"' \
        'default_temperature = 0.7' \
        '' \
        '[gateway]' \
        'transport = "tcp"' \
        'port = 42617' \
        'host = "[::]"' \
        'allow_public_bind = true' \
        'require_pairing = false' \
        'web_dist_dir = "/kylinbot-data/web/dist"' \
        '' \
        '[autonomy]' \
        'level = "supervised"' \
        'auto_approve = ["file_read", "file_write", "file_edit", "memory_recall", "memory_store", "web_search_tool", "web_fetch", "calculator", "glob_search", "content_search", "image_info", "weather", "git_operations"]' \
        > /kylinbot-data/.kylinbot/config.toml && \
    chown -R 65534:65534 /kylinbot-data

# ── Stage 2: Development Runtime (Debian) ────────────────────
FROM debian:trixie-slim@sha256:f6e2cfac5cf956ea044b4bd75e6397b4372ad88fe00908045e9a0d21712ae3ba AS dev

# Install essential runtime dependencies only (use docker-compose.override.yml for dev tools)
RUN apt-get update && apt-get install -y \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /kylinbot-data /kylinbot-data
COPY --from=builder /app/kylinbot /usr/local/bin/kylinbot
COPY --from=web-builder /web/dist /kylinbot-data/web/dist

# Overwrite minimal config with DEV template (Ollama defaults)
COPY dev/config.template.toml /kylinbot-data/.kylinbot/config.toml
RUN chown 65534:65534 /kylinbot-data/.kylinbot/config.toml

# Environment setup
# Ensure UTF-8 locale so CJK / multibyte input is handled correctly
ENV LANG=C.UTF-8
# Use consistent workspace path
ENV KYLINBOT_WORKSPACE=/kylinbot-data/workspace
ENV HOME=/kylinbot-data
# Defaults for local dev (Ollama) - matches config.template.toml
ENV PROVIDER="ollama"
ENV KYLINBOT_MODEL="llama3.2"
ENV KYLINBOT_GATEWAY_TRANSPORT=tcp
ENV KYLINBOT_GATEWAY_PORT=42617

# Note: API_KEY is intentionally NOT set here to avoid confusion.
# It is set in config.toml as the Ollama URL.

WORKDIR /kylinbot-data
USER 65534:65534
EXPOSE 42617
# Container images intentionally use TCP. UDS is the Linux desktop default;
# container socket sharing is out of scope for this image.
HEALTHCHECK --interval=60s --timeout=10s --retries=3 --start-period=10s \
    CMD ["kylinbot", "status", "--format=exit-code"]
ENTRYPOINT ["kylinbot"]
CMD ["daemon"]

# ── Stage 3: Production Runtime (Distroless) ─────────────────
FROM gcr.io/distroless/cc-debian13:nonroot@sha256:84fcd3c223b144b0cb6edc5ecc75641819842a9679a3a58fd6294bec47532bf7 AS release

COPY --from=builder /app/kylinbot /usr/local/bin/kylinbot
COPY --from=builder /kylinbot-data /kylinbot-data
COPY --from=web-builder /web/dist /kylinbot-data/web/dist

# Environment setup
# Ensure UTF-8 locale so CJK / multibyte input is handled correctly
ENV LANG=C.UTF-8
ENV KYLINBOT_WORKSPACE=/kylinbot-data/workspace
ENV HOME=/kylinbot-data
# Default provider and model are set in config.toml, not here,
# so config file edits are not silently overridden
#ENV PROVIDER=
ENV KYLINBOT_GATEWAY_TRANSPORT=tcp
ENV KYLINBOT_GATEWAY_PORT=42617

# API_KEY must be provided at runtime!

WORKDIR /kylinbot-data
USER 65534:65534
EXPOSE 42617
# Container images intentionally use TCP. UDS is the Linux desktop default;
# container socket sharing is out of scope for this image.
HEALTHCHECK --interval=60s --timeout=10s --retries=3 --start-period=10s \
    CMD ["kylinbot", "status", "--format=exit-code"]
ENTRYPOINT ["kylinbot"]
CMD ["daemon"]
