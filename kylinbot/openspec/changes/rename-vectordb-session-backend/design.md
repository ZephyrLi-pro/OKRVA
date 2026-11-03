## Context

The `kylinbot-infra` crate contains session-related modules following the naming pattern `session_*.rs`:
- `session_backend.rs` - the trait definition
- `session_sqlite.rs` - SQLite implementation
- `session_store.rs` - session storage utility
- `vector_db_session_backend.rs` - VectorDB implementation (inconsistent)

The `vector_db_session_backend.rs` file breaks this pattern by placing `vector_db` at the front instead of using the `session_backend_*` suffix convention.

## Goals / Non-Goals

**Goals:**
- Rename the file to `session_backend_vectordb.rs` for consistency
- Update all module references and imports across the codebase
- Maintain all existing functionality unchanged

**Non-Goals:**
- No changes to the `VectorDBSessionBackend` type name or public API
- No modifications to the implementation logic
- No changes to tests beyond import paths if needed

## Decisions

### File naming: `session_backend_vectordb.rs`

**Rationale:** This follows the established pattern where:
- The prefix `session_` indicates this is a session-related module
- The suffix `backend` indicates this is a backend implementation
- The `_vectordb` suffix distinguishes this from other backends (sqlite)

**Alternative considered:** `session_backend_vector_db.rs`
- Rejected: Less consistent with `session_sqlite.rs` style which uses no underscores in the variant name

### Module path change: `vector_db_session_backend` → `session_backend_vectordb`

**Rationale:** The module path should match the file name for clarity. Users importing this will use:
```rust
use kylinbot_infra::session_backend_vectordb::VectorDBSessionBackend;
```

## Risks / Trade-offs

**Risk: Breaking change for external consumers** → Mitigation: This is an internal crate; no external consumers known. The type name `VectorDBSessionBackend` remains unchanged.

**Risk: Merge conflicts in ongoing branches** → Mitigation: Simple rename operation, easy to re-resolve if conflicts occur.
