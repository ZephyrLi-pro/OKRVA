# Architecture Overview

KylinBot is a layered Rust workspace. At the top is the agent runtime; below it are pluggable providers, channels, tools, and memory; supporting crates handle config and sandboxing.

## High-level shape

```mermaid
flowchart TB
    subgraph External["External world"]
        UI["CLI / chat platforms / gateway clients / ACP IDEs"]
        LLM["LLM providers<br/>Anthropic · OpenAI · Ollama · ..."]
        FS["Filesystem · shell · network"]
    end

    subgraph Edges["Edge crates — talk to the outside"]
        CH["kylinbot-channels<br/>30+ messaging integrations"]
        GW["kylinbot-gateway<br/>REST · WebSocket · dashboard"]
        PR["kylinbot-providers<br/>LLM clients · fallback · routing"]
        TL["kylinbot-tools<br/>browser · HTTP · PDF"]
    end

    subgraph Core["Core"]
        RT["kylinbot-runtime<br/>agent loop · security · SOP · cron · onboarding"]
        MEM["kylinbot-memory<br/>SQLite · embeddings · consolidation"]
        CFG["kylinbot-config<br/>schema · autonomy · secrets"]
    end

    UI --> CH
    UI --> GW
    CH --> RT
    GW --> RT
    RT --> PR
    RT --> TL
    RT --> MEM
    RT --> CFG
    PR --> LLM
    TL --> FS
```

## Crates in scope

| Crate | Role |
|---|---|
| `kylinbot-runtime` | Agent loop, security policy enforcement, SOP engine, cron scheduler, onboarding wizard, TUI |
| `kylinbot-config` | TOML schema, secrets encryption, autonomy levels, workspace resolution |
| `kylinbot-api` | Public traits — `Provider`, `Channel`, `Tool`. The kernel ABI |
| `kylinbot-providers` | All LLM client impls (Anthropic, OpenAI, Ollama, …) plus the router and fallback wrapper |
| `kylinbot-channels` | 30+ messaging integrations (Discord, Slack, Telegram, Matrix, email, voice, …) |
| `kylinbot-gateway` | HTTP / WebSocket gateway, web dashboard, webhook ingress |
| `kylinbot-tools` | Callable tool implementations the agent invokes (browser, HTTP, PDF) |
| `kylinbot-tool-call-parser` | Model-side tool-call syntax parsing and normalisation |
| `kylinbot-memory` | Conversation memory, embeddings, vector retrieval |
| `kylinbot-plugins` | Dynamic plugin loading |
| `kylinbot-infra` | Tracing, metrics, structured logging |
| `kylinbot-macros` | Derive macros for config, tool registration |
| `kylinbot-tui` | Terminal UI |

The microkernel roadmap (RFC #5574) is actively splitting `kylinbot-runtime` further — the kernel layer will shrink to the agent loop and policy enforcement, with everything else moving behind feature flags.

## Request lifecycle (short)

```mermaid
sequenceDiagram
    participant U as User
    participant CH as Channel
    participant RT as Runtime
    participant SEC as Security
    participant PR as Provider
    participant TL as Tool

    U->>CH: message / DM / webhook
    CH->>RT: deliver_message(ctx)
    RT->>PR: chat(messages, tools)
    PR-->>RT: stream: text · tool_call
    RT->>SEC: validate(tool_call)
    SEC-->>RT: approved / blocked
    RT->>TL: invoke(args)
    TL-->>RT: result
    RT->>PR: chat(..., + tool_result)
    PR-->>RT: stream: text (final)
    RT-->>CH: reply (partial / final)
    CH-->>U: message
```

Full detail: [Request lifecycle](./request-lifecycle.md).

## Extension points

Three trait-based extension points live in `kylinbot-api`:

- **`Provider`** — implement for a new LLM endpoint. See [Custom providers](../providers/custom.md).
- **`Channel`** — implement for a new messaging platform. Inbound and outbound are separate hooks.
- **`Tool`** — implement for a new capability the agent can invoke. See [Developing → Plugin protocol](../developing/plugin-protocol.md).

All three are registered at startup via factory functions; the kernel doesn't know the concrete types. Compile-time feature flags decide which implementations ship in a given binary.

## Where to read next

- [Crates](./crates.md) — per-crate deep dive
- [Request lifecycle](./request-lifecycle.md) — streaming, tool calls, approvals
- [Model Providers → Overview](../providers/overview.md)
- [Security → Overview](../security/overview.md)
