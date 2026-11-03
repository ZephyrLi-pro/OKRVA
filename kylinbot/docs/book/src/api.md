# API Reference

Full rustdoc for every public type in the workspace, auto-generated from the `///` comments on each type, function, and module. Use this when you need to know the exact shape of a struct, the methods on a trait, or what a function returns — anything the generated reference exposes better than prose can.

**[Open the rustdoc →](../api/kylinbot/index.html)**

## How to navigate it

- The sidebar on the left lists every crate in the workspace
- Click `kylinbot-api` first — that's where the public traits (`Provider`, `Channel`, `Tool`) live
- Use `cmd/ctrl+F` in the rustdoc page to search within a crate
- Click on any trait to see implementors across the workspace

## Crate index

| Crate | What it exposes |
|---|---|
| [`kylinbot`](../api/kylinbot/index.html) | Top-level umbrella with re-exports |
| [`kylinbot-api`](../api/kylinbot_api/index.html) | Public traits: `Provider`, `Channel`, `Tool`, `StreamEvent` |
| [`kylinbot-config`](../api/kylinbot_config/index.html) | Config schema, autonomy types, secrets |
| [`kylinbot-runtime`](../api/kylinbot_runtime/index.html) | Agent loop, security, SOP, onboarding |
| [`kylinbot-providers`](../api/kylinbot_providers/index.html) | Every LLM-provider implementation |
| [`kylinbot-channels`](../api/kylinbot_channels/index.html) | Messaging integrations |
| [`kylinbot-gateway`](../api/kylinbot_gateway/index.html) | HTTP/WebSocket gateway |
| [`kylinbot-tools`](../api/kylinbot_tools/index.html) | Agent-callable tools |
| [`kylinbot-memory`](../api/kylinbot_memory/index.html) | Conversation memory, embeddings |
| [`kylinbot-plugins`](../api/kylinbot_plugins/index.html) | WASM plugin host |
| [`kylinbot-infra`](../api/kylinbot_infra/index.html) | Tracing, metrics |

See [Architecture → Crates](./architecture/crates.md) for a plain-English description of how the crates fit together.

## Regenerating the API reference

The rustdoc ships with every doc deploy. For local builds:

```bash
cargo mdbook refs     # generates CLI + config reference + rustdoc
cargo mdbook build    # rebuilds the full book including rustdoc bridge
```

See [Maintainers → Docs & Translations](./maintainers/docs-and-translations.md).
