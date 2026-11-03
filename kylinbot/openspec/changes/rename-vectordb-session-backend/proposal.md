## Why

The file `vector_db_session_backend.rs` in `kylinbot-infra` crate uses an inconsistent naming pattern compared to other session-related files. All other session files follow the pattern `session_*.rs` (e.g., `session_backend.rs`, `session_sqlite.rs`, `session_store.rs`), but this file uses `vector_db_session_backend.rs` which breaks the convention. This inconsistency makes the codebase harder to navigate and understand at a glance.

## What Changes

- Rename `vector_db_session_backend.rs` to `session_backend_vectordb.rs` to match the established naming pattern
- Update the module declaration in `lib.rs`
- Update all import statements across the codebase that reference this module

## Capabilities

### New Capabilities

None - this is a pure refactoring change with no new functionality.

### Modified Capabilities

None - this is a naming convention fix with no behavior changes.

## Impact

**Affected files:**
- `crates/kylinbot-infra/src/vector_db_session_backend.rs` → `crates/kylinbot-infra/src/session_backend_vectordb.rs`
- `crates/kylinbot-infra/src/lib.rs` - module declaration
- `crates/kylinbot-gateway/src/lib.rs` - import statement
- `crates/kylinbot-channels/src/orchestrator/acp_server.rs` - import statement
- `crates/kylinbot-runtime/src/agent/loop_.rs` - import statement

**No API changes** - only internal module path changes. The public type `VectorDBSessionBackend` remains the same.
